//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// cmd_stream_writer.cpp


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


void CmdStreamWriter::writeString(const wstringi &s)
{
	uint16_t len = static_cast<uint16_t>(s.size());
	writeU16(len);
	for (size_t i = 0; i < len; ++i) {
		wchar_t ch = s[i];
		writeU8(static_cast<uint8_t>(ch & 0xFF));
		writeU8(static_cast<uint8_t>((ch >> 8) & 0xFF));
	}
}


void CmdStreamWriter::writeModifierSpec(const ModifierSpec &mod)
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
	writeModifierSpec(mk.modifier);
	writeString(mk.keyName);
}


void CmdStreamWriter::writeArgument(const FuncArg &arg)
{
	std::visit(overloaded{
		[&](const FuncArgString&       a) { writeU8(FuncArgTag_String);       writeString(a); },
		[&](const FuncArgNumber&       a) { writeU8(FuncArgTag_Number);       writeI32(a); },
		[&](const FuncArgRegexp&       a) { writeU8(FuncArgTag_Regexp);       writeString(a.str()); },
		[&](const FuncArgKeySeqIdx&    a) { writeU8(FuncArgTag_KeySeqIdx);    writeU32(a); },
		[&](const FuncArgModifierSpec& a) { writeU8(FuncArgTag_ModifierSpec); writeModifierSpec(a); },
		[&](const FuncArgTokenSeq&  a) {
			writeU8(FuncArgTag_TokenSeq);
			writeU16(static_cast<uint16_t>(a.size()));
			for (const auto &tok : a)
				writeString(tok);
		},
	}, arg);
}


void CmdStreamWriter::writeAction(const CmdAction &action)
{
	writeU8(static_cast<uint8_t>(action.type));
	writeModifierSpec(action.modifier);
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


void CmdStreamWriter::writeKeySequence(const CmdArgsRegKeySeq &ks)
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

void CmdStreamWriter::writeRegKeySeq(const CmdArgsRegKeySeq &ks)
{
	writeU8(static_cast<uint8_t>(CmdId::RegKeySeq));
	writeKeySequence(ks);
}


void CmdStreamWriter::writeDefKey(const CmdArgsDefKey &data)
{
	writeU8(static_cast<uint8_t>(CmdId::DefKey));
	writeU32(static_cast<uint32_t>(data.names.size()));
	for (const auto &n : data.names)
		writeString(n);
	writeU32(static_cast<uint32_t>(data.scanCodes.size()));
	for (const auto &sc : data.scanCodes)
		writeScanCode(sc);
}


void CmdStreamWriter::writeDefMod(const CmdArgsDefMod &data)
{
	writeU8(static_cast<uint8_t>(CmdId::DefMod));
	writeString(data.modifierName);
	writeU32(static_cast<uint32_t>(data.keyNames.size()));
	for (const auto &n : data.keyNames)
		writeString(n);
}


void CmdStreamWriter::writeDefSync(const CmdArgsDefSync &data)
{
	writeU8(static_cast<uint8_t>(CmdId::DefSync));
	writeU32(static_cast<uint32_t>(data.scanCodes.size()));
	for (const auto &sc : data.scanCodes)
		writeScanCode(sc);
}


void CmdStreamWriter::writeDefAlias(const CmdArgsDefAlias &data)
{
	writeU8(static_cast<uint8_t>(CmdId::DefAlias));
	writeString(data.aliasName);
	writeString(data.keyName);
}


void CmdStreamWriter::writeDefSubst(const CmdArgsDefSubst &data)
{
	writeU8(static_cast<uint8_t>(CmdId::DefSubst));
	writeU32(static_cast<uint32_t>(data.lhsKeys.size()));
	for (const auto &mk : data.lhsKeys)
		writeModifiedKey(mk);
	writeU32(data.rhsKeySeqIdx);
}


void CmdStreamWriter::writeDefOption(const CmdArgsDefOption &data)
{
	writeU8(static_cast<uint8_t>(CmdId::DefOption));
	writeString(data.optionName);
	writeString(data.value);
}


void CmdStreamWriter::writeDefSymbol(const CmdArgsDefSymbol &data)
{
	writeU8(static_cast<uint8_t>(CmdId::DefSymbol));
	writeString(data.symbolName);
}


void CmdStreamWriter::writeBeginKeymap(const CmdArgsBeginKeymap &data)
{
	writeU8(static_cast<uint8_t>(CmdId::BeginKeymap));
	writeString(data.keyword);
	writeString(data.name);
	writeString(data.windowClassName);
	writeString(data.windowTitleName);
	writeString(data.windowOp);
	writeString(data.parentName);
	writeI32(data.defaultKeySeqIdx);
}


void CmdStreamWriter::writeAssignKey(const CmdArgsAssignKey &data)
{
	writeU8(static_cast<uint8_t>(CmdId::AssignKey));
	writeU32(static_cast<uint32_t>(data.lhsKeys.size()));
	for (const auto &mk : data.lhsKeys)
		writeModifiedKey(mk);
	writeU32(data.rhsKeySeqIdx);
}


void CmdStreamWriter::writeAssignEvent(const CmdArgsAssignEvent &data)
{
	writeU8(static_cast<uint8_t>(CmdId::AssignEvent));
	writeString(data.eventName);
	writeU32(data.rhsKeySeqIdx);
}


void CmdStreamWriter::writeAssignMod(const CmdArgsAssignMod &data)
{
	writeU8(static_cast<uint8_t>(CmdId::AssignMod));
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



void CmdStreamWriter::writePushKeymap()
{
	writeU8(static_cast<uint8_t>(CmdId::PushKeymap));
}


void CmdStreamWriter::writePopKeymap()
{
	writeU8(static_cast<uint8_t>(CmdId::PopKeymap));
}


void CmdStreamWriter::writeReset()
{
	writeU8(static_cast<uint8_t>(CmdId::Reset));
}


void CmdStreamWriter::writeCommit()
{
	writeU8(static_cast<uint8_t>(CmdId::Commit));
}


void CmdStreamWriter::writeExecKeySeq(const std::vector<CmdAction> &actions,
                                      const TriggerInfo &ctx)
{
	writeU8(static_cast<uint8_t>(CmdId::ExecKeySeq));
	writeU32(static_cast<uint32_t>(actions.size()));
	for (const auto &action : actions)
		writeAction(action);
	writeU8(ctx.scanCode);
	writeU8(ctx.extended ? 1 : 0);
	writeString(wstringi(ctx.windowClass));
	writeString(wstringi(ctx.windowTitle));
}
