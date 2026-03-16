//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// scripter_engine.cpp - yamy-scripter entry point
//
// Reads CtrlStream commands from an inherited pipe handle passed via YSCR_CTRL env var.
// Compiles .mayu files and writes CmdStream to an inherited pipe handle passed
// via YSCR_CMD env var.  stdout and stderr are text log channels (UTF-16 on stderr).


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
#include <iostream>


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
		std::wcerr << L"error: could not find config file." << std::endl;
		return;
	}
	for (const auto &s : regSymbols) symbols.insert(s);
	std::wcerr << L"  loading: " << path << std::endl;

	MayuParser parser;
	auto ast = parser.parseFile(path, cf);
	if (parser.hasErrors()) {
		for (const auto &msg : parser.getMessages())
			std::wcerr << msg << std::endl;
		return;
	}

	MayuCompiler compiler(writer, symbols, cf, nullptr, &std::wcerr);
	compiler.compile(*ast);
	if (compiler.hasErrors()) {
		std::wcerr << L"error: compile failed." << std::endl;
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

	if (hCtrlRead == INVALID_HANDLE_VALUE || hDataWrite == INVALID_HANDLE_VALUE) {
		// Set stderr to UTF-16 before writing the error message
		_setmode(_fileno(stderr), _O_U16TEXT);
		std::wcerr << L"error: YSCR_CTRL and YSCR_CMD environment variables are required" << std::endl;
		return;
	}

	// stdout/stderr are now text log channels (not used for binary protocol).
	// Set stderr to UTF-16 so the parent's PipeReadWStreambuf can read wide chars.
	_setmode(_fileno(stderr), _O_U16TEXT);

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
