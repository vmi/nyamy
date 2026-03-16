//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// cmd_stream.cpp


#include "misc.h"

#include "cmd_stream_reader.h"
#include "errormessage.h"
#include "keyboard.h"

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
	case CmdId::BeginKeymap:     return readBeginKeymap();
	case CmdId::AssignKey:     return readAssignKey();
	case CmdId::AssignEvent:   return readAssignEvent();
	case CmdId::AssignMod:     return readAssignMod();
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


CmdModifier CmdStreamReader::readModifier()
{
	CmdModifier mod;
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
	mk.modifier = readModifier();
	mk.keyName = readString();
	return mk;
}


CmdFuncArg CmdStreamReader::readArgument()
{
	CmdFuncArg arg;
	arg.type = static_cast<CmdFuncArg::Type>(readU8());
	switch (arg.type) {
	case CmdFuncArg::String:
	case CmdFuncArg::Regexp:
		arg.stringValue = readString();
		break;
	case CmdFuncArg::Number:
		arg.numberValue = readI32();
		arg.stringValue = readString();
		break;
	case CmdFuncArg::KeySeqIdx:
		arg.keySeqIndex = readU32();
		break;
	case CmdFuncArg::ModSeq:
		arg.modifierValue = readModifier();
		break;
	case CmdFuncArg::TokenSeq: {
		uint16_t count = readU16();
		arg.tokens.resize(count);
		for (uint16_t i = 0; i < count; ++i)
			arg.tokens[i] = readString();
		break;
	}
	}
	return arg;
}


CmdAction CmdStreamReader::readAction()
{
	CmdAction action;
	action.type = static_cast<CmdAction::Type>(readU8());
	action.modifier = readModifier();
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


CmdArgsBeginKeymap CmdStreamReader::readBeginKeymap()
{
	CmdArgsBeginKeymap data;
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
//=============================================================================

static const struct {
	Modifier::Type type;
	const wchar_t *name;
} g_modNameTable[] = {
	{ Modifier::Type_Shift,          L"S-" },
	{ Modifier::Type_Alt,            L"A-" },
	{ Modifier::Type_Control,        L"C-" },
	{ Modifier::Type_Windows,        L"W-" },
	{ Modifier::Type_Up,             L"U-" },
	{ Modifier::Type_Down,           L"D-" },
	{ Modifier::Type_Repeat,         L"R-" },
	{ Modifier::Type_ImeLock,        L"IL-" },
	{ Modifier::Type_ImeComp,        L"IC-" },
	{ Modifier::Type_NumLock,        L"NL-" },
	{ Modifier::Type_CapsLock,       L"CL-" },
	{ Modifier::Type_ScrollLock,     L"SL-" },
	{ Modifier::Type_KanaLock,       L"KL-" },
	{ Modifier::Type_Maximized,      L"MAX-" },
	{ Modifier::Type_Minimized,      L"MIN-" },
	{ Modifier::Type_MdiMaximized,   L"MMAX-" },
	{ Modifier::Type_MdiMinimized,   L"MMIN-" },
	{ Modifier::Type_Mod0,           L"M0-" },
	{ Modifier::Type_Mod1,           L"M1-" },
	{ Modifier::Type_Mod2,           L"M2-" },
	{ Modifier::Type_Mod3,           L"M3-" },
	{ Modifier::Type_Mod4,           L"M4-" },
	{ Modifier::Type_Mod5,           L"M5-" },
	{ Modifier::Type_Mod6,           L"M6-" },
	{ Modifier::Type_Mod7,           L"M7-" },
	{ Modifier::Type_Mod8,           L"M8-" },
	{ Modifier::Type_Mod9,           L"M9-" },
	{ Modifier::Type_Lock0,          L"L0-" },
	{ Modifier::Type_Lock1,          L"L1-" },
	{ Modifier::Type_Lock2,          L"L2-" },
	{ Modifier::Type_Lock3,          L"L3-" },
	{ Modifier::Type_Lock4,          L"L4-" },
	{ Modifier::Type_Lock5,          L"L5-" },
	{ Modifier::Type_Lock6,          L"L6-" },
	{ Modifier::Type_Lock7,          L"L7-" },
	{ Modifier::Type_Lock8,          L"L8-" },
	{ Modifier::Type_Lock9,          L"L9-" },
};


void CmdStreamReader::dumpModifier(std::wostream &out, const CmdModifier &mod)
{
	out << L"{";
	bool first = true;
	for (size_t i = 0; i < NUMBER_OF(g_modNameTable); ++i) {
		uint64_t bit = static_cast<uint64_t>(1) << g_modNameTable[i].type;
		if (mod.dontcares & bit) {
			if (!first) out << L" ";
			out << L"*" << g_modNameTable[i].name;
			first = false;
		} else if (mod.modifiers & bit) {
			if (!first) out << L" ";
			out << g_modNameTable[i].name;
			first = false;
		}
	}
	out << L"}";
}


void CmdStreamReader::dumpArgument(std::wostream &out, const CmdFuncArg &arg)
{
	switch (arg.type) {
	case CmdFuncArg::String:
		out << L"\"" << arg.stringValue << L"\"";
		break;
	case CmdFuncArg::Number:
		out << arg.numberValue;
		break;
	case CmdFuncArg::Regexp:
		out << L"/" << arg.stringValue << L"/";
		break;
	case CmdFuncArg::KeySeqIdx:
		out << L"@" << arg.keySeqIndex;
		break;
	case CmdFuncArg::ModSeq:
		out << L"mod";
		dumpModifier(out, arg.modifierValue);
		break;
	}
}


void CmdStreamReader::dumpAction(std::wostream &out, const CmdAction &action,
								 int indent)
{
	for (int i = 0; i < indent; ++i)
		out << L"  ";

	switch (action.type) {
	case CmdAction::Key:
		out << L"KEY mod=";
		dumpModifier(out, action.modifier);
		out << L" \"" << action.name << L"\"";
		break;
	case CmdAction::KeySeqRef:
		out << L"REF mod=";
		dumpModifier(out, action.modifier);
		out << L" $" << action.name;
		break;
	case CmdAction::FuncCall:
		out << L"FUNC mod=";
		dumpModifier(out, action.modifier);
		out << L" &" << action.name;
		if (!action.arguments.empty()) {
			out << L"(";
			for (size_t i = 0; i < action.arguments.size(); ++i) {
				if (i > 0) out << L", ";
				dumpArgument(out, action.arguments[i]);
			}
			out << L")";
		}
		break;
	case CmdAction::SubSeq:
		out << L"SUBSEQ mod=";
		dumpModifier(out, action.modifier);
		out << std::endl;
		for (const auto &sub : action.subActions)
			dumpAction(out, sub, indent + 1);
		return; // no newline after sub
	}
	out << std::endl;
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
	case CmdId::BeginKeymap:    return L"BeginKeymap";
	case CmdId::AssignKey:    return L"AssignKey";
	case CmdId::AssignEvent:  return L"AssignEvent";
	case CmdId::AssignMod:    return L"AssignMod";
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
		case CmdId::BeginKeymap: {
			auto data = reader.readBeginKeymap();
			out << L"keyword=\"" << data.keyword
				<< L"\" name=\"" << data.name << L"\"";
			if (!data.windowClassName.empty())
				out << L" class=/" << data.windowClassName << L"/";
			if (!data.windowOp.empty())
				out << L" op=" << data.windowOp;
			if (!data.windowTitleName.empty())
				out << L" title=/" << data.windowTitleName << L"/";
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
		case CmdId::Commit:
			break;
		}
		out << std::endl;
		cmdIndex++;
	}
}
