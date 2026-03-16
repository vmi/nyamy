//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// func_arg.h
//
// FuncArg: unified function argument type (std::variant).
// Used in both the CmdStream bytecode pipeline and the CtrlStream IPC pipeline.

#ifndef _FUNC_ARG_H
#  define _FUNC_ARG_H

#  include "stringtool.h"
#  include <cstdint>
#  include <variant>
#  include <vector>


/// Modifier state (bitfield, resolved from string form at compile time)
struct CmdModifier {
	uint64_t modifiers;
	uint64_t dontcares;

	CmdModifier() : modifiers(0), dontcares(0) {}
};


/// Per-type structs for FuncArg variant
using FuncArgString = wstringq;
using FuncArgNumber = int32_t;
using FuncArgRegexp = wregex_stored;
using FuncArgKeySeqIdx = uint32_t;
using FuncArgModSeq = CmdModifier;
using FuncArgTokenSeq = std::vector<wstringi>;


/// Unified function argument type
using FuncArg = std::variant<
	FuncArgString,      // 0
	FuncArgNumber,      // 1
	FuncArgRegexp,      // 2
	FuncArgKeySeqIdx,   // 3
	FuncArgModSeq,      // 4
	FuncArgTokenSeq     // 5
>;


/// Serialization tag (matches variant index)
enum FuncArgTag : uint8_t {
	FuncArgTag_String    = 0,
	FuncArgTag_Number    = 1,
	FuncArgTag_Regexp    = 2,
	FuncArgTag_KeySeqIdx = 3,
	FuncArgTag_ModSeq    = 4,
	FuncArgTag_TokenSeq  = 5,
};


/// overloaded helper for std::visit
template<class... Ts>
struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;


/// Accessor helpers (for use in generated loadFromCmd code)
inline const wstringq&              getFuncArgString(const FuncArg& a) { return std::get<FuncArgString>(a); }
inline int32_t                      getFuncArgNumber(const FuncArg& a) { return std::get<FuncArgNumber>(a); }
inline const wregex_stored&         getFuncArgRegexp(const FuncArg& a) { return std::get<FuncArgRegexp>(a); }
inline uint32_t                     getFuncArgKeySeq(const FuncArg& a) { return std::get<FuncArgKeySeqIdx>(a); }
inline const CmdModifier&           getFuncArgModSeq(const FuncArg& a) { return std::get<FuncArgModSeq>(a); }
inline const std::vector<wstringi>& getFuncArgTokens(const FuncArg& a) { return std::get<FuncArgTokenSeq>(a); }


#endif // !_FUNC_ARG_H
