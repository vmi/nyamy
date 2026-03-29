//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// scripter_engine.cpp - yamy-scripter entry point
//
// Reads CtrlStream commands from an inherited pipe handle passed via YSCR_CTRL env var.
// Compiles .mayu files and writes CmdStream to an inherited pipe handle passed
// via YSCR_CMD env var.  stdout and stderr are binary log channels (UTF-8, one message per line).


#include "misc.h"

#include "ctrl_stream_reader.h"
#include "cmd_stream_writer.h"
#include "config_files.h"
#include "mayu_parser.h"
#include "mayu_compiler.h"

#define _YAMY_SCRIPTER_IMPL
#include "yamy_scripter.h"

#include "pipe_streambuf.h"

#include <fcntl.h>
#include <io.h>


//-----------------------------------------------------------------------------
// logLine - write a log line to stderr as UTF-8 (binary mode, always appends newline)
//-----------------------------------------------------------------------------

static void logLine(const std::wstring &msg)
{
	std::string utf8 = to_UTF8(msg);
	utf8 += '\n';
	fwrite(utf8.c_str(), 1, utf8.size(), stderr);
}


//-----------------------------------------------------------------------------
// Utf8LineWStreambuf - wstreambuf that routes wide output through logLine
//-----------------------------------------------------------------------------

class Utf8LineWStreambuf : public std::wstreambuf
{
public:
	~Utf8LineWStreambuf() { flush(); }
	void flush() {
		if (!m_buf.empty()) { logLine(m_buf); m_buf.clear(); }
	}
protected:
	int_type overflow(int_type c) override {
		if (c == traits_type::eof()) { flush(); return traits_type::eof(); }
		wchar_t wc = static_cast<wchar_t>(c);
		if (wc == L'\n') { logLine(m_buf); m_buf.clear(); }
		else              m_buf += wc;
		return c;
	}
	std::streamsize xsputn(const wchar_t *s, std::streamsize n) override {
		for (std::streamsize i = 0; i < n; ++i) overflow(s[i]);
		return n;
	}
private:
	std::wstring m_buf;
};


//-----------------------------------------------------------------------------
// Note: ConfigFiles/MayuCompiler accept (SyncObject*, std::wostream*) = (nullptr, nullptr)
// which silently discards log output.  We use nullptr here and collect
// structured error info via hasErrors() / getMessages() instead.
// Stderr output is done directly in doReload() below.


//-----------------------------------------------------------------------------
// execUserFunc - stub for scripter-side user function dispatch
//-----------------------------------------------------------------------------

static void execUserFunc(const wstringi &name,
                         const std::vector<FuncArg> &args,
                         const TriggerInfo &context)
{
	// stub: scripter-side user function dispatch (to be implemented)
	(void)name; (void)args; (void)context;
}


//-----------------------------------------------------------------------------
// doReload - compile .mayu files and write CmdStream to stdout
//-----------------------------------------------------------------------------

static void doCompile(const Symbols &syms, CmdStreamWriter &writer)
{
	ConfigFiles cf;  // no log: errors surface through getMessages() / hasErrors()

	wstringi path;
	Symbols regSymbols;
	Symbols symbols = syms;

	if (!cf.getFilename(L"", &path, &regSymbols)) {
		logLine(L"error: could not find config file.");
		return;
	}
	for (const auto &s : regSymbols) symbols.insert(s);
	logLine(L"  loading: " + std::wstring(path));

	MayuParser parser;
	auto ast = parser.parseFile(path, cf);
	if (parser.hasErrors()) {
		for (const auto &msg : parser.getMessages())
			logLine(msg);
		return;
	}

	Utf8LineWStreambuf utf8WBuf;
	std::wostream      utf8WStream(&utf8WBuf);
	MayuCompiler compiler(writer, symbols, cf, nullptr, &utf8WStream);
	compiler.compile(*ast);
	if (compiler.hasErrors()) {
		logLine(L"error: compile failed.");
		return;
	}

	writer.writeCommit();
}


SCRIPTER_API void scripter_engine(int argc, wchar_t *argv[])
{
	// Read YSCR_CTRL (CtrlStream read handle) and YSCR_CMD (CmdStream write handle)
	// from environment variables.  Both handles are inherited from the parent process.
	HANDLE hCtrlRead  = INVALID_HANDLE_VALUE;
	HANDLE hDataWrite = INVALID_HANDLE_VALUE;
	{
		wchar_t buf[32];
		if (GetEnvironmentVariableW(L"YSCR_CTRL", buf, 32) > 0)
			hCtrlRead  = reinterpret_cast<HANDLE>(
			                 static_cast<uintptr_t>(wcstoull(buf, nullptr, 10)));
		if (GetEnvironmentVariableW(L"YSCR_CMD",  buf, 32) > 0)
			hDataWrite = reinterpret_cast<HANDLE>(
			                 static_cast<uintptr_t>(wcstoull(buf, nullptr, 10)));
	}

	// Set stdout and stderr to binary mode: UTF-8 encoded, one message per line.
	_setmode(_fileno(stdout), _O_BINARY);
	_setmode(_fileno(stderr), _O_BINARY);

	if (hCtrlRead == INVALID_HANDLE_VALUE || hDataWrite == INVALID_HANDLE_VALUE) {
		logLine(L"error: YSCR_CTRL and YSCR_CMD environment variables are required");
		return;
	}

	// Wrap inherited pipe handles in streambufs
	PipeReadStreambuf  ctrlBuf(hCtrlRead);
	PipeWriteStreambuf dataBuf(hDataWrite);
	std::istream ctrlStream(&ctrlBuf);
	std::ostream dataStream(&dataBuf);

	CtrlStreamReader ctrlReader(ctrlStream);
	CmdStreamWriter  dataWriter(dataStream);

	for (;;) {
		CtrlId id;
		if (!ctrlReader.readNext(id))
			break;  // ctrl pipe closed -> exit

		if (id == CtrlId::Quit) {
			break;
		} else if (id == CtrlId::Start) {
			Symbols syms = ctrlReader.readStart();
			doCompile(syms, dataWriter);
			dataStream.flush();
		} else if (id == CtrlId::ExecUserFunc) {
			auto req = ctrlReader.readExecUserFunc();
			execUserFunc(req.name, req.args, req.context);
		}
	}

	CloseHandle(hCtrlRead);
	CloseHandle(hDataWrite);
}
