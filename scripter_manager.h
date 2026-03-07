//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// scripter_manager.h
//
// Manages the lifecycle and communication with the yamy-scripter subprocess.
// Spawns the process, sends control commands (CtrlStream), and receives
// compiled CmdStream, delivering the resulting Setting asynchronously.


#ifndef _SCRIPTER_MANAGER_H
#  define _SCRIPTER_MANAGER_H


#  include "ctrl_stream_writer.h"
#  include "symbols.h"
#  include "setting.h"
#  include "multithread.h"
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

	/// start scripter process and establish pipes
	bool start();

	/// signal quit to scripter process (non-blocking; idempotent)
	void sendQuit();
	/// fill buf with active wait handles (process + reader threads); returns count
	DWORD collectHandles(HANDLE *buf, DWORD maxCount);
	/// close and null out all process/thread/pipe handles (call after WaitForMultipleObjects)
	void closeHandles();

	/// send re-compile request to scripter (asynchronous; notifies WM_ScripterSettingReady on completion)
	void reload(const Symbols &syms);

	/// call from WM_ScripterSettingReady handler: take received Setting
	std::unique_ptr<Setting> takeNewSetting();

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
	std::unique_ptr<std::streambuf>  m_ctrlStreambuf;
	std::unique_ptr<std::ostream>    m_ctrlStream;
	std::unique_ptr<CtrlStreamWriter> m_ctrlWriter;

	// received setting (shared between threads)
	std::mutex m_mutex;
	std::unique_ptr<Setting> m_pendingSetting;

	bool m_quitSent;

	SyncObject *m_soLog;
	std::wostream   *m_log;
	HWND        m_hwndNotify;

	// background thread entry points
	static unsigned __stdcall dataThread(void *param);
	static unsigned __stdcall msgThread(void *param);
	void runReader();
	void runMsgReader();
};


#endif // !_SCRIPTER_MANAGER_H
