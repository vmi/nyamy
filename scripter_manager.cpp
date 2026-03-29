//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// scripter_manager.cpp


#include "misc.h"

#include "scripter_manager.h"
#include "cmd_processor.h"
#include "cmd_stream_reader.h"
#include "pipe_streambuf.h"
#include "stringtool.h"

#include <process.h>
#include <vector>


const UINT ScripterManager::WM_ScripterSettingReady = WM_APP + 120;


//=============================================================================
// ScripterManager
//=============================================================================

ScripterManager::ScripterManager(SyncObject *i_soLog, std::wostream *i_log,
                                 HWND i_hwndNotify)
	: m_soLog(i_soLog)
	, m_log(i_log)
	, m_hwndNotify(i_hwndNotify)
	, m_hCtrlWrite(INVALID_HANDLE_VALUE)
	, m_hDataRead(INVALID_HANDLE_VALUE)
	, m_hMsgRead(INVALID_HANDLE_VALUE)
	, m_hScripterProcess(NULL)
	, m_hDataThread(NULL)
	, m_hMsgThread(NULL)
	, m_quitSent(false)
{
}


ScripterManager::~ScripterManager()
{
	sendQuit();

	// wait for any async start/restart task to finish
	if (m_startFuture.valid())
		m_startFuture.wait();

	// wait for any handles not yet closed by an external WaitForMultipleObjects
	HANDLE handles[3];
	DWORD n = collectHandles(handles, 3);
	if (n > 0)
		WaitForMultipleObjects(n, handles, TRUE, 2000);

	closeHandles();
}

void ScripterManager::sendQuit()
{
	if (m_quitSent) return;
	m_quitSent = true;

	if (m_ctrlWriter) {
		try { m_ctrlWriter->writeQuit(); } catch (...) {}
	}
	m_ctrlWriter.reset();
	m_ctrlStream.reset();
	m_ctrlStreambuf.reset();

	// closing stdin causes scripter to see EOF and exit its loop
	if (m_hCtrlWrite != INVALID_HANDLE_VALUE) {
		CloseHandle(m_hCtrlWrite);
		m_hCtrlWrite = INVALID_HANDLE_VALUE;
	}
}

DWORD ScripterManager::collectHandles(HANDLE *buf, DWORD maxCount)
{
	DWORD n = 0;
	if (m_hScripterProcess && n < maxCount) buf[n++] = m_hScripterProcess;
	if (m_hDataThread      && n < maxCount) buf[n++] = m_hDataThread;
	if (m_hMsgThread       && n < maxCount) buf[n++] = m_hMsgThread;
	return n;
}

void ScripterManager::closeHandles()
{
	if (m_hScripterProcess) { CloseHandle(m_hScripterProcess); m_hScripterProcess = NULL; }
	if (m_hDataThread)      { CloseHandle(m_hDataThread);      m_hDataThread      = NULL; }
	if (m_hMsgThread)       { CloseHandle(m_hMsgThread);       m_hMsgThread       = NULL; }
	if (m_hDataRead != INVALID_HANDLE_VALUE) { CloseHandle(m_hDataRead); m_hDataRead = INVALID_HANDLE_VALUE; }
	if (m_hMsgRead  != INVALID_HANDLE_VALUE) { CloseHandle(m_hMsgRead);  m_hMsgRead  = INVALID_HANDLE_VALUE; }
}


bool ScripterManager::start(const Symbols &syms)
{
	// If a previous async start is still running, skip
	if (m_startFuture.valid() &&
	    m_startFuture.wait_for(std::chrono::seconds(0)) == std::future_status::timeout)
		return false;

	m_startFuture = std::async(std::launch::async,
	                           &ScripterManager::launchScripter, this, syms);
	return true;
}


