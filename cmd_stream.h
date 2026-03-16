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
#  include "ctrl_stream.h"
#  include <variant>
#  include <vector>
#  include <cstdint>

// ModifierSpec, FuncArg and related types are defined in scripter_types.h
// (included transitively via ctrl_stream.h)


//=============================================================================
// Command IDs
//=============================================================================

enum class CmdId : uint8_t {
	RegKeySeq      = 0x01,
	ExecKeySeq     = 0x02,
	DefKey         = 0x10,
	DefMod         = 0x11,
	DefSync        = 0x12,
	DefAlias       = 0x13,
	DefSubst       = 0x14,
	DefOption      = 0x15,
	DefSymbol      = 0x16,
	BeginKeymap    = 0x20,
	AssignKey      = 0x21,
	AssignEvent    = 0x23,
	AssignMod      = 0x24,
	Commit         = 0xFF,
};


//=============================================================================
// Bytecode sub-structures (transferred from bytecode.h)
//=============================================================================

/// Scan code with flags
struct CmdScanCode {
	uint16_t scan;
	uint16_t flags;		///< E0, E1

	CmdScanCode() : scan(0), flags(0) {}
};


/// Modified key reference (modifier + key name)
struct CmdModifiedKey {
	ModifierSpec modifier;
	wstringi keyName;
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
	ModifierSpec modifier;
	wstringi name;				///< key name / keyseq name / func name
	std::vector<FuncArg> arguments;		///< for FuncCall only
	std::vector<CmdAction> subActions;	///< for SubSeq only

	CmdAction() : type(Key) {}
};


/// Named key sequence (stored in the pool)
struct CmdArgsRegKeySeq {
	wstringi name;
	uint8_t mode;			///< Modifier::Type_KEYSEQ or Type_ASSIGN
	std::vector<CmdAction> actions;

	CmdArgsRegKeySeq() : mode(0) {}
};


//=============================================================================
// Command data structures (moved from bytecode.h)
//=============================================================================

struct CmdArgsDefKey {
	std::vector<wstringi> names;
	std::vector<CmdScanCode> scanCodes;
};

struct CmdArgsDefMod {
	wstringi modifierName;
	std::vector<wstringi> keyNames;
};

struct CmdArgsDefSync {
	std::vector<CmdScanCode> scanCodes;
};

struct CmdArgsDefAlias {
	wstringi aliasName;
	wstringi keyName;
};

struct CmdArgsDefSubst {
	std::vector<CmdModifiedKey> lhsKeys;
	uint32_t rhsKeySeqIdx;

	CmdArgsDefSubst() : rhsKeySeqIdx(0) {}
};

struct CmdArgsDefOption {
	wstringi optionName;	///< The qualifier has already been combined by the compiler (e.g., "delay-of !!!").
	wstringi value;
};

struct CmdArgsDefSymbol {
	wstringi symbolName;
};

struct CmdArgsBeginKeymap {
	wstringi keyword;		///< "keymap", "keymap2", "window"
	wstringi name;
	wstringi windowClassName;
	wstringi windowTitleName;
	wstringi windowOp;		///< "&&", "||", or empty
	wstringi parentName;
	int32_t defaultKeySeqIdx;	///< -1 if none

	CmdArgsBeginKeymap() : defaultKeySeqIdx(-1) {}
};

struct CmdArgsAssignKey {
	std::vector<CmdModifiedKey> lhsKeys;
	uint32_t rhsKeySeqIdx;

	CmdArgsAssignKey() : rhsKeySeqIdx(0) {}
};

struct CmdArgsAssignEvent {
	wstringi eventName;
	uint32_t rhsKeySeqIdx;

	CmdArgsAssignEvent() : rhsKeySeqIdx(0) {}
};

struct CmdArgsAssignMod {
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

//=============================================================================
// CmdArgs variant - a single fully-parsed command from a CmdStream
//=============================================================================

/// Tag-only struct for the payload-free Commit command
struct CmdArgsCommit {};

/// Ad-hoc key sequence execution (scripter -> engine)
struct CmdArgsExecKeySeq {
	std::vector<CmdAction>  actions;
	TriggerInfo             context;  ///< echo back from ExecUserFunc
};

/// A fully-parsed command, covering all CmdId values
using CmdArgs = std::variant<
	CmdArgsRegKeySeq,
	CmdArgsExecKeySeq,
	CmdArgsDefKey,
	CmdArgsDefMod,
	CmdArgsDefSync,
	CmdArgsDefAlias,
	CmdArgsDefSubst,
	CmdArgsDefOption,
	CmdArgsDefSymbol,
	CmdArgsBeginKeymap,
	CmdArgsAssignKey,
	CmdArgsAssignEvent,
	CmdArgsAssignMod,
	CmdArgsCommit
>;


#endif // !_CMD_STREAM_H
