//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// scripter_manager.cpp


#include "misc.h"

#include "scripter_manager.h"
#include "cmd_processor.h"
#include "cmd_stream_reader.h"
#include "stringtool.h"

#include <process.h>


const UINT ScripterManager::WM_ScripterSettingReady = WM_APP + 120;


//=============================================================================
// PipeWriteStreambuf - wraps a Win32 write pipe HANDLE as a std::streambuf
//=============================================================================

class PipeWriteStreambuf : public std::streambuf
{
public:
	explicit PipeWriteStreambuf(HANDLE h) : m_h(h) {}

protected:
	int_type overflow(int_type c) override {
		if (c == EOF) return EOF;
		char ch = static_cast<char>(c);
		DWORD written;
		return WriteFile(m_h, &ch, 1, &written, NULL) ? c : EOF;
	}

	std::streamsize xsputn(const char *s, std::streamsize n) override {
		DWORD written;
		if (!WriteFile(m_h, s, static_cast<DWORD>(n), &written, NULL))
			return 0;
		return static_cast<std::streamsize>(written);
	}

private:
	HANDLE m_h;
};


//=============================================================================
// PipeReadStreambuf - wraps a Win32 read pipe HANDLE as a std::streambuf
//=============================================================================

class PipeReadStreambuf : public std::streambuf
{
public:
	explicit PipeReadStreambuf(HANDLE h) : m_h(h) {}

protected:
	int_type underflow() override {
		DWORD got = 0;
		if (!ReadFile(m_h, &m_ch, 1, &got, NULL) || got == 0)
			return traits_type::eof();
		setg(&m_ch, &m_ch, &m_ch + 1);
		return traits_type::to_int_type(m_ch);
	}

private:
	HANDLE m_h;
	char m_ch = 0;
};


//=============================================================================
// PipeReadWStreambuf - wraps a Win32 read pipe HANDLE as a std::wstreambuf
// Reads UTF-16 wchar_t units from a pipe whose write end uses _O_U16TEXT mode
//=============================================================================

class PipeReadWStreambuf : public std::wstreambuf
{
public:
	explicit PipeReadWStreambuf(HANDLE h) : m_h(h) {}

protected:
	int_type underflow() override {
		DWORD got = 0;
		if (!ReadFile(m_h, &m_ch, sizeof(wchar_t), &got, NULL) || got < sizeof(wchar_t))
			return traits_type::eof();
		setg(&m_ch, &m_ch, &m_ch + 1);
		return traits_type::to_int_type(m_ch);
	}

private:
	HANDLE m_h;
	wchar_t m_ch = 0;
};


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
	, m_hStderrRead(INVALID_HANDLE_VALUE)
	, m_hScripterProcess(NULL)
	, m_hDataThread(NULL)
	, m_hStderrThread(NULL)
{
}


ScripterManager::~ScripterManager()
{
	// send quit request to scripter
	if (m_ctrlWriter) {
		try { m_ctrlWriter->writeQuit(); } catch (...) {}
	}
	m_ctrlWriter.reset();
	m_ctrlStream.reset();
	m_ctrlStreambuf.reset();

	// close ctrl pipe (scripter's stdin EOF -> end loop)
	if (m_hCtrlWrite != INVALID_HANDLE_VALUE) {
		CloseHandle(m_hCtrlWrite);
		m_hCtrlWrite = INVALID_HANDLE_VALUE;
	}

	// wait for scripter process to exit
	if (m_hScripterProcess) {
		WaitForSingleObject(m_hScripterProcess, 5000);
		CloseHandle(m_hScripterProcess);
		m_hScripterProcess = NULL;
	}

	// wait for data / stderr threads to exit
	HANDLE threads[2] = {};
	DWORD nThreads = 0;
	if (m_hDataThread)   threads[nThreads++] = m_hDataThread;
	if (m_hStderrThread) threads[nThreads++] = m_hStderrThread;
	if (nThreads > 0)
		WaitForMultipleObjects(nThreads, threads, TRUE, 5000);
	if (m_hDataThread)   { CloseHandle(m_hDataThread);   m_hDataThread   = NULL; }
	if (m_hStderrThread) { CloseHandle(m_hStderrThread); m_hStderrThread = NULL; }

	if (m_hDataRead   != INVALID_HANDLE_VALUE) { CloseHandle(m_hDataRead);   m_hDataRead   = INVALID_HANDLE_VALUE; }
	if (m_hStderrRead != INVALID_HANDLE_VALUE) { CloseHandle(m_hStderrRead); m_hStderrRead = INVALID_HANDLE_VALUE; }
}


