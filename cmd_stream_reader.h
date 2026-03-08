//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// cmd_stream_reader.h

#ifndef _CMD_STREAM_READER_H
#  define _CMD_STREAM_READER_H

#  include "cmd_stream.h"
#  include <optional>


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
	CmdEventAssignData readEventAssign();
	CmdModAssignData readModAssign();

	/// Read the next command as a fully-parsed value.
	/// Returns std::nullopt on EOF or unknown CmdId.
	std::optional<AnyCmd> readCmd();

	/// Dump the entire command stream to text (replaces BcDisassembler)
	static void dump(std::istream &in, std::wostream &out);

private:
	std::istream &m_in;

	// Primitive readers (little-endian)
	uint8_t readU8();
	uint16_t readU16();
	uint32_t readU32();
	int32_t readI32();
	uint64_t readU64();
	wstringi readString();
	CmdModifier readModifier();
	CmdScanCode readScanCode();
	CmdModifiedKey readModifiedKey();
	CmdArgument readArgument();
	CmdAction readAction();
	CmdKeySequence readKeySequence();

	// Dump helpers
	static void dumpModifier(std::wostream &out, const CmdModifier &mod);
	static void dumpAction(std::wostream &out, const CmdAction &action, int indent);
	static void dumpArgument(std::wostream &out, const CmdArgument &arg);
};


#endif // !_CMD_STREAM_READER_H
