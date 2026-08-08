//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// scripter_manager.cpp


#include "misc.h"

#include "scripter_manager.h"
#include "cmd_processor.h"
#include "cmd_stream_reader.h"
#include "pipe_streambuf.h"
#include "stringtool.h"
#include "inifile.h"
#include "mayu.h"

#include <process.h>
#include <atomic>
#include <random>
#include <vector>


//=============================================================================
// pending Setting slot
//=============================================================================

// Single-slot handoff from the scripter data thread (producer) to the tasktray
// window procedure (consumer).  WM_ScripterSettingReady carries no payload: the
// Setting travels through this slot, so a notification that is posted but never
// dispatched (WM_QUIT, destroyed window, shutdown race) cannot leak it, and a
// stale HWND can no longer deliver a raw pointer to a foreign window procedure.

namespace
{

std::atomic<std::shared_ptr<Setting> > &pendingSettingSlot()
{
	static std::atomic<std::shared_ptr<Setting> > slot;
	return slot;
}

} // namespace


void ScripterManager::setPendingSetting(std::shared_ptr<Setting> i_setting)
{
	// The superseded Setting, if any, is released as the returned temporary
	// goes out of scope.
	pendingSettingSlot().exchange(std::move(i_setting));
}


std::shared_ptr<Setting> ScripterManager::takePendingSetting()
{
	return pendingSettingSlot().exchange(nullptr);
}


void ScripterManager::clearPendingSetting()
{
	takePendingSetting();
}


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
	, m_hReaderStop(CreateEvent(NULL, TRUE, FALSE, NULL))
	, m_quitSent(false)
{
}


ScripterManager::~ScripterManager()
{
	// Mayu normally stops the reader threads itself and leaves nothing to do
	// here; this is the fallback for any other owner.
	sendQuit();
	waitForPendingStart();
	forceStop(kScripterQuitGraceMillisec);
	closeHandles();

	// Release a Setting that was handed over but never dispatched, rather than
	// keeping it alive until process exit.
	clearPendingSetting();

	if (m_hReaderStop)
		CloseHandle(m_hReaderStop);
}

void ScripterManager::sendQuit()
{
	if (m_quitSent) return;
	m_quitSent = true;

	{
		std::lock_guard<std::mutex> lock(m_ctrlMutex);
		if (m_ctrlWriter) {
			// Quit is written once, on the shutdown path, over a pipe left in
			// drop-on-full mode - so make this one message wait for room
			// instead of vanishing.  The scripter's ctrl thread drains
			// unconditionally, so any backlog clears in well under the bound.
			if (m_ctrlStreambuf)
				m_ctrlStreambuf->setRetryOnce(200);
			try {
				m_ctrlWriter->writeQuit();
				m_ctrlStream->flush();
			} catch (...) {}
			if (m_ctrlStreambuf && m_ctrlStreambuf->wasBlocked()) {
				m_ctrlStreambuf->clearBlocked();
				if (m_log) {
					Acquire a(m_soLog, 0);
					*m_log << L"ScripterManager: ctrl pipe full; Quit not "
					          L"delivered (closing the pipe still signals it)"
					       << std::endl;
				}
			}
		}
		m_ctrlWriter.reset();
		m_ctrlStream.reset();
		m_ctrlStreambuf.reset();
	}

	// Closing the write end is the signal that always gets through: the
	// scripter sees EOF on its ctrl pipe.  CtrlId::Quit above is the early
	// notice, not the guarantee.
	if (m_hCtrlWrite != INVALID_HANDLE_VALUE) {
		CloseHandle(m_hCtrlWrite);
		m_hCtrlWrite = INVALID_HANDLE_VALUE;
	}
}


void ScripterManager::stopReaders()
{
	if (m_hReaderStop)
		SetEvent(m_hReaderStop);
}


void ScripterManager::forceStop(DWORD i_graceMillisec)
{
	stopReaders();

	HANDLE h[3];
	DWORD n = collectHandles(h, 3);
	if (n == 0)
		return;
	if (WaitForMultipleObjects(n, h, TRUE, i_graceMillisec) != WAIT_TIMEOUT)
		return;

	// The scripter neither exited on its own nor terminated itself, so it is a
	// foreign implementation, one wedged before its ctrl thread started, or one
	// held alive by an error dialog.  Kill it rather than let it outlive nyamy.
	if (m_hScripterProcess) {
		if (m_log) {
			Acquire a(m_soLog, 0);
			*m_log << L"ScripterManager: scripter did not exit; terminating."
			       << std::endl;
		}
		TerminateProcess(m_hScripterProcess, 1);
	}

	n = collectHandles(h, 3);
	if (n > 0)
		WaitForMultipleObjects(n, h, TRUE, kScripterKillWaitMillisec);
}


