//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// scripter_manager.h
//
// Manages the lifecycle and communication with the yamy-scripter subprocess.
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
#  include <future>
#  include <functional>
#  include <memory>
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
	           const Symbols &syms);

	/// signal quit to scripter process (non-blocking; idempotent)
	void sendQuit();
	/// fill buf with active wait handles (process + reader threads); returns count
	DWORD collectHandles(HANDLE *buf, DWORD maxCount);
	/// close and null out all process/thread/pipe handles (call after WaitForMultipleObjects)
	void closeHandles();

	/// ExecKeySeq callback type (called from background thread with the materialized item)
	using ExecKeySeqCallback = std::function<void(AdHocKeySeq)>;
	/// Register callback for ExecKeySeq commands received from scripter
	void setExecKeySeqCallback(ExecKeySeqCallback cb);

	/// Send ExecUserFunc to scripter (called from Engine callback)
	void execUserFunc(const wstringi &name,
	                  const std::vector<FuncArg> &args,
	                  const TriggerInfo &ctx);

	/// notification message ID
	static const UINT WM_ScripterSettingReady;

private:
	// pipe handles
	HANDLE m_hCtrlWrite;        ///< yamy -> scripter (CtrlStream, non-stdio)
	HANDLE m_hDataRead;         ///< scripter -> yamy (CmdStream, non-stdio)
	HANDLE m_hMsgRead;          ///< scripter stdout+stderr -> yamy (log text, merged)
	HANDLE m_hScripterProcess;

	// background threads (data + msg)
	HANDLE m_hDataThread;
	HANDLE m_hMsgThread;

	// ostream and its streambuf to write to ctrl pipe
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
	                    const Symbols &syms);

	// background thread entry points
	static unsigned __stdcall dataThread(void *param);
	static unsigned __stdcall msgThread(void *param);
	void runReader();
	void runMsgReader();
};


#endif // !_SCRIPTER_MANAGER_H
