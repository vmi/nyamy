//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// mruby_main.cpp
//
// Entry point for nyamy-scripter.exe (mruby variant).
// argv[1] is the .rb script path; argv[2..] are passed as ARGV.
// When invoked with no arguments, searches home directories for .mayu.rb.
//
// argv is UTF-8 (guaranteed by the UTF-8 activeCodePage manifest).

#include "nyamy_scripter.h"
#include "mruby_binding.h"
#include "ctrl_stream.h"
#include <windows.h>


int main(int argc, char *argv[])
{
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
