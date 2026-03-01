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
// StderrLog - thin SyncObject + tostream adaptor that writes to std::wcerr
//-----------------------------------------------------------------------------

// ConfigFiles/MayuCompiler accept (SyncObject*, tostream*) = (nullptr, nullptr)
// which silently discards log output.  We use nullptr here and collect
// structured error info via hasErrors() / getMessages() instead.
// Stderr output is done directly in doReload() below.


//-----------------------------------------------------------------------------
// doReload - compile .mayu files and write CmdStream to stdout
//-----------------------------------------------------------------------------

static void doReload(const Symbols &syms, CmdStreamWriter &writer)
{
	ConfigFiles cf;  // no log: errors surface through getMessages() / hasErrors()

	tstringi path;
	Symbols regSymbols;
	Symbols symbols = syms;

	if (!cf.getFilename(_T(""), &path, &regSymbols)) {
		std::wcerr << L"error: could not find config file." << std::endl;
		return;
	}
	for (const auto &s : regSymbols) symbols.insert(s);
	std::wcerr << _T("  loading: ") << path << std::endl;

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
// _tmain
//-----------------------------------------------------------------------------

int _tmain(int /*argc*/, _TCHAR * /*argv*/[])
{
	// Set stdin/stdout to binary mode (CmdStream is binary)
	_setmode(_fileno(stdin),  _O_BINARY);
	_setmode(_fileno(stdout), _O_BINARY);
	// stderr remains in text mode (the yamy stderr thread reads it line by line)

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
