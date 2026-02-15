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
	Reset          = 0xFE,
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
	tstringi keyName;
};


/// Function argument in bytecode
struct CmdArgument {
	enum Type : uint8_t {
		String,
		Number,
		Regexp,
		KeySeqIdx,		///< index into keySeqPool
		ModSeq,			///< modifier sequence
	};

	Type type;
	tstringi stringValue;
	int32_t numberValue;
	uint32_t keySeqIndex;
	CmdModifier modifierValue;

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
	tstringi name;				///< key name / keyseq name / func name
	std::vector<CmdArgument> arguments;	///< for FuncCall only
	std::vector<CmdAction> subActions;	///< for SubSeq only

	CmdAction() : type(Key) {}
};


/// Named key sequence (stored in the pool)
struct CmdKeySequence {
	tstringi name;
	uint8_t mode;			///< Modifier::Type_KEYSEQ or Type_ASSIGN
	std::vector<CmdAction> actions;

	CmdKeySequence() : mode(0) {}
};


//=============================================================================
// Command data structures (moved from bytecode.h)
//=============================================================================

struct CmdDefKeyData {
	std::vector<tstringi> names;
	std::vector<CmdScanCode> scanCodes;
};

struct CmdDefModifierData {
	tstringi modifierName;
	std::vector<tstringi> keyNames;
};

struct CmdDefSyncData {
	std::vector<CmdScanCode> scanCodes;
};

struct CmdDefAliasData {
	tstringi aliasName;
	tstringi keyName;
};

struct CmdDefSubstituteData {
	std::vector<CmdModifiedKey> lhsKeys;
	uint32_t rhsKeySeqIdx;

	CmdDefSubstituteData() : rhsKeySeqIdx(0) {}
};

struct CmdDefOptionData {
	tstringi optionName;
	tstringi qualifier;
	tstringi value;
};

struct CmdDefSymbolData {
	tstringi symbolName;
};

struct CmdKeymapDefData {
	tstringi keyword;		///< "keymap", "keymap2", "window"
	tstringi name;
	tstringi windowClassName;
	tstringi windowTitleName;
	tstringi windowOp;		///< "&&", "||", or empty
	tstringi parentName;
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
	tstringi eventName;
	uint32_t rhsKeySeqIdx;

	CmdEventAssignData() : rhsKeySeqIdx(0) {}
};

struct CmdModAssignData {
	struct PrefixMod {
		tstringi assignMode;
		tstringi modifierName;
	};
	struct KeyEntry {
		tstringi assignMode;
		tstringi keyName;
	};

	std::vector<PrefixMod> prefixes;
	tstringi mainModifierName;
	tstringi op;			///< "=", "+=", "-="
	std::vector<KeyEntry> keys;
};

struct CmdKeySeqDefData {
	uint32_t keySeqIdx;

	CmdKeySeqDefData() : keySeqIdx(0) {}
};


//=============================================================================
// CmdStreamWriter - writes commands to an output stream
//=============================================================================

class CmdStreamWriter
{
public:
	explicit CmdStreamWriter(std::ostream &out);

	void writeDefKeySeq(const CmdKeySequence &ks);
	void writeDefKey(const CmdDefKeyData &data);
	void writeDefModifier(const CmdDefModifierData &data);
	void writeDefSync(const CmdDefSyncData &data);
	void writeDefAlias(const CmdDefAliasData &data);
	void writeDefSubstitute(const CmdDefSubstituteData &data);
	void writeDefOption(const CmdDefOptionData &data);
	void writeDefSymbol(const CmdDefSymbolData &data);
	void writeKeymapDef(const CmdKeymapDefData &data);
	void writeKeyAssign(const CmdKeyAssignData &data);
	void writeKeyDefaultMod(const CmdKeyDefaultModData &data);
	void writeEventAssign(const CmdEventAssignData &data);
	void writeModAssign(const CmdModAssignData &data);
	void writeKeySeqDef(const CmdKeySeqDefData &data);
	void writeReset();
	void writeCommit();

private:
	std::ostream &m_out;

	// Primitive writers (little-endian)
	void writeU8(uint8_t v);
	void writeU16(uint16_t v);
	void writeU32(uint32_t v);
	void writeI32(int32_t v);
	void writeU64(uint64_t v);
	void writeString(const tstringi &s);
	void writeModifier(const CmdModifier &mod);
	void writeScanCode(const CmdScanCode &sc);
	void writeModifiedKey(const CmdModifiedKey &mk);
	void writeArgument(const CmdArgument &arg);
	void writeAction(const CmdAction &action);
	void writeKeySequence(const CmdKeySequence &ks);
};


//=============================================================================
// CmdStreamReader - reads commands from an input stream
//=============================================================================

class CmdStreamReader
{
public:
	explicit CmdStreamReader(std::istream &in);

	/// Read the next command ID. Returns false on EOF.
	bool readNext(CmdId &cmdId);

	// Data readers - call after readNext() returns the corresponding CmdId
	CmdKeySequence readDefKeySeq();
	CmdDefKeyData readDefKey();
	CmdDefModifierData readDefModifier();
	CmdDefSyncData readDefSync();
	CmdDefAliasData readDefAlias();
	CmdDefSubstituteData readDefSubstitute();
	CmdDefOptionData readDefOption();
	CmdDefSymbolData readDefSymbol();
	CmdKeymapDefData readKeymapDef();
	CmdKeyAssignData readKeyAssign();
	CmdKeyDefaultModData readKeyDefaultMod();
	CmdEventAssignData readEventAssign();
	CmdModAssignData readModAssign();
	CmdKeySeqDefData readKeySeqDef();

	/// Dump the entire command stream to text (replaces BcDisassembler)
	static void dump(std::istream &in, tostream &out);

private:
	std::istream &m_in;

	// Primitive readers (little-endian)
	uint8_t readU8();
	uint16_t readU16();
	uint32_t readU32();
	int32_t readI32();
	uint64_t readU64();
	tstringi readString();
	CmdModifier readModifier();
	CmdScanCode readScanCode();
	CmdModifiedKey readModifiedKey();
	CmdArgument readArgument();
	CmdAction readAction();
	CmdKeySequence readKeySequence();

	// Dump helpers
	static void dumpModifier(tostream &out, const CmdModifier &mod);
	static void dumpAction(tostream &out, const CmdAction &action, int indent);
	static void dumpArgument(tostream &out, const CmdArgument &arg);
};


#endif // !_CMD_STREAM_H
