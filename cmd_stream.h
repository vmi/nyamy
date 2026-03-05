//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// cmd_stream.h
//
// Command stream format for .mayu configuration processing.
// Commands are written/read sequentially on a byte stream (pipe, etc.).
// Each command is: [CmdId:uint8] [arguments...].
// No stream header; the first byte is always a CmdId.

#ifndef _CMD_STREAM_H
#  define _CMD_STREAM_H

#  include "stringtool.h"
#  include <variant>
#  include <vector>
#  include <cstdint>


//=============================================================================
// Command IDs
//=============================================================================

enum class CmdId : uint8_t {
	DefKeySeq      = 0x01,
	DefKey         = 0x10,
	DefModifier    = 0x11,
	DefSync        = 0x12,
	DefAlias       = 0x13,
	DefSubstitute  = 0x14,
	DefOption      = 0x15,
	DefSymbol      = 0x16,
	KeymapDef      = 0x20,
	KeyAssign      = 0x21,
	KeyDefaultMod  = 0x22,
	EventAssign    = 0x23,
	ModAssign      = 0x24,
	KeySeqDef      = 0x25,
	Commit         = 0xFF,
};


//=============================================================================
// Bytecode sub-structures (transferred from bytecode.h)
//=============================================================================

/// Modifier state (bitfield, resolved from string form at compile time)
struct CmdModifier {
	uint64_t modifiers;
	uint64_t dontcares;

	CmdModifier() : modifiers(0), dontcares(0) {}
};


/// Scan code with flags
struct CmdScanCode {
	uint16_t scan;
	uint16_t flags;		///< E0, E1

	CmdScanCode() : scan(0), flags(0) {}
};


/// Modified key reference (modifier + key name)
struct CmdModifiedKey {
	CmdModifier modifier;
	wstringi keyName;
};


/// Function argument in bytecode
struct CmdArgument {
	enum Type : uint8_t {
		String,
		Number,
		Regexp,
		KeySeqIdx,		///< index into keySeqPool
		ModSeq,			///< modifier sequence
		TokenSeq,		///< raw token sequence (e.g. U- D- RButton)
	};

	Type type;
	wstringi stringValue;
	int32_t numberValue;
	uint32_t keySeqIndex;
	CmdModifier modifierValue;
	std::vector<wstringi> tokens;		///< for TokenSeq

	CmdArgument() : type(String), numberValue(0), keySeqIndex(0) {}
};


/// Action within a key sequence
struct CmdAction {
	enum Type : uint8_t {
		Key,			///< modified key press
		KeySeqRef,		///< $name reference
		FuncCall,		///< &function(args)
		SubSeq,			///< (sub-sequence)
	};

	Type type;
	CmdModifier modifier;
	wstringi name;				///< key name / keyseq name / func name
	std::vector<CmdArgument> arguments;	///< for FuncCall only
	std::vector<CmdAction> subActions;	///< for SubSeq only

	CmdAction() : type(Key) {}
};


/// Named key sequence (stored in the pool)
struct CmdKeySequence {
	wstringi name;
	uint8_t mode;			///< Modifier::Type_KEYSEQ or Type_ASSIGN
	std::vector<CmdAction> actions;

	CmdKeySequence() : mode(0) {}
};


//=============================================================================
// Command data structures (moved from bytecode.h)
//=============================================================================

struct CmdDefKeyData {
	std::vector<wstringi> names;
	std::vector<CmdScanCode> scanCodes;
};

struct CmdDefModifierData {
	wstringi modifierName;
	std::vector<wstringi> keyNames;
};

struct CmdDefSyncData {
	std::vector<CmdScanCode> scanCodes;
};

struct CmdDefAliasData {
	wstringi aliasName;
	wstringi keyName;
};

struct CmdDefSubstituteData {
	std::vector<CmdModifiedKey> lhsKeys;
	uint32_t rhsKeySeqIdx;

	CmdDefSubstituteData() : rhsKeySeqIdx(0) {}
};

struct CmdDefOptionData {
	wstringi optionName;
	wstringi qualifier;
	wstringi value;
};

struct CmdDefSymbolData {
	wstringi symbolName;
};

struct CmdKeymapDefData {
	wstringi keyword;		///< "keymap", "keymap2", "window"
	wstringi name;
	wstringi windowClassName;
	wstringi windowTitleName;
	wstringi windowOp;		///< "&&", "||", or empty
	wstringi parentName;
	int32_t defaultKeySeqIdx;	///< -1 if none

	CmdKeymapDefData() : defaultKeySeqIdx(-1) {}
};

struct CmdKeyAssignData {
	std::vector<CmdModifiedKey> lhsKeys;
	uint32_t rhsKeySeqIdx;

	CmdKeyAssignData() : rhsKeySeqIdx(0) {}
};

struct CmdKeyDefaultModData {
	CmdModifier assignMod;
	CmdModifier keySeqMod;
};

struct CmdEventAssignData {
	wstringi eventName;
	uint32_t rhsKeySeqIdx;

	CmdEventAssignData() : rhsKeySeqIdx(0) {}
};

struct CmdModAssignData {
	struct PrefixMod {
		wstringi assignMode;
		wstringi modifierName;
	};
	struct KeyEntry {
		wstringi assignMode;
		wstringi keyName;
	};

	std::vector<PrefixMod> prefixes;
	wstringi mainModifierName;
	wstringi op;			///< "=", "+=", "-="
	std::vector<KeyEntry> keys;
};

struct CmdKeySeqDefData {
	uint32_t keySeqIdx;

	CmdKeySeqDefData() : keySeqIdx(0) {}
};


//=============================================================================
// AnyCmd variant - a single fully-parsed command from a CmdStream
//=============================================================================

/// Tag-only struct for the payload-free Commit command
struct CmdCommit {};

/// A fully-parsed command, covering all CmdId values
using AnyCmd = std::variant<
	CmdKeySequence,
	CmdDefKeyData,
	CmdDefModifierData,
	CmdDefSyncData,
	CmdDefAliasData,
	CmdDefSubstituteData,
	CmdDefOptionData,
	CmdDefSymbolData,
	CmdKeymapDefData,
	CmdKeyAssignData,
	CmdKeyDefaultModData,
	CmdEventAssignData,
	CmdModAssignData,
	CmdKeySeqDefData,
	CmdCommit
>;


#endif // !_CMD_STREAM_H
