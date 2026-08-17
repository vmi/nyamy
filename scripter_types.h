//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// scripter_types.h
//
// Types defined by the scripter pipeline and shared with the engine side.
// ModifierSpec: modifier condition bitfields (resolved from string form at compile time).
// FuncArg: unified function argument type (std::variant).
// Used in both the CmdStream pipeline and the CtrlStream IPC pipeline.

#ifndef _SCRIPTER_TYPES_H
#  define _SCRIPTER_TYPES_H

#  include "stringtool.h"
#  include <cstdint>
#  include <iosfwd>
#  include <variant>
#  include <vector>


/// Modifier condition bitfields (resolved from string form at compile time)
struct ModifierSpec {
	uint64_t modifiers;   ///< bit set = modifier is ON
	uint64_t dontcares;   ///< bit set = don't care (wildcard match)

	ModifierSpec() : modifiers(0), dontcares(0) {}
};

std::wostream& operator<<(std::wostream& out, const ModifierSpec& mod);


/** A "$NAME" written where a function argument goes.

    That spelling means one of two things - a reference to a named keyseq, or an
    argument substitution such as $Clipboard - and which one is decided by the
    type of the parameter it lands on.  Only the generated loadFromCmd() knows
    those types, so the name travels this far as itself and is resolved there.
    Compare the old text loader, where the parse was type directed and
    load_ARGUMENT() was overloaded per parameter type. */
struct FuncArgDollarName {
	wstringq m_name;			///< the name, without the '$'
};


/// Per-type structs for FuncArg variant
using FuncArgString       = wstringq;
using FuncArgNumber       = int32_t;
using FuncArgRegexp       = wregex_stored;
using FuncArgKeySeqIdx    = uint32_t;
using FuncArgModifierSpec = ModifierSpec;
using FuncArgTokenSeq     = std::vector<wstringi>;


/// Unified function argument type
using FuncArg = std::variant<
	FuncArgString,       // 0
	FuncArgNumber,       // 1
	FuncArgRegexp,       // 2
	FuncArgKeySeqIdx,    // 3
	FuncArgModifierSpec, // 4
	FuncArgTokenSeq,     // 5
	FuncArgDollarName    // 6
>;

std::wostream& operator<<(std::wostream& out, const FuncArg& arg);


/// Serialization tag (matches variant index)
enum FuncArgTag : uint8_t {
	FuncArgTag_String       = 0,
	FuncArgTag_Number       = 1,
	FuncArgTag_Regexp       = 2,
	FuncArgTag_KeySeqIdx    = 3,
	FuncArgTag_ModifierSpec = 4,
	FuncArgTag_TokenSeq     = 5,
	FuncArgTag_DollarName   = 6,
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
inline const ModifierSpec&          getFuncArgModifierSpec(const FuncArg& a) { return std::get<FuncArgModifierSpec>(a); }
inline const std::vector<wstringi>& getFuncArgTokens(const FuncArg& a) { return std::get<FuncArgTokenSeq>(a); }
inline const wstringq&              getFuncArgDollarName(const FuncArg& a) { return std::get<FuncArgDollarName>(a).m_name; }
inline bool                         isFuncArgDollarName(const FuncArg& a) { return std::holds_alternative<FuncArgDollarName>(a); }


#endif // !_SCRIPTER_TYPES_H
