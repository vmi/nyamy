//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// test_harness.h
//
// Drives the yamy-scripter pipeline in-process: runs ys_start() with the
// mruby callbacks on a worker thread, feeds it a Start command (with the
// given symbol set) over an anonymous pipe, and consumes the resulting
// CmdStream with CmdProcessor to build a Setting.

#ifndef _TEST_HARNESS_H
#  define _TEST_HARNESS_H

#  include "symbols.h"
#  include <memory>
#  include <string>

class Setting;

/// Build a Setting by running the mruby script at i_scriptPathUtf8 with the
/// given symbol set.  Returns nullptr if the script failed to produce a Commit
/// (e.g. a Ruby error) within the timeout.
std::shared_ptr<Setting> buildSetting(const std::string &i_scriptPathUtf8,
                                      const Symbols &i_symbols);

#endif // !_TEST_HARNESS_H
