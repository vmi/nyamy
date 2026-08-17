//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// cmd_stream_reader.cpp


#include "misc.h"

#include "cmd_stream_reader.h"
#include "errormessage.h"

#include <iomanip>
#include <sstream>


//=============================================================================
// CmdStreamReader - primitive helpers
//=============================================================================

CmdStreamReader::CmdStreamReader(std::istream &in) : m_in(in) {}


std::optional<CmdArgs> CmdStreamReader::readCmd()
{
	CmdId cmdId;
	if (!readNext(cmdId)) return std::nullopt;
	switch (cmdId) {
	case CmdId::RegKeySeq:     return readRegKeySeq();
	case CmdId::ExecKeySeq:    return readExecKeySeq();
	case CmdId::DefKey:        return readDefKey();
	case CmdId::DefMod:   return readDefMod();
	case CmdId::DefSync:       return readDefSync();
	case CmdId::DefAlias:      return readDefAlias();
	case CmdId::DefSubst: return readDefSubst();
	case CmdId::DefOption:     return readDefOption();
	case CmdId::DefSymbol:     return readDefSymbol();
	case CmdId::DefKeymap:     return readDefKeymap();
	case CmdId::AssignKey:     return readAssignKey();
	case CmdId::AssignEvent:   return readAssignEvent();
	case CmdId::AssignMod:     return readAssignMod();
	case CmdId::EndKeymap:     return CmdArgsEndKeymap{};
	case CmdId::Reset:         return CmdArgsReset{};
	case CmdId::Commit:        return CmdArgsCommit{};
	default:                   return std::nullopt;
	}
}


bool CmdStreamReader::readNext(CmdId &cmdId)
{
	int ch = m_in.get();
	if (ch == std::char_traits<char>::eof())
		return false;
	cmdId = static_cast<CmdId>(static_cast<uint8_t>(ch));
	return true;
}


uint8_t CmdStreamReader::readU8()
{
	int ch = m_in.get();
	if (ch == std::char_traits<char>::eof())
		throw ErrorMessage() << L"unexpected end of command stream";
	return static_cast<uint8_t>(ch);
}


uint16_t CmdStreamReader::readU16()
{
	uint16_t lo = readU8();
	uint16_t hi = readU8();
	return lo | (hi << 8);
}


uint32_t CmdStreamReader::readU32()
{
	uint32_t b0 = readU8();
	uint32_t b1 = readU8();
	uint32_t b2 = readU8();
	uint32_t b3 = readU8();
	return b0 | (b1 << 8) | (b2 << 16) | (b3 << 24);
}


int32_t CmdStreamReader::readI32()
{
	return static_cast<int32_t>(readU32());
}


uint64_t CmdStreamReader::readU64()
{
	uint64_t lo = readU32();
	uint64_t hi = readU32();
	return lo | (hi << 32);
}


wstringi CmdStreamReader::readString()
{
	uint16_t len = readU16();
	wstringi s;
	s.reserve(len);
	for (uint16_t i = 0; i < len; ++i) {
		uint8_t lo = readU8();
		uint8_t hi = readU8();
		s.push_back(static_cast<wchar_t>(lo | (hi << 8)));
	}
	return s;
}


ModifierSpec CmdStreamReader::readModifierSpec()
{
	ModifierSpec mod;
	mod.modifiers = readU64();
	mod.dontcares = readU64();
	return mod;
}


CmdScanCode CmdStreamReader::readScanCode()
{
	CmdScanCode sc;
	sc.scan = readU16();
	sc.flags = readU16();
	return sc;
}


CmdModifiedKey CmdStreamReader::readModifiedKey()
{
	CmdModifiedKey mk;
	mk.modifier = readModifierSpec();
	mk.keyName = readString();
	return mk;
}


FuncArg CmdStreamReader::readArgument()
{
	switch (static_cast<FuncArgTag>(readU8())) {
	case FuncArgTag_String:
		return FuncArgString{ readString() };
	case FuncArgTag_Number:
		return FuncArgNumber{ readI32() };
	case FuncArgTag_Regexp:
		return FuncArgRegexp{ readString() };
	case FuncArgTag_KeySeqIdx:
		return FuncArgKeySeqIdx{ readU32() };
	case FuncArgTag_ModifierSpec:
		return FuncArgModifierSpec{ readModifierSpec() };
	case FuncArgTag_TokenSeq: {
		uint16_t count = readU16();
		FuncArgTokenSeq ts;
		ts.resize(count);
		for (uint16_t i = 0; i < count; ++i)
			ts[i] = readString();
		return ts;
	}
	case FuncArgTag_DollarName:
		return FuncArgDollarName{ readString() };
	default:
		return FuncArgString{};
	}
}


