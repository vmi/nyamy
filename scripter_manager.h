//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// scripter_manager.h
//
// Manages the lifecycle and communication with the nyamy-scripter subprocess.
// Spawns the process, sends control commands (CtrlStream), and receives
// compiled CmdStream, delivering the resulting Setting asynchronously.


#ifndef _SCRIPTER_MANAGER_H
#  define _SCRIPTER_MANAGER_H


#  include "ctrl_stream_writer.h"
#  include "adhoc_keyseq.h"
#  include "ctrl_stream.h"     // FuncArg, TriggerInfo
#  include "pipe_streambuf.h"
#  include "symbols.h"
#  include "setting.h"
#  include "multithread.h"
#  include <atomic>
#  include <future>
#  include <functional>
#  include <memory>
#  include <mutex>
#  include <ostream>
#  include <streambuf>
#  include <windows.h>


class ScripterManager
{
public:
	/// i_hwndNotify: Window to notify Setting completion (mayu's tasktray window)
	ScripterManager(SyncObject *i_soLog, std::wostream *i_log, HWND i_hwndNotify);
	~ScripterManager();

	/// Start (or restart) the scripter process asynchronously.
	/// Sends CtrlId::Start with config name, path, and symbols after the process is ready.
	/// If a previous start is still in progress, returns false and does nothing.
	bool start(const wstringi &configName, const wstringi &configPath,
	           const Symbols &syms, LogLevel logLevel);

	/// Tell the scripter the new log threshold (non-blocking; ignored if the
	/// scripter is not running yet - the next start() carries it instead).
	void setLogLevel(LogLevel logLevel);

	/// signal quit to scripter process (non-blocking; idempotent)
	void sendQuit();
	/// End the reads the reader threads are parked in, so that they return
	/// whatever the scripter is doing (non-blocking; idempotent).
	void stopReaders();
	/// fill buf with active wait handles (process + reader threads); returns count
	DWORD collectHandles(HANDLE *buf, DWORD maxCount);
	/// close and null out all process/thread/pipe handles (call after WaitForMultipleObjects)
	void closeHandles();

	/// Stop the reader threads and wait for them and the scripter process to
	/// finish.  i_graceMillisec is how long the scripter is given to exit on
	/// its own; pass kScripterQuitGraceMillisec unless the caller has already
	/// waited.  If it is still running after that it is terminated, so that it
	/// does not outlive nyamy.
	void forceStop(DWORD i_graceMillisec);

	/// Wait for a pending asynchronous start()/restart to finish, so that the
	/// pipe and thread handles stop changing under the caller.
	void waitForPendingStart();

	/// ExecKeySeq callback type (called from background thread with the materialized item)
	using ExecKeySeqCallback = std::function<void(AdHocKeySeq)>;
	/// Register callback for ExecKeySeq commands received from scripter
	void setExecKeySeqCallback(ExecKeySeqCallback cb);

	/// Send ExecUserFunc to scripter (called from Engine callback)
	void execUserFunc(const wstringi &name,
	                  const std::vector<FuncArg> &args,
	                  const TriggerInfo &ctx);

	/// Notification message ID.  Posted with wParam == 0 and lParam == 0; the
	/// Setting itself is picked up with takePendingSetting().
	static constexpr UINT WM_ScripterSettingReady = WM_APP + 120;

	// Single-slot handoff storage for a freshly built Setting.  The slot is
	// static because the producing thread may outlive the ScripterManager
	// object; there is at most one ScripterManager per process, so one slot is
	// enough.

	/// Store a Setting into the slot, releasing whatever it held before, so a
	/// notification that is never dispatched cannot leak a Setting.
	static void setPendingSetting(std::shared_ptr<Setting> i_setting);

	/// Move the pending Setting out of the slot.  Returns an empty pointer when
	/// the slot is empty (already taken, or superseded by a later Setting).
	static std::shared_ptr<Setting> takePendingSetting();

	/// Release anything still held in the slot.
	static void clearPendingSetting();

private:
	// pipe handles
	HANDLE m_hCtrlWrite;        ///< nyamy -> scripter (CtrlStream, non-stdio)
	HANDLE m_hDataRead;         ///< scripter -> nyamy (CmdStream, non-stdio)
	HANDLE m_hMsgRead;          ///< scripter stdout+stderr -> nyamy (log text, merged)
	HANDLE m_hScripterProcess;

	// background threads (data + msg) and the event that ends their reads
	HANDLE m_hDataThread;
	HANDLE m_hMsgThread;
	HANDLE m_hReaderStop;

	// ostream and its streambuf to write to ctrl pipe.
	// Guarded by m_ctrlMutex: sendQuit() destroys them on the UI thread while
	// execUserFunc() uses them on the engine thread.
	std::mutex m_ctrlMutex;
	std::unique_ptr<PipeWriteStreambuf> m_ctrlStreambuf;
	std::unique_ptr<std::ostream>    m_ctrlStream;
	std::unique_ptr<CtrlStreamWriter> m_ctrlWriter;

	bool m_quitSent;

	SyncObject *m_soLog;
	std::wostream   *m_log;
	HWND        m_hwndNotify;

	ExecKeySeqCallback m_execKeySeqCallback;

	// async start/restart
	std::future<bool> m_startFuture;
	bool launchScripter(const wstringi &configName, const wstringi &configPath,
	                    const Symbols &syms, LogLevel logLevel);
	/// most recent threshold, so a restart can carry it
	std::atomic<LogLevel> m_logLevel;

	// background thread entry points
	static unsigned __stdcall dataThread(void *param);
	static unsigned __stdcall msgThread(void *param);
	void runReader();
	void runMsgReader();
};


#endif // !_SCRIPTER_MANAGER_H
