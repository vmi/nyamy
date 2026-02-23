//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// cmd_stream.cpp


#include "misc.h"

#include "cmd_stream_writer.h"
#include "errormessage.h"
#include "keyboard.h"

#include <iomanip>
#include <sstream>


//=============================================================================
// CmdStreamWriter - primitive helpers
//=============================================================================

CmdStreamWriter::CmdStreamWriter(std::ostream &out) : m_out(out) {}


void CmdStreamWriter::writeU8(uint8_t v)
{
	m_out.put(static_cast<char>(v));
}


void CmdStreamWriter::writeU16(uint16_t v)
{
	writeU8(static_cast<uint8_t>(v & 0xFF));
	writeU8(static_cast<uint8_t>((v >> 8) & 0xFF));
}


void CmdStreamWriter::writeU32(uint32_t v)
{
	writeU8(static_cast<uint8_t>(v & 0xFF));
	writeU8(static_cast<uint8_t>((v >> 8) & 0xFF));
	writeU8(static_cast<uint8_t>((v >> 16) & 0xFF));
	writeU8(static_cast<uint8_t>((v >> 24) & 0xFF));
}


void CmdStreamWriter::writeI32(int32_t v)
{
	writeU32(static_cast<uint32_t>(v));
}


void CmdStreamWriter::writeU64(uint64_t v)
{
	writeU32(static_cast<uint32_t>(v & 0xFFFFFFFF));
	writeU32(static_cast<uint32_t>((v >> 32) & 0xFFFFFFFF));
}


void CmdStreamWriter::writeString(const tstringi &s)
{
	uint16_t len = static_cast<uint16_t>(s.size());
	writeU16(len);
	for (size_t i = 0; i < len; ++i) {
		_TCHAR ch = s[i];
		writeU8(static_cast<uint8_t>(ch & 0xFF));
		writeU8(static_cast<uint8_t>((ch >> 8) & 0xFF));
	}
}


void CmdStreamWriter::writeModifier(const CmdModifier &mod)
{
	writeU64(mod.modifiers);
	writeU64(mod.dontcares);
}


void CmdStreamWriter::writeScanCode(const CmdScanCode &sc)
{
	writeU16(sc.scan);
	writeU16(sc.flags);
}


void CmdStreamWriter::writeModifiedKey(const CmdModifiedKey &mk)
{
	writeModifier(mk.modifier);
	writeString(mk.keyName);
}


void CmdStreamWriter::writeArgument(const CmdArgument &arg)
{
	writeU8(static_cast<uint8_t>(arg.type));
	switch (arg.type) {
	case CmdArgument::String:
	case CmdArgument::Regexp:
		writeString(arg.stringValue);
		break;
	case CmdArgument::Number:
		writeI32(arg.numberValue);
		writeString(arg.stringValue);
		break;
	case CmdArgument::KeySeqIdx:
		writeU32(arg.keySeqIndex);
		break;
	case CmdArgument::ModSeq:
		writeModifier(arg.modifierValue);
		break;
	case CmdArgument::TokenSeq:
		writeU16(static_cast<uint16_t>(arg.tokens.size()));
		for (const auto &tok : arg.tokens)
			writeString(tok);
		break;
	}
}


void CmdStreamWriter::writeAction(const CmdAction &action)
{
	writeU8(static_cast<uint8_t>(action.type));
	writeModifier(action.modifier);
	writeString(action.name);

	switch (action.type) {
	case CmdAction::Key:
	case CmdAction::KeySeqRef:
		break;
	case CmdAction::FuncCall:
		writeU32(static_cast<uint32_t>(action.arguments.size()));
		for (const auto &arg : action.arguments)
			writeArgument(arg);
		break;
	case CmdAction::SubSeq:
		writeU32(static_cast<uint32_t>(action.subActions.size()));
		for (const auto &sub : action.subActions)
			writeAction(sub);
		break;
	}
}


void CmdStreamWriter::writeKeySequence(const CmdKeySequence &ks)
{
	writeString(ks.name);
	writeU8(ks.mode);
	writeU32(static_cast<uint32_t>(ks.actions.size()));
	for (const auto &action : ks.actions)
		writeAction(action);
}


//=============================================================================
// CmdStreamWriter - command writers
//=============================================================================

void CmdStreamWriter::writeDefKeySeq(const CmdKeySequence &ks)
{
	writeU8(static_cast<uint8_t>(CmdId::DefKeySeq));
	writeKeySequence(ks);
}


