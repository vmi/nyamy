//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// mruby_main.cpp
//
// Entry point for nyamy-scripter.exe (mruby variant).
// The command line is [options] <script> [args...]; see cli_options.h.
// Arguments after <script> are passed to the script as ARGV.
//
// argv is UTF-8 (guaranteed by the UTF-8 activeCodePage manifest).

#include "nyamy_scripter.h"
#include "mruby_binding.h"
#include "cli_options.h"
#include "ctrl_stream.h"
#include <stdio.h>
#include <vector>
#include <windows.h>


// Usage errors are reported here rather than from on_load_setting: that
// callback returning false only discards the setting, it does not end the
// process, so a missing script has to be caught before nys_start().
static const int kUsageExitCode = 2;


int main(int argc, char *argv[])
{
	const char *argv0 = 0 < argc ? argv[0] : "nyamy-scripter";

	CliOptions options;
	std::string error;
	if (!parseCliOptions(argc, argv, &options, &error)) {
		fprintf(stderr, "%s: %s\n\n", argv0, error.c_str());
		printCliUsage(argv0);
		return kUsageExitCode;
	}
	if (options.showVersion) {
		printCliVersion();
		return 0;
	}
	if (options.showHelp) {
		printCliUsage(argv0);
		return 0;
	}
	if (options.scriptArgIndex == 0) {
		printCliUsage(argv0);
		return kUsageExitCode;
	}

	// -D means the same as a symbol carried by the Start command, and has to be
	// in place before the first load; nys_add_default_symbol re-applies it to
	// every Start, so it survives reloads too.
	for (const auto &symbol : options.symbols)
		nys_add_default_symbol(symbol.c_str());

	// A NULL-terminated view of the -I directories.  options owns the strings
	// and outlives nys_start(), which reads them again on every reload.
	std::vector<const char *> includeDirs;
	for (const auto &dir : options.includeDirs)
		includeDirs.push_back(dir.c_str());
	includeDirs.push_back(nullptr);

	MRubyContext ctx = {};
	ctx.argc           = argc;
	ctx.argv           = (const char *const *)argv;
	ctx.mrb            = nullptr;
	ctx.scriptArgIndex = options.scriptArgIndex;
	ctx.includeDirs    = includeDirs.data();

	NYsCallbacks callbacks = {};
	callbacks.on_load_setting = mruby_on_load_setting;
	callbacks.on_quit         = mruby_on_quit;

	// Terminate rather than hang when a script does not return after Quit.
	// nyamy waits longer than this before killing us itself, so a script that
	// runs away is normally cleaned up here.
	nys_set_quit_timeout(kScripterQuitTimeoutMillisec);

	return nys_start(&callbacks, &ctx);
}