CmdAction CmdStreamReader::readAction()
{
	CmdAction action;
	action.type = static_cast<CmdAction::Type>(readU8());
	action.modifier = readModifierSpec();
	action.name = readString();

	switch (action.type) {
	case CmdAction::Key:
	case CmdAction::KeySeqRef:
		break;
	case CmdAction::FuncCall: {
		uint32_t count = readU32();
		for (uint32_t i = 0; i < count; ++i)
			action.arguments.push_back(readArgument());
		break;
	}
	case CmdAction::SubSeq: {
		uint32_t count = readU32();
		for (uint32_t i = 0; i < count; ++i)
			action.subActions.push_back(readAction());
		break;
	}
	}
	return action;
}


CmdArgsRegKeySeq CmdStreamReader::readKeySequence()
{
	CmdArgsRegKeySeq ks;
	ks.name = readString();
	ks.mode = readU8();
	uint32_t count = readU32();
	for (uint32_t i = 0; i < count; ++i)
		ks.actions.push_back(readAction());
	return ks;
}


//=============================================================================
// CmdStreamReader - command data readers
//=============================================================================

CmdArgsRegKeySeq CmdStreamReader::readRegKeySeq()
{
	return readKeySequence();
}


CmdArgsExecKeySeq CmdStreamReader::readExecKeySeq()
{
	CmdArgsExecKeySeq data;
	uint32_t count = readU32();
	for (uint32_t i = 0; i < count; ++i)
		data.actions.push_back(readAction());
	data.context.scanCode = readU8();
	data.context.extended = (readU8() != 0);
	data.context.windowClass = std::wstring(readString());
	data.context.windowTitle = std::wstring(readString());
	return data;
}


CmdArgsDefKey CmdStreamReader::readDefKey()
{
	CmdArgsDefKey data;
	uint32_t nameCount = readU32();
	for (uint32_t i = 0; i < nameCount; ++i)
		data.names.push_back(readString());
	uint32_t scCount = readU32();
	for (uint32_t i = 0; i < scCount; ++i)
		data.scanCodes.push_back(readScanCode());
	return data;
}


CmdArgsDefMod CmdStreamReader::readDefMod()
{
	CmdArgsDefMod data;
	data.modifierName = readString();
	uint32_t count = readU32();
	for (uint32_t i = 0; i < count; ++i)
		data.keyNames.push_back(readString());
	return data;
}


CmdArgsDefSync CmdStreamReader::readDefSync()
{
	CmdArgsDefSync data;
	uint32_t count = readU32();
	for (uint32_t i = 0; i < count; ++i)
		data.scanCodes.push_back(readScanCode());
	return data;
}


CmdArgsDefAlias CmdStreamReader::readDefAlias()
{
	CmdArgsDefAlias data;
	data.aliasName = readString();
	data.keyName = readString();
	return data;
}


CmdArgsDefSubst CmdStreamReader::readDefSubst()
{
	CmdArgsDefSubst data;
	uint32_t count = readU32();
	for (uint32_t i = 0; i < count; ++i)
		data.lhsKeys.push_back(readModifiedKey());
	data.rhsKeySeqIdx = readU32();
	return data;
}


CmdArgsDefOption CmdStreamReader::readDefOption()
{
	CmdArgsDefOption data;
	data.optionName = readString();
	data.value = readString();
	return data;
}


CmdArgsDefSymbol CmdStreamReader::readDefSymbol()
{
	CmdArgsDefSymbol data;
	data.symbolName = readString();
	return data;
}


CmdArgsDefKeymap CmdStreamReader::readDefKeymap()
{
	CmdArgsDefKeymap data;
	uint8_t scope = readU8();
	if (scope > static_cast<uint8_t>(CmdKeymapScope::Block))
		throw ErrorMessage() << L"DefKeymap: unknown scope " << scope;
	data.scope = static_cast<CmdKeymapScope>(scope);
	data.keyword = readString();
	data.name = readString();
	data.windowClassName = readString();
	data.windowTitleName = readString();
	data.windowOp = readString();
	data.parentName = readString();
	data.defaultKeySeqIdx = readI32();
	return data;
}