void ScripterManager::waitForPendingStart()
{
	if (m_startFuture.valid())
		m_startFuture.wait();
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


bool ScripterManager::start(const wstringi &configName, const wstringi &configPath,
                             const Symbols &syms)
{
	// If a previous async start is still running, skip
	if (m_startFuture.valid() &&
	    m_startFuture.wait_for(std::chrono::seconds(0)) == std::future_status::timeout)
		return false;

	m_startFuture = std::async(std::launch::async,
		[this, configName, configPath, syms]() {
			return launchScripter(configName, configPath, syms);
		});
	return true;
}


// Expand ${VAR} placeholders in s.
// ${NYAMY_HOME} -> nyamyHome; others -> GetEnvironmentVariableW().
// Unknown vars are left as-is and appended to *unknownVars if provided.
// After each expansion, if the result ends with '\' and the next input char is also '\',
// one backslash is consumed to prevent double separators.
static std::wstring expandVars(const std::wstring &s, const std::wstring &nyamyHome,
                               std::vector<std::wstring> *unknownVars = nullptr)
{
	std::wstring result;
	result.reserve(s.size());
	for (size_t i = 0; i < s.size(); ) {
		if (s[i] == L'$' && i + 1 < s.size() && s[i + 1] == L'{') {
			size_t end = s.find(L'}', i + 2);
			if (end == std::wstring::npos) { result += s[i++]; continue; }
			std::wstring name = s.substr(i + 2, end - i - 2);
			if (name == L"NYAMY_HOME") {
				result += nyamyHome;
			} else {
				wchar_t buf[2048];
				DWORD len = GetEnvironmentVariableW(name.c_str(), buf, 2048);
				if (len > 0 && len < 2048) {
					result.append(buf, len);
				} else {
					result += s.substr(i, end - i + 1);
					if (unknownVars) unknownVars->push_back(name);
				}
			}
			i = end + 1;
			// Prevent double backslash when expansion ends with '\' and next char is also '\'.
			if (!result.empty() && result.back() == L'\\' && i < s.size() && s[i] == L'\\')
				++i;
		} else {
			result += s[i++];
		}
	}
	return result;
}


// Pipe buffer sizes.  CreatePipe() used the 4 KB default; the ctrl size is
// stated explicitly because it is the threshold at which ExecUserFunc is
// dropped rather than allowed to stall the engine thread.
static const DWORD kCtrlPipeBufSize = 16 * 1024;
static const DWORD kDataPipeBufSize = 64 * 1024;
static const DWORD kMsgPipeBufSize  = 16 * 1024;


static void closePipeHandle(HANDLE *io_h)
{
	if (*io_h != INVALID_HANDLE_VALUE) {
		CloseHandle(*io_h);
		*io_h = INVALID_HANDLE_VALUE;
	}
}


// Create one pipe as a named pipe: nyamy owns the server end, the child
// inherits an ordinary synchronous client handle.  Named rather than anonymous
// because only a named pipe handle can be opened for overlapped I/O, which is
// what lets a parked reader thread be stopped without killing the scripter.
// The ends nyamy reads are therefore overlapped; the ctrl end it writes is not,
// since PIPE_NOWAIT already keeps that write off the engine thread's back.
// Nothing changes on the child's side, so a foreign scripter launched through
// the ini "cmdLine" setting is unaffected.
//
// Nothing else can connect: the name is unguessable, the create fails outright
// if it is taken (FILE_FLAG_FIRST_PIPE_INSTANCE), only one instance exists, it
// is consumed immediately below, and remote clients are rejected.
static bool createNamedPipePair(bool i_serverReads, DWORD i_bufSize,
                                HANDLE *o_hServer, HANDLE *o_hClient)
{
	*o_hServer = INVALID_HANDLE_VALUE;
	*o_hClient = INVALID_HANDLE_VALUE;

	DWORD sessionId = 0;
	ProcessIdToSessionId(GetCurrentProcessId(), &sessionId);
	std::random_device rd;	// cryptographically strong on Windows

	SECURITY_ATTRIBUTES saInherit = {};
	saInherit.nLength        = sizeof(saInherit);
	saInherit.bInheritHandle = TRUE;

	for (int attempt = 0; attempt < 4; ++attempt) {
		wchar_t name[128];
		swprintf_s(name, L"\\\\.\\pipe\\GANAware\\nyamy\\%u-%u-%08x%08x",
		           sessionId, GetCurrentProcessId(), rd(), rd());

		// the server end is left non-inheritable (no SECURITY_ATTRIBUTES)
		HANDLE hServer = CreateNamedPipe(
			name,
			(i_serverReads ? PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED
			               : PIPE_ACCESS_OUTBOUND) |
				FILE_FLAG_FIRST_PIPE_INSTANCE,
			PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT |
				PIPE_REJECT_REMOTE_CLIENTS,
			1, i_bufSize, i_bufSize, 0, NULL);
		if (hServer == INVALID_HANDLE_VALUE)
			continue;	// name already taken: try another one

		// Opening the client end here fills the pipe's only instance, and
		// leaves it connected - which is why no ConnectNamedPipe() is needed.
		HANDLE hClient = CreateFile(
			name, i_serverReads ? GENERIC_WRITE : GENERIC_READ, 0,
			&saInherit, OPEN_EXISTING, 0, NULL);
		if (hClient == INVALID_HANDLE_VALUE) {
			CloseHandle(hServer);
			return false;
		}

		*o_hServer = hServer;
		*o_hClient = hClient;
		return true;
	}
	return false;
}


bool ScripterManager::launchScripter(const wstringi &configName,
                                      const wstringi &configPath,
                                      const Symbols &syms)
{
	// Stop existing scripter if running.  This runs on the async start task,
	// which ~ScripterManager waits for, so an unbounded wait here would hang
	// the UI thread whenever the old scripter is stuck.
	if (m_hScripterProcess != NULL) {
		sendQuit();
		forceStop(kScripterQuitGraceMillisec);
		closeHandles();
		m_quitSent = false;
	}

	// ctrl pipe:  nyamy (write) -> scripter (read) via inherited handle in NYS_CTRL env var
	HANDLE hCtrlRead  = INVALID_HANDLE_VALUE;
	// data pipe:  scripter (write) -> nyamy (read) via inherited handle in NYS_CMD env var
	HANDLE hDataWrite = INVALID_HANDLE_VALUE;
	// msg pipe:   scripter stdout+stderr (write) -> nyamy (read), merged
	HANDLE hMsgWrite  = INVALID_HANDLE_VALUE;
	// NUL device: passed as STARTUPINFO.hStdInput below, but deliberately not
	// inheritable, which leaves the child without a usable stdin.  That is the
	// intent: control travels over its own pipe (NYS_CTRL), so there is nothing
	// for stdin to carry and nothing should arrive through it.
	HANDLE hNul       = INVALID_HANDLE_VALUE;

	hNul = CreateFile(L"NUL", GENERIC_READ,
	                  FILE_SHARE_READ | FILE_SHARE_WRITE,
	                  NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if (hNul == INVALID_HANDLE_VALUE ||
	    !createNamedPipePair(false, kCtrlPipeBufSize, &m_hCtrlWrite, &hCtrlRead) ||
	    !createNamedPipePair(true,  kDataPipeBufSize, &m_hDataRead,  &hDataWrite) ||
	    !createNamedPipePair(true,  kMsgPipeBufSize,  &m_hMsgRead,   &hMsgWrite)) {
		if (m_log) {
			Acquire a(m_soLog, 0);
			*m_log << L"ScripterManager: cannot create pipes" << std::endl;
		}
		closePipeHandle(&hNul);
		closePipeHandle(&hCtrlRead);
		closePipeHandle(&hDataWrite);
		closePipeHandle(&hMsgWrite);
		closePipeHandle(&m_hCtrlWrite);
		closePipeHandle(&m_hDataRead);
		closePipeHandle(&m_hMsgRead);
		return false;
	}

	// determine nyamy's home directory (used for ${NYAMY_HOME} and the default
	// scripter path)
	wchar_t exePath[GANA_MAX_PATH];
	wchar_t exeDrive[GANA_MAX_PATH];
	wchar_t exeDir[GANA_MAX_PATH];
	GetModuleFileName(NULL, exePath, GANA_MAX_PATH);
	_wsplitpath_s(exePath, exeDrive, GANA_MAX_PATH, exeDir, GANA_MAX_PATH,
	              NULL, 0, NULL, 0);
	wstringi nyamyHome = exeDrive;
	nyamyHome += exeDir;

	// The ini value "cmdLine", if present, is the FULL command line
	// (executable plus arguments) used to launch the scripter, so that any
	// program speaking the scripter protocol can be substituted.  When absent,
	// fall back to nyamy-scripter.exe next to nyamy.exe.
	wstringi iniCmdLine;
	{
		IniFile ini;
		ini.read(L"cmdLine", &iniCmdLine);
	}

	std::wstring cmdLineStr;
	if (!iniCmdLine.empty()) {
		std::vector<std::wstring> unknownVars;
		cmdLineStr = expandVars(iniCmdLine, nyamyHome, &unknownVars);
		for (const auto &uv : unknownVars) {
			if (m_log) {
				Acquire a(m_soLog, 0);
				*m_log << L"warning: cmdLine: unknown variable: ${" << uv << L"}" << std::endl;
			}
		}
	} else {
		cmdLineStr = L"\"" + std::wstring((nyamyHome + L"nyamy-scripter.exe").c_str()) + L"\"";
	}

	// build an environment block that includes NYS_CTRL and NYS_CMD
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
		addVar(L"NYS_CTRL", ctrlVal);
		addVar(L"NYS_CMD",  cmdVal);

		// append current process environment
		wchar_t *cur = GetEnvironmentStringsW();
		if (cur) {
			for (const wchar_t *p = cur; *p; ) {
				const wchar_t *entry = p;
				while (*p) ++p;
				++p;  // skip NUL
				// skip any existing NYS_CTRL/NYS_CMD entries
				wchar_t nysCtrlEq[] = L"NYS_CTRL=";
				wchar_t nysCmdEq[] = L"NYS_CMD=";
				bool skip = (wcsncmp(entry, nysCtrlEq, sizeof(nysCtrlEq) - 1) == 0 ||
				             wcsncmp(entry, nysCmdEq, sizeof(nysCmdEq) - 1) == 0);
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
	// CreateProcess may modify the command line buffer; pass a copy so that
	// cmdLineStr stays intact for logging
	std::wstring cmdLineBuf = cmdLineStr;
	BOOL result = CreateProcess(NULL, cmdLineBuf.data(), NULL, NULL, TRUE,
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
			*m_log << L"ScripterManager: failed to start " << cmdLineStr
			       << L" (error " << lastErr << L")" << std::endl;
		}
		return false;
	}

	CloseHandle(pi.hThread);
	m_hScripterProcess = pi.hProcess;

	{
		std::lock_guard<std::mutex> lock(m_ctrlMutex);

		// construct ctrl write stream
		m_ctrlStreambuf = std::make_unique<PipeWriteStreambuf>(m_hCtrlWrite);
		m_ctrlStream   = std::make_unique<std::ostream>(m_ctrlStreambuf.get());
		m_ctrlWriter   = std::make_unique<CtrlStreamWriter>(*m_ctrlStream);

		// start background threads.  A restart above left the stop event set.
		ResetEvent(m_hReaderStop);
		unsigned tid;
		m_hDataThread = (HANDLE)_beginthreadex(NULL, 0, dataThread, this, 0, &tid);
		m_hMsgThread  = (HANDLE)_beginthreadex(NULL, 0, msgThread,  this, 0, &tid);

		if (m_log) {
			Acquire a(m_soLog, 0);
			*m_log << L"ScripterManager: started " << cmdLineStr << std::endl;
		}

		// Send CtrlId::Start with config name, path, and symbols.
		// This runs while the pipe is still blocking, so the (possibly large)
		// Start message is delivered reliably before we switch to drop-on-full.
		if (m_ctrlWriter) {
			try { m_ctrlWriter->writeStart(configName, configPath, syms); } catch (...) {}
		}

		// From now on, ctrl writes (ExecUserFunc from the engine thread) must not
		// block if the scripter is busy: drop and log instead of stalling input.
		if (m_ctrlStreambuf)
			m_ctrlStreambuf->setNonBlocking();
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
	// Called on the engine's keyboard handler thread.  The ctrl pipe is in
	// non-blocking mode (see launchScripter), so if the scripter is busy and
	// the pipe buffer is full this write is dropped rather than stalling all
	// key processing.  Report the drop instead.
	// The lock is what keeps the stream from being destroyed under us:
	// sendQuit() resets it from the UI thread.
	std::lock_guard<std::mutex> lock(m_ctrlMutex);
	if (m_ctrlWriter) {
		try { m_ctrlWriter->writeExecUserFunc(name, args, ctx); } catch (...) {}
	}
	if (m_ctrlStreambuf && m_ctrlStreambuf->wasBlocked()) {
		m_ctrlStreambuf->clearBlocked();
		if (m_log) {
			Acquire a(m_soLog, 0);
			*m_log << L"ScripterManager: ctrl pipe full; discarded "
			          L"ExecUserFunc(" << name << L")" << std::endl;
		}
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
	PipeReadStreambuf rsb(m_hDataRead, m_hReaderStop);
	std::istream pipeStream(&rsb);
	CmdStreamReader reader(pipeStream);

	CmdProcessor processor(m_soLog, m_log);
	processor.onCommit([this](std::shared_ptr<Setting> s) {
		// Hand the Setting over through the static single slot and notify with
		// an empty payload.  If the notification is lost, or superseded by a
		// later commit, the Setting is released by the next store or by
		// clearPendingSetting() at shutdown - it is never leaked.  A failed
		// post needs no cleanup for the same reason, so the result is
		// intentionally ignored.
		setPendingSetting(std::move(s));
		PostMessage(m_hwndNotify, WM_ScripterSettingReady, 0, 0);
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
	PipeReadStreambuf rsb(m_hMsgRead, m_hReaderStop);
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
