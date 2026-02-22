//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// cmd_stream_reader.h

#ifndef _CMD_STREAM_READER_H
#  define _CMD_STREAM_READER_H

#  include "cmd_stream.h"


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


#endif // !_CMD_STREAM_READER_H