CmdArgsAssignKey CmdStreamReader::readAssignKey()
{
	CmdArgsAssignKey data;
	uint32_t count = readU32();
	for (uint32_t i = 0; i < count; ++i)
		data.lhsKeys.push_back(readModifiedKey());
	data.rhsKeySeqIdx = readU32();
	return data;
}



CmdArgsAssignEvent CmdStreamReader::readAssignEvent()
{
	CmdArgsAssignEvent data;
	data.eventName = readString();
	data.rhsKeySeqIdx = readU32();
	return data;
}


CmdArgsAssignMod CmdStreamReader::readAssignMod()
{
	CmdArgsAssignMod data;
	uint32_t prefCount = readU32();
	for (uint32_t i = 0; i < prefCount; ++i) {
		CmdArgsAssignMod::PrefixMod pm;
		pm.assignMode = readString();
		pm.modifierName = readString();
		data.prefixes.push_back(pm);
	}
	data.mainModifierName = readString();
	data.op = readString();
	uint32_t keyCount = readU32();
	for (uint32_t i = 0; i < keyCount; ++i) {
		CmdArgsAssignMod::KeyEntry ke;
		ke.assignMode = readString();
		ke.keyName = readString();
		data.keys.push_back(ke);
	}
	return data;
}


//=============================================================================
// Dump helpers
void CmdStreamReader::dumpAction(std::wostream &out, const CmdAction &action,
								 int indent)
{
	for (int i = 0; i < indent; ++i)
		out << L"  ";

	switch (action.type) {
	case CmdAction::Key:
		out << L"KEY mod=" << action.modifier << L" \"" << action.name << L"\"";
		break;
	case CmdAction::KeySeqRef:
		out << L"REF mod=" << action.modifier << L" $" << action.name;
		break;
	case CmdAction::FuncCall:
		out << L"FUNC mod=" << action.modifier << L" &" << action.name;
		if (!action.arguments.empty()) {
			out << L"(";
			for (size_t i = 0; i < action.arguments.size(); ++i) {
				if (i > 0) out << L", ";
				out << action.arguments[i];
			}
			out << L")";
		}
		break;
	case CmdAction::SubSeq:
		out << L"SUBSEQ mod=" << action.modifier;
		out << std::endl;
		for (const auto &sub : action.subActions)
			dumpAction(out, sub, indent + 1);
		return; // no newline after sub
	}
	out << std::endl;
}


static const wchar_t *keymapScopeToString(CmdKeymapScope scope)
{
	switch (scope) {
	case CmdKeymapScope::Declare: return L"Declare";
	case CmdKeymapScope::Enter:   return L"Enter";
	case CmdKeymapScope::Block:   return L"Block";
	default:                      return L"???";
	}
}


static const wchar_t *cmdIdToString(CmdId id)
{
	switch (id) {
	case CmdId::RegKeySeq:    return L"RegKeySeq";
	case CmdId::ExecKeySeq:   return L"ExecKeySeq";
	case CmdId::DefKey:       return L"DefKey";
	case CmdId::DefMod:  return L"DefMod";
	case CmdId::DefSync:      return L"DefSync";
	case CmdId::DefAlias:     return L"DefAlias";
	case CmdId::DefSubst:return L"DefSubst";
	case CmdId::DefOption:    return L"DefOption";
	case CmdId::DefSymbol:    return L"DefSymbol";
	case CmdId::DefKeymap:    return L"DefKeymap";
	case CmdId::AssignKey:    return L"AssignKey";
	case CmdId::AssignEvent:  return L"AssignEvent";
	case CmdId::AssignMod:    return L"AssignMod";
	case CmdId::EndKeymap:    return L"EndKeymap";
	case CmdId::Reset:        return L"Reset";
	case CmdId::Commit:       return L"Commit";
	default:                  return L"???";
	}
}


