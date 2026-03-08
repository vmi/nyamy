//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// cmd_stream_writer.h

#ifndef _CMD_STREAM_WRITER_H
#  define _CMD_STREAM_WRITER_H

#  include "cmd_stream.h"


//=============================================================================
// CmdStreamWriter - writes commands to an output stream
//=============================================================================

class CmdStreamWriter
{
public:
	explicit CmdStreamWriter(std::ostream &out);

	void writeRegKeySeq(const CmdArgsRegKeySeq &ks);
	void writeDefKey(const CmdArgsDefKey &data);
	void writeDefMod(const CmdArgsDefMod &data);
	void writeDefSync(const CmdArgsDefSync &data);
	void writeDefAlias(const CmdArgsDefAlias &data);
	void writeDefSubst(const CmdArgsDefSubst &data);
	void writeDefOption(const CmdArgsDefOption &data);
	void writeDefSymbol(const CmdArgsDefSymbol &data);
	void writeBeginKeymap(const CmdArgsBeginKeymap &data);
	void writeAssignKey(const CmdArgsAssignKey &data);
	void writeAssignEvent(const CmdArgsAssignEvent &data);
	void writeAssignMod(const CmdArgsAssignMod &data);
	void writeCommit();

private:
	std::ostream &m_out;

	// Primitive writers (little-endian)
	void writeU8(uint8_t v);
	void writeU16(uint16_t v);
	void writeU32(uint32_t v);
	void writeI32(int32_t v);
	void writeU64(uint64_t v);
	void writeString(const wstringi &s);
	void writeModifier(const CmdModifier &mod);
	void writeScanCode(const CmdScanCode &sc);
	void writeModifiedKey(const CmdModifiedKey &mk);
	void writeArgument(const CmdFuncArg &arg);
	void writeAction(const CmdAction &action);
	void writeKeySequence(const CmdArgsRegKeySeq &ks);
};


#endif // !_CMD_STREAM_H