bool ScripterManager::launchScripter(const Symbols &syms)
{
	// Stop existing scripter if running
	if (m_hScripterProcess != NULL) {
		sendQuit();
		HANDLE h[3];
		DWORD n = collectHandles(h, 3);
		if (n > 0) WaitForMultipleObjects(n, h, TRUE, INFINITE);
		closeHandles();
		m_quitSent = false;
	}

	// ctrl pipe:  yamy (write) -> scripter (read) via inherited handle in YSCR_CTRL env var
	HANDLE hCtrlRead  = INVALID_HANDLE_VALUE;
	// data pipe:  scripter (write) -> yamy (read) via inherited handle in YSCR_CMD env var
	HANDLE hDataWrite = INVALID_HANDLE_VALUE;
	// msg pipe:   scripter stdout+stderr (write) -> yamy (read), merged
	HANDLE hMsgWrite  = INVALID_HANDLE_VALUE;
	// NUL device: used as scripter's stdin (reads return EOF immediately)
	HANDLE hNul       = INVALID_HANDLE_VALUE;

	SECURITY_ATTRIBUTES sa = {};
	sa.nLength        = sizeof(sa);
	sa.bInheritHandle = TRUE;

	hNul = CreateFile(L"NUL", GENERIC_READ,
	                  FILE_SHARE_READ | FILE_SHARE_WRITE,
	                  &sa, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if (hNul == INVALID_HANDLE_VALUE ||
	    !CreatePipe(&hCtrlRead,   &m_hCtrlWrite, &sa, 0) ||
	    !CreatePipe(&m_hDataRead, &hDataWrite,   &sa, 0) ||
	    !CreatePipe(&m_hMsgRead,  &hMsgWrite,    &sa, 0)) {
		if (m_log) {
			Acquire a(m_soLog, 0);
			*m_log << L"ScripterManager: CreatePipe failed" << std::endl;
		}
		if (hNul      != INVALID_HANDLE_VALUE) CloseHandle(hNul);
		if (hCtrlRead != INVALID_HANDLE_VALUE) CloseHandle(hCtrlRead);
		if (hDataWrite!= INVALID_HANDLE_VALUE) CloseHandle(hDataWrite);
		if (hMsgWrite != INVALID_HANDLE_VALUE) CloseHandle(hMsgWrite);
		return false;
	}

	// yamy-side handles must not be inherited by the child
	SetHandleInformation(m_hCtrlWrite, HANDLE_FLAG_INHERIT, 0);
	SetHandleInformation(m_hDataRead,  HANDLE_FLAG_INHERIT, 0);
	SetHandleInformation(m_hMsgRead,   HANDLE_FLAG_INHERIT, 0);
	SetHandleInformation(hNul,         HANDLE_FLAG_INHERIT, 0);
	// hCtrlRead, hDataWrite, hMsgWrite are inherited (sa.bInheritHandle=TRUE)

	// determine scripter's executable path
	wchar_t exePath[GANA_MAX_PATH];
	wchar_t exeDrive[GANA_MAX_PATH];
	wchar_t exeDir[GANA_MAX_PATH];
	GetModuleFileName(NULL, exePath, GANA_MAX_PATH);
	_wsplitpath_s(exePath, exeDrive, GANA_MAX_PATH, exeDir, GANA_MAX_PATH,
	              NULL, 0, NULL, 0);
	wstringi scripterPath = exeDrive;
	scripterPath += exeDir;
	scripterPath += L"yamy-scripter.exe";

	// build command line
	wchar_t cmdLine[1024];
	swprintf_s(cmdLine, L"\"%s\"", scripterPath.c_str());

	// build an environment block that includes YSCR_CTRL and YSCR_CMD
	// so the child receives pipe handle values without polluting the parent environment
	wchar_t ctrlVal[32], cmdVal[32];
	swprintf_s(ctrlVal, L"%llu",
	           static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(hCtrlRead)));
	swprintf_s(cmdVal, L"%llu",
	           static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(hDataWrite)));

	std::vector<wchar_t> envBlock;
	{
		auto addVar = [&](const wchar_t *name, const wchar_t *value) {
			for (const wchar_t *p = name;  *p; ++p) envBlock.push_back(*p);
			envBlock.push_back(L'=');
			for (const wchar_t *p = value; *p; ++p) envBlock.push_back(*p);
			envBlock.push_back(L'\0');
		};
		addVar(L"YSCR_CTRL", ctrlVal);
		addVar(L"YSCR_CMD",  cmdVal);

		// append current process environment
		wchar_t *cur = GetEnvironmentStringsW();
		if (cur) {
			for (const wchar_t *p = cur; *p; ) {
				const wchar_t *entry = p;
				while (*p) ++p;
				++p;  // skip NUL
				// skip any existing YSCR_CTRL/YSCR_CMD entries
				bool skip = (wcsncmp(entry, L"YSCR_CTRL=", 10) == 0 ||
				             wcsncmp(entry, L"YSCR_CMD=",   9) == 0);
				if (!skip) {
					for (const wchar_t *q = entry; q < p; ++q) envBlock.push_back(*q);
				}
			}
			FreeEnvironmentStringsW(cur);
		}
		envBlock.push_back(L'\0');  // final double-NUL terminator
	}

	STARTUPINFO si = {};
	si.cb         = sizeof(si);
	si.dwFlags    = STARTF_USESTDHANDLES;
	si.hStdInput  = hNul;       // stdin = NUL (EOF on read)
	si.hStdOutput = hMsgWrite;  // stdout = message log
	si.hStdError  = hMsgWrite;  // stderr = same pipe (merged)

	PROCESS_INFORMATION pi = {};
	BOOL result = CreateProcess(NULL, cmdLine, NULL, NULL, TRUE,
	                            CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT,
	                            envBlock.data(), NULL, &si, &pi);
	DWORD lastErr = GetLastError();

	// close handles that were passed to / used by the child
	CloseHandle(hCtrlRead);
	CloseHandle(hDataWrite);
	CloseHandle(hMsgWrite);
	CloseHandle(hNul);

	if (!result) {
		if (m_log) {
			Acquire a(m_soLog, 0);
			*m_log << L"ScripterManager: failed to start " << scripterPath
			       << L" (error " << lastErr << L")" << std::endl;
		}
		return false;
	}

	CloseHandle(pi.hThread);
	m_hScripterProcess = pi.hProcess;

	// construct ctrl write stream
	m_ctrlStreambuf = std::make_unique<PipeWriteStreambuf>(m_hCtrlWrite);
	m_ctrlStream   = std::make_unique<std::ostream>(m_ctrlStreambuf.get());
	m_ctrlWriter   = std::make_unique<CtrlStreamWriter>(*m_ctrlStream);

	// start background threads
	unsigned tid;
	m_hDataThread = (HANDLE)_beginthreadex(NULL, 0, dataThread, this, 0, &tid);
	m_hMsgThread  = (HANDLE)_beginthreadex(NULL, 0, msgThread,  this, 0, &tid);

	if (m_log) {
		Acquire a(m_soLog, 0);
		*m_log << L"ScripterManager: started " << scripterPath << std::endl;
	}

	// Send CtrlId::Start with the requested symbols
	if (m_ctrlWriter) {
		try { m_ctrlWriter->writeStart(syms); } catch (...) {}
	}
	return true;
}