void CmdStreamReader::dump(std::istream &in, std::wostream &out)
{
	CmdStreamReader reader(in);
	CmdId cmdId;
	uint32_t cmdIndex = 0;
	uint32_t keySeqIndex = 0;

	out << L"; Command stream dump" << std::endl << std::endl;

	while (reader.readNext(cmdId)) {
		out << L"  "
			<< std::setw(4) << std::setfill(L'0') << cmdIndex
			<< L": ";
		out << std::setw(14) << std::setfill(L' ') << std::left
			<< cmdIdToString(cmdId);

		switch (cmdId) {
		case CmdId::RegKeySeq: {
			CmdArgsRegKeySeq ks = reader.readKeySequence();
			out << L"[" << keySeqIndex << L"] name=\""
				<< ks.name << L"\" mode=" << (int)ks.mode
				<< std::endl;
			for (const auto &action : ks.actions)
				dumpAction(out, action, 3);
			keySeqIndex++;
			break;
		}
		case CmdId::DefKey: {
			auto data = reader.readDefKey();
			out << L"names=[";
			for (size_t j = 0; j < data.names.size(); ++j) {
				if (j > 0) out << L", ";
				out << L"\"" << data.names[j] << L"\"";
			}
			out << L"] scancodes=[";
			for (size_t j = 0; j < data.scanCodes.size(); ++j) {
				if (j > 0) out << L", ";
				out << L"0x" << std::hex << data.scanCodes[j].scan
					<< std::dec;
				if (data.scanCodes[j].flags)
					out << L"(flags=" << data.scanCodes[j].flags
						<< L")";
			}
			out << L"]";
			break;
		}
		case CmdId::DefMod: {
			auto data = reader.readDefMod();
			out << L"mod=\"" << data.modifierName << L"\" keys=[";
			for (size_t j = 0; j < data.keyNames.size(); ++j) {
				if (j > 0) out << L", ";
				out << L"\"" << data.keyNames[j] << L"\"";
			}
			out << L"]";
			break;
		}
		case CmdId::DefSync: {
			auto data = reader.readDefSync();
			out << L"scancodes=[";
			for (size_t j = 0; j < data.scanCodes.size(); ++j) {
				if (j > 0) out << L", ";
				out << L"0x" << std::hex << data.scanCodes[j].scan
					<< std::dec;
			}
			out << L"]";
			break;
		}
		case CmdId::DefAlias: {
			auto data = reader.readDefAlias();
			out << L"alias=\"" << data.aliasName
				<< L"\" key=\"" << data.keyName << L"\"";
			break;
		}
		case CmdId::DefSubst: {
			auto data = reader.readDefSubst();
			out << L"lhs=[...] rhs=@" << data.rhsKeySeqIdx;
			break;
		}
		case CmdId::DefOption: {
			auto data = reader.readDefOption();
			out << L"option=\"" << data.optionName << L"\"";
			out << L" value=\"" << data.value << L"\"";
			break;
		}
		case CmdId::DefSymbol: {
			auto data = reader.readDefSymbol();
			out << L"symbol=\"" << data.symbolName << L"\"";
			break;
		}
		case CmdId::DefKeymap: {
			auto data = reader.readDefKeymap();
			out << L"scope=" << keymapScopeToString(data.scope)
				<< L" keyword=\"" << data.keyword
				<< L"\" name=\"" << data.name << L"\"";
			if (!data.windowClassName.empty()) {
				out << L" class=";
				outputRegexp(out, data.windowClassName);
			}
			if (!data.windowOp.empty())
				out << L" op=" << data.windowOp;
			if (!data.windowTitleName.empty()) {
				out << L" title=";
				outputRegexp(out, data.windowTitleName);
			}
			if (!data.parentName.empty())
				out << L" parent=\"" << data.parentName << L"\"";
			if (data.defaultKeySeqIdx >= 0)
				out << L" default=@" << data.defaultKeySeqIdx;
			break;
		}
		case CmdId::AssignKey: {
			auto data = reader.readAssignKey();
			out << L"lhs=[";
			for (size_t j = 0; j < data.lhsKeys.size(); ++j) {
				if (j > 0) out << L", ";
				out << L"\"" << data.lhsKeys[j].keyName << L"\"";
			}
			out << L"] rhs=@" << data.rhsKeySeqIdx;
			break;
		}
		case CmdId::AssignEvent: {
			auto data = reader.readAssignEvent();
			out << L"event=\"" << data.eventName
				<< L"\" rhs=@" << data.rhsKeySeqIdx;
			break;
		}
		case CmdId::AssignMod: {
			auto data = reader.readAssignMod();
			out << L"main=\"" << data.mainModifierName
				<< L"\" op=\"" << data.op << L"\" keys=[";
			for (size_t j = 0; j < data.keys.size(); ++j) {
				if (j > 0) out << L", ";
				if (!data.keys[j].assignMode.empty())
					out << data.keys[j].assignMode;
				out << L"\"" << data.keys[j].keyName << L"\"";
			}
			out << L"]";
			break;
		}
		case CmdId::EndKeymap:
		case CmdId::Reset:
		case CmdId::Commit:
			break;
		}
		out << std::endl;
		cmdIndex++;
	}
}
