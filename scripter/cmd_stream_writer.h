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


#endif // !_CMD_STREAM_H
