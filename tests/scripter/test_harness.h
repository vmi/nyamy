//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// test_harness.h
//
// Drives the nyamy-scripter pipeline in-process: runs nys_start() with the
// mruby callbacks on a worker thread, feeds it a Start command (with the
// given symbol set) over an anonymous pipe, and consumes the resulting
// CmdStream with CmdProcessor to build a Setting.

#ifndef _TEST_HARNESS_H
#  define _TEST_HARNESS_H

#  include "symbols.h"
#  include "scripter_types.h"
#  include "trigger_info.h"
#  include <memory>
#  include <string>
#  include <vector>

class Setting;

/// One &ExecUserFunc request to send once the setting has been loaded.
struct ExecUserFuncRequest {
	wstringi             name;
	std::vector<FuncArg> args;
	TriggerInfo          context;
};

/// Build a Setting by running the mruby script at i_scriptPathUtf8 with the
/// given symbol set.  Returns nullptr if the script failed to produce a Commit
/// (e.g. a Ruby error) within the timeout.
/// An empty i_scriptPathUtf8 runs the no-argument form, which probes the
/// home directories for a default ".mayu.rb" script.
/// i_loadCount > 1 sends that many Start commands over the same pipe and
/// returns the Setting of the last Commit, exercising the reload path.
/// i_execs are sent behind the Start commands, so the script has been loaded
/// and its `deffunc' handlers registered by the time they run.  Whatever the
/// handlers write goes to the scripter's log (stderr), not to the Setting.
std::shared_ptr<Setting> buildSetting(const std::string &i_scriptPathUtf8,
                                      const Symbols &i_symbols,
                                      int i_loadCount = 1,
                                      const std::vector<ExecUserFuncRequest>
                                          *i_execs = nullptr);

#endif // !_TEST_HARNESS_H
