//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// nys_types.h - scripter-internal implementation types
//
// NYsFuncArg and NYsFuncArgs/NYsStrs struct definitions shared between
// nyamy_scripter.cpp and ctrl_stream_reader.cpp.

#ifndef _YS_TYPES_H
#  define _YS_TYPES_H

#  include <string>
#  include <vector>
#  include "nyamy_scripter.h"


struct NYsStrs {
	std::vector<std::string> strs;  // UTF-8 strings
};


struct NYsFuncArg {
	NYsType      type    = NYsType_Number;
	std::string str;                    // String / Regexp (UTF-8 primary data)
	NYsStrs*     strs    = nullptr;      // TokenSeq (non-owning; lifetime managed by SessionAllocator)
	int64_t     numval  = 0;            // Number (int32) / KeySeqIdx (uint32) / ModifierSpec.modifiers (uint64)
	int64_t     numval2 = 0;            // ModifierSpec.dontcares (uint64)
	NYsFuncArg() = default;
	// non-copyable; allow move
	NYsFuncArg(const NYsFuncArg&) = delete;
	NYsFuncArg& operator=(const NYsFuncArg&) = delete;
	NYsFuncArg(NYsFuncArg&&) = default;
	NYsFuncArg& operator=(NYsFuncArg&&) = default;
};


struct NYsFuncArgs {
	std::vector<NYsFuncArg> entries;
};


#endif // !_YS_TYPES_H