void ScripterManager::setExecKeySeqCallback(ExecKeySeqCallback cb)
{
	m_execKeySeqCallback = std::move(cb);
}


void ScripterManager::execUserFunc(const wstringi &name,
                                   const std::vector<FuncArg> &args,
                                   const TriggerInfo &ctx)
{
	if (m_ctrlWriter) {
		try { m_ctrlWriter->writeExecUserFunc(name, args, ctx); } catch (...) {}
	}
}


//-----------------------------------------------------------------------------
// Background thread: data (CmdStream from scripter stdout)
//-----------------------------------------------------------------------------

unsigned __stdcall ScripterManager::dataThread(void *param)
{
	static_cast<ScripterManager *>(param)->runReader();
	return 0;
}


void ScripterManager::runReader()
{
	PipeReadStreambuf rsb(m_hDataRead);
	std::istream pipeStream(&rsb);
	CmdStreamReader reader(pipeStream);

	CmdProcessor processor(m_soLog, m_log);
	processor.onCommit([this](std::shared_ptr<Setting> s) {
		auto *p = new std::shared_ptr<Setting>(std::move(s));
		if (!PostMessage(m_hwndNotify, WM_ScripterSettingReady, 0,
		                 reinterpret_cast<LPARAM>(p)))
			delete p;
	});
	processor.onExecKeySeq([this](AdHocKeySeq item) {
		if (m_execKeySeqCallback) m_execKeySeqCallback(std::move(item));
	});

	processor.process(reader);
}


//-----------------------------------------------------------------------------
// Background thread: msg (log text from scripter stdout+stderr, merged)
//-----------------------------------------------------------------------------

unsigned __stdcall ScripterManager::msgThread(void *param)
{
	static_cast<ScripterManager *>(param)->runMsgReader();
	return 0;
}


void ScripterManager::runMsgReader()
{
	PipeReadStreambuf rsb(m_hMsgRead);
	std::istream     is(&rsb);
	std::string      line;

	while (std::getline(is, line)) {
		if (!line.empty() && line.back() == '\r') line.pop_back();
		std::wstring wline = from_UTF8(line);
		if (m_log) {
			Acquire a(m_soLog, 0);
			*m_log << L"[scripter] " << wline << std::endl;
		}
	}
}
