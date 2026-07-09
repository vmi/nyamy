//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// setting_dump.h
//
// Canonical, order-independent textual dump of a Setting, used to compare
// the result of loading a .mayu file vs. its .mayu.rb equivalent.

#ifndef _SETTING_DUMP_H
#  define _SETTING_DUMP_H

#  include <string>

class Setting;

/// Produce a deterministic textual representation of a Setting.
/// Collections are sorted and key sequences are inlined (via the existing
/// stream-output operators), so anonymous keyseq names / indices that differ
/// between the two compile pipelines do not affect the result.
std::wstring dumpSetting(const Setting &i_setting);

#endif // !_SETTING_DUMP_H
