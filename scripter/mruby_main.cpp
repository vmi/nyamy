//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// mruby_main.cpp
//
// Entry point for nyamy-scripter.exe (mruby variant).
// argv[1] is the script path; argv[2..] are passed as ARGV.
//
// argv is UTF-8 (guaranteed by the UTF-8 activeCodePage manifest).

#include "nyamy_scripter.h"
#include "mruby_binding.h"
#include "ctrl_stream.h"
#include <stdio.h>
#include <windows.h>


// Usage errors are reported here rather than from on_load_setting: that
// callback returning false only discards the setting, it does not end the
// process, so a missing script has to be caught before nys_start().
static const int kUsageExitCode = 2;

static void printUsage(const char *i_argv0)
{
	fprintf(stderr,
		"usage: %s <script> [args...]\n"
		"\n"
		"  script   configuration script (.rb, or .mayu to compile).  A\n"
		"           relative path is searched in NYAMY_CONFIG then NYAMY_ROOT;\n"
		"           the current directory is not searched.\n"
		"  args     passed to the script as ARGV\n"
		"\n"
		"NYAMY_ROOT=%s\n"
		"NYAMY_HOME=%s\n"
		"NYAMY_CONFIG=%s\n",
		i_argv0, nys_paths_root(), nys_paths_home(), nys_paths_config());
}


int main(int argc, char *argv[])
{
	if (argc < 2) {
		printUsage(0 < argc ? argv[0] : "nyamy-scripter");
		return kUsageExitCode;
	}

	MRubyContext ctx = { argc, (const char* const*)argv, nullptr };

	NYsCallbacks callbacks = {};
	callbacks.on_load_setting = mruby_on_load_setting;
	callbacks.on_quit         = mruby_on_quit;

	// Terminate rather than hang when a script does not return after Quit.
	// nyamy waits longer than this before killing us itself, so a script that
	// runs away is normally cleaned up here.
	nys_set_quit_timeout(kScripterQuitTimeoutMillisec);

	return nys_start(&callbacks, &ctx);
}