bool ScripterManager::start()
{
	// ctrl pipe:   yamy (write) -> scripter stdin (read)
	HANDLE hCtrlRead    = INVALID_HANDLE_VALUE;
	// data pipe:   scripter stdout (write) -> yamy (read)
	HANDLE hDataWrite   = INVALID_HANDLE_VALUE;
	// stderr pipe: scripter stderr (write) -> yamy (read)
	HANDLE hStderrWrite = INVALID_HANDLE_VALUE;

	SECURITY_ATTRIBUTES sa = {};
	sa.nLength        = sizeof(sa);
	sa.bInheritHandle = TRUE;

	if (!CreatePipe(&hCtrlRead,      &m_hCtrlWrite, &sa, 0) ||
	    !CreatePipe(&m_hDataRead,    &hDataWrite,   &sa, 0) ||
	    !CreatePipe(&m_hStderrRead,  &hStderrWrite, &sa, 0)) {
		if (m_log) {
			Acquire a(m_soLog, 0);
			*m_log << L"ScripterManager: CreatePipe failed\n";
		}
		return false;
	}

	// do not inherit yamy-side handles to child process
	SetHandleInformation(m_hCtrlWrite,  HANDLE_FLAG_INHERIT, 0);
	SetHandleInformation(m_hDataRead,   HANDLE_FLAG_INHERIT, 0);
	SetHandleInformation(m_hStderrRead, HANDLE_FLAG_INHERIT, 0);

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

	STARTUPINFO si = {};
	si.cb         = sizeof(si);
	si.dwFlags    = STARTF_USESTDHANDLES;
	si.hStdInput  = hCtrlRead;
	si.hStdOutput = hDataWrite;
	si.hStdError  = hStderrWrite;

	PROCESS_INFORMATION pi = {};
	BOOL result = CreateProcess(scripterPath.c_str(), NULL, NULL, NULL, TRUE,
	                            CREATE_NO_WINDOW, NULL, NULL, &si, &pi);

	// handles passed to child process are no longer needed on parent side
	CloseHandle(hCtrlRead);
	CloseHandle(hDataWrite);
	CloseHandle(hStderrWrite);

	if (!result) {
		if (m_log) {
			Acquire a(m_soLog, 0);
			*m_log << L"ScripterManager: failed to start " << scripterPath
			       << L" (error " << GetLastError() << L")\n";
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
	m_hDataThread   = (HANDLE)_beginthreadex(NULL, 0, dataThread,   this, 0, &tid);
	m_hStderrThread = (HANDLE)_beginthreadex(NULL, 0, stderrThread, this, 0, &tid);

	if (m_log) {
		Acquire a(m_soLog, 0);
		*m_log << L"ScripterManager: started " << scripterPath << L"\n";
	}
	return true;
}


void ScripterManager::reload(const Symbols &syms)
{
	if (!m_ctrlWriter) return;
	try {
		m_ctrlWriter->writeReload(syms);
	} catch (...) {
		if (m_log) {
			Acquire a(m_soLog, 0);
			*m_log << L"ScripterManager: writeReload failed\n";
		}
	}
}


std::unique_ptr<Setting> ScripterManager::takeNewSetting()
{
	std::lock_guard<std::mutex> lk(m_mutex);
	return std::move(m_pendingSetting);
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
	processor.onCommit([this](std::unique_ptr<Setting> s) {
		{
			std::lock_guard<std::mutex> lk(m_mutex);
			m_pendingSetting = std::move(s);
		}
		PostMessage(m_hwndNotify, WM_ScripterSettingReady, 0, 0);
	});

	processor.process(reader);
}


//-----------------------------------------------------------------------------
// Background thread: stderr (log text from scripter)
//-----------------------------------------------------------------------------

unsigned __stdcall ScripterManager::stderrThread(void *param)
{
	static_cast<ScripterManager *>(param)->runStderrReader();
	return 0;
}


void ScripterManager::runStderrReader()
{
	PipeReadWStreambuf wsb(m_hStderrRead);
	std::wistream ws(&wsb);
	std::wstring line;

	while (std::getline(ws, line)) {
		if (!line.empty() && line.back() == L'\r') line.pop_back();
		if (m_log) {
			Acquire a(m_soLog, 0);
			*m_log << L"[scripter] " << line << L"\n";
		}
	}
}