void CmdStreamWriter::writeDefKey(const CmdDefKeyData &data)
{
	writeU8(static_cast<uint8_t>(CmdId::DefKey));
	writeU32(static_cast<uint32_t>(data.names.size()));
	for (const auto &n : data.names)
		writeString(n);
	writeU32(static_cast<uint32_t>(data.scanCodes.size()));
	for (const auto &sc : data.scanCodes)
		writeScanCode(sc);
}


void CmdStreamWriter::writeDefModifier(const CmdDefModifierData &data)
{
	writeU8(static_cast<uint8_t>(CmdId::DefModifier));
	writeString(data.modifierName);
	writeU32(static_cast<uint32_t>(data.keyNames.size()));
	for (const auto &n : data.keyNames)
		writeString(n);
}


void CmdStreamWriter::writeDefSync(const CmdDefSyncData &data)
{
	writeU8(static_cast<uint8_t>(CmdId::DefSync));
	writeU32(static_cast<uint32_t>(data.scanCodes.size()));
	for (const auto &sc : data.scanCodes)
		writeScanCode(sc);
}


void CmdStreamWriter::writeDefAlias(const CmdDefAliasData &data)
{
	writeU8(static_cast<uint8_t>(CmdId::DefAlias));
	writeString(data.aliasName);
	writeString(data.keyName);
}


void CmdStreamWriter::writeDefSubstitute(const CmdDefSubstituteData &data)
{
	writeU8(static_cast<uint8_t>(CmdId::DefSubstitute));
	writeU32(static_cast<uint32_t>(data.lhsKeys.size()));
	for (const auto &mk : data.lhsKeys)
		writeModifiedKey(mk);
	writeU32(data.rhsKeySeqIdx);
}


void CmdStreamWriter::writeDefOption(const CmdDefOptionData &data)
{
	writeU8(static_cast<uint8_t>(CmdId::DefOption));
	writeString(data.optionName);
	writeString(data.qualifier);
	writeString(data.value);
}


void CmdStreamWriter::writeDefSymbol(const CmdDefSymbolData &data)
{
	writeU8(static_cast<uint8_t>(CmdId::DefSymbol));
	writeString(data.symbolName);
}


void CmdStreamWriter::writeKeymapDef(const CmdKeymapDefData &data)
{
	writeU8(static_cast<uint8_t>(CmdId::KeymapDef));
	writeString(data.keyword);
	writeString(data.name);
	writeString(data.windowClassName);
	writeString(data.windowTitleName);
	writeString(data.windowOp);
	writeString(data.parentName);
	writeI32(data.defaultKeySeqIdx);
}


void CmdStreamWriter::writeKeyAssign(const CmdKeyAssignData &data)
{
	writeU8(static_cast<uint8_t>(CmdId::KeyAssign));
	writeU32(static_cast<uint32_t>(data.lhsKeys.size()));
	for (const auto &mk : data.lhsKeys)
		writeModifiedKey(mk);
	writeU32(data.rhsKeySeqIdx);
}


void CmdStreamWriter::writeKeyDefaultMod(const CmdKeyDefaultModData &data)
{
	writeU8(static_cast<uint8_t>(CmdId::KeyDefaultMod));
	writeModifier(data.assignMod);
	writeModifier(data.keySeqMod);
}


void CmdStreamWriter::writeEventAssign(const CmdEventAssignData &data)
{
	writeU8(static_cast<uint8_t>(CmdId::EventAssign));
	writeString(data.eventName);
	writeU32(data.rhsKeySeqIdx);
}


void CmdStreamWriter::writeModAssign(const CmdModAssignData &data)
{
	writeU8(static_cast<uint8_t>(CmdId::ModAssign));
	writeU32(static_cast<uint32_t>(data.prefixes.size()));
	for (const auto &p : data.prefixes) {
		writeString(p.assignMode);
		writeString(p.modifierName);
	}
	writeString(data.mainModifierName);
	writeString(data.op);
	writeU32(static_cast<uint32_t>(data.keys.size()));
	for (const auto &k : data.keys) {
		writeString(k.assignMode);
		writeString(k.keyName);
	}
}


void CmdStreamWriter::writeKeySeqDef(const CmdKeySeqDefData &data)
{
	writeU8(static_cast<uint8_t>(CmdId::KeySeqDef));
	writeU32(data.keySeqIdx);
}


void CmdStreamWriter::writeReset()
{
	writeU8(static_cast<uint8_t>(CmdId::Reset));
}


void CmdStreamWriter::writeCommit()
{
	writeU8(static_cast<uint8_t>(CmdId::Commit));
}
