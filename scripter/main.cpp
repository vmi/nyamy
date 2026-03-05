//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// main.cpp - yamy-scripter entry point
//
// Reads CtrlStream commands from stdin (binary), compiles .mayu files,
// and writes CmdStream to stdout (binary).
// Errors and warnings are written to stderr (text, one message per line).


#include "misc.h"

#include "ctrl_stream_reader.h"
#include "cmd_stream_writer.h"
#include "config_files.h"
#include "mayu_parser.h"
#include "mayu_compiler.h"

#include <fcntl.h>
#include <io.h>
#include <iostream>


//-----------------------------------------------------------------------------
// StderrLog - thin SyncObject + std::wostream adaptor that writes to std::wcerr
//-----------------------------------------------------------------------------

// ConfigFiles/MayuCompiler accept (SyncObject*, std::wostream*) = (nullptr, nullptr)
// which silently discards log output.  We use nullptr here and collect
// structured error info via hasErrors() / getMessages() instead.
// Stderr output is done directly in doReload() below.


//-----------------------------------------------------------------------------
// doReload - compile .mayu files and write CmdStream to stdout
//-----------------------------------------------------------------------------

static void doReload(const Symbols &syms, CmdStreamWriter &writer)
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


//-----------------------------------------------------------------------------
// wmain
//-----------------------------------------------------------------------------

int wmain(int /*argc*/, wchar_t * /*argv*/[])
{
	// Set stdin/stdout to binary mode (CmdStream is binary)
	_setmode(_fileno(stdin),  _O_BINARY);
	_setmode(_fileno(stdout), _O_BINARY);
	// Set stderr to UTF-16 so that wcerr output is read as wide chars by the engine
	_setmode(_fileno(stderr), _O_U16TEXT);

	CtrlStreamReader ctrlReader(std::cin);
	CmdStreamWriter  dataWriter(std::cout);

	for (;;) {
		CtrlId id;
		if (!ctrlReader.readNext(id))
			break;  // stdin closed -> exit

		if (id == CtrlId::Quit) {
			break;
		} else if (id == CtrlId::Reload) {
			Symbols syms = ctrlReader.readReload();
			doReload(syms, dataWriter);
			std::cout.flush();
		}
	}

	return 0;
}
