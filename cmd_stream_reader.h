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
	CmdArgsRegKeySeq readRegKeySeq();
	CmdArgsExecKeySeq readExecKeySeq();
	CmdArgsDefKey readDefKey();
	CmdArgsDefMod readDefMod();
	CmdArgsDefSync readDefSync();
	CmdArgsDefAlias readDefAlias();
	CmdArgsDefSubst readDefSubst();
	CmdArgsDefOption readDefOption();
	CmdArgsDefSymbol readDefSymbol();
	CmdArgsBeginKeymap readBeginKeymap();
	CmdArgsAssignKey readAssignKey();
	CmdArgsAssignEvent readAssignEvent();
	CmdArgsAssignMod readAssignMod();

	/// Read the next command as a fully-parsed value.
	/// Returns std::nullopt on EOF or unknown CmdId.
	std::optional<CmdArgs> readCmd();

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
	CmdFuncArg readArgument();
	CmdAction readAction();
	CmdArgsRegKeySeq readKeySequence();

	// Dump helpers
	static void dumpModifier(std::wostream &out, const CmdModifier &mod);
	static void dumpAction(std::wostream &out, const CmdAction &action, int indent);
	static void dumpArgument(std::wostream &out, const CmdFuncArg &arg);
};


#endif // !_CMD_STREAM_READER_H
