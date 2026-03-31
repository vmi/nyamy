//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ys_types.h - scripter-internal implementation types
//
// YsFuncArg and YsFuncArgs/YsStrs struct definitions shared between
// yamy_scripter.cpp and ctrl_stream_reader.cpp.

#ifndef _YS_TYPES_H
#  define _YS_TYPES_H

#  include <string>
#  include <vector>
#  include "yamy_scripter.h"


struct YsStrs {
	std::vector<std::string> strs;  // UTF-8 strings
};


struct YsFuncArg {
	YsType      type    = YsType_Number;
	std::string str;                    // String / Regexp (UTF-8 primary data)
	YsStrs*     strs    = nullptr;      // TokenSeq (non-owning; lifetime managed by SessionAllocator)
	int64_t     numval  = 0;            // Number (int32) / KeySeqIdx (uint32) / ModifierSpec.modifiers (uint64)
	int64_t     numval2 = 0;            // ModifierSpec.dontcares (uint64)
	YsFuncArg() = default;
	// non-copyable; allow move
	YsFuncArg(const YsFuncArg&) = delete;
	YsFuncArg& operator=(const YsFuncArg&) = delete;
	YsFuncArg(YsFuncArg&&) = default;
	YsFuncArg& operator=(YsFuncArg&&) = default;
};


struct YsFuncArgs {
	std::vector<YsFuncArg> entries;
};


#endif // !_YS_TYPES_H
