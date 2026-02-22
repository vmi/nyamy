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
		throw ErrorMessage() << _T("unexpected end of command stream");
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


tstringi CmdStreamReader::readString()
{
	uint16_t len = readU16();
	tstringi s;
	s.reserve(len);
	for (uint16_t i = 0; i < len; ++i) {
		uint8_t lo = readU8();
		uint8_t hi = readU8();
		s.push_back(static_cast<_TCHAR>(lo | (hi << 8)));
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


CmdArgument CmdStreamReader::readArgument()
{
	CmdArgument arg;
	arg.type = static_cast<CmdArgument::Type>(readU8());
	switch (arg.type) {
	case CmdArgument::String:
	case CmdArgument::Regexp:
		arg.stringValue = readString();
		break;
	case CmdArgument::Number:
		arg.numberValue = readI32();
		arg.stringValue = readString();
		break;
	case CmdArgument::KeySeqIdx:
		arg.keySeqIndex = readU32();
		break;
	case CmdArgument::ModSeq:
		arg.modifierValue = readModifier();
		break;
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


CmdKeySequence CmdStreamReader::readKeySequence()
{
	CmdKeySequence ks;
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

CmdKeySequence CmdStreamReader::readDefKeySeq()
{
	return readKeySequence();
}


CmdDefKeyData CmdStreamReader::readDefKey()
{
	CmdDefKeyData data;
	uint32_t nameCount = readU32();
	for (uint32_t i = 0; i < nameCount; ++i)
		data.names.push_back(readString());
	uint32_t scCount = readU32();
	for (uint32_t i = 0; i < scCount; ++i)
		data.scanCodes.push_back(readScanCode());
	return data;
}


CmdDefModifierData CmdStreamReader::readDefModifier()
{
	CmdDefModifierData data;
	data.modifierName = readString();
	uint32_t count = readU32();
	for (uint32_t i = 0; i < count; ++i)
		data.keyNames.push_back(readString());
	return data;
}


CmdDefSyncData CmdStreamReader::readDefSync()
{
	CmdDefSyncData data;
	uint32_t count = readU32();
	for (uint32_t i = 0; i < count; ++i)
		data.scanCodes.push_back(readScanCode());
	return data;
}


CmdDefAliasData CmdStreamReader::readDefAlias()
{
	CmdDefAliasData data;
	data.aliasName = readString();
	data.keyName = readString();
	return data;
}


CmdDefSubstituteData CmdStreamReader::readDefSubstitute()
{
	CmdDefSubstituteData data;
	uint32_t count = readU32();
	for (uint32_t i = 0; i < count; ++i)
		data.lhsKeys.push_back(readModifiedKey());
	data.rhsKeySeqIdx = readU32();
	return data;
}


CmdDefOptionData CmdStreamReader::readDefOption()
{
	CmdDefOptionData data;
	data.optionName = readString();
	data.qualifier = readString();
	data.value = readString();
	return data;
}


CmdDefSymbolData CmdStreamReader::readDefSymbol()
{
	CmdDefSymbolData data;
	data.symbolName = readString();
	return data;
}


CmdKeymapDefData CmdStreamReader::readKeymapDef()
{
	CmdKeymapDefData data;
	data.keyword = readString();
	data.name = readString();
	data.windowClassName = readString();
	data.windowTitleName = readString();
	data.windowOp = readString();
	data.parentName = readString();
	data.defaultKeySeqIdx = readI32();
	return data;
}


CmdKeyAssignData CmdStreamReader::readKeyAssign()
{
	CmdKeyAssignData data;
	uint32_t count = readU32();
	for (uint32_t i = 0; i < count; ++i)
		data.lhsKeys.push_back(readModifiedKey());
	data.rhsKeySeqIdx = readU32();
	return data;
}


CmdKeyDefaultModData CmdStreamReader::readKeyDefaultMod()
{
	CmdKeyDefaultModData data;
	data.assignMod = readModifier();
	data.keySeqMod = readModifier();
	return data;
}


CmdEventAssignData CmdStreamReader::readEventAssign()
{
	CmdEventAssignData data;
	data.eventName = readString();
	data.rhsKeySeqIdx = readU32();
	return data;
}


CmdModAssignData CmdStreamReader::readModAssign()
{
	CmdModAssignData data;
	uint32_t prefCount = readU32();
	for (uint32_t i = 0; i < prefCount; ++i) {
		CmdModAssignData::PrefixMod pm;
		pm.assignMode = readString();
		pm.modifierName = readString();
		data.prefixes.push_back(pm);
	}
	data.mainModifierName = readString();
	data.op = readString();
	uint32_t keyCount = readU32();
	for (uint32_t i = 0; i < keyCount; ++i) {
		CmdModAssignData::KeyEntry ke;
		ke.assignMode = readString();
		ke.keyName = readString();
		data.keys.push_back(ke);
	}
	return data;
}


CmdKeySeqDefData CmdStreamReader::readKeySeqDef()
{
	CmdKeySeqDefData data;
	data.keySeqIdx = readU32();
	return data;
}


//=============================================================================
// Dump helpers
//=============================================================================

static const struct {
	Modifier::Type type;
	const _TCHAR *name;
} g_modNameTable[] = {
	{ Modifier::Type_Shift,          _T("S-") },
	{ Modifier::Type_Alt,            _T("A-") },
	{ Modifier::Type_Control,        _T("C-") },
	{ Modifier::Type_Windows,        _T("W-") },
	{ Modifier::Type_Up,             _T("U-") },
	{ Modifier::Type_Down,           _T("D-") },
	{ Modifier::Type_Repeat,         _T("R-") },
	{ Modifier::Type_ImeLock,        _T("IL-") },
	{ Modifier::Type_ImeComp,        _T("IC-") },
	{ Modifier::Type_NumLock,        _T("NL-") },
	{ Modifier::Type_CapsLock,       _T("CL-") },
	{ Modifier::Type_ScrollLock,     _T("SL-") },
	{ Modifier::Type_KanaLock,       _T("KL-") },
	{ Modifier::Type_Maximized,      _T("MAX-") },
	{ Modifier::Type_Minimized,      _T("MIN-") },
	{ Modifier::Type_MdiMaximized,   _T("MMAX-") },
	{ Modifier::Type_MdiMinimized,   _T("MMIN-") },
	{ Modifier::Type_Touchpad,       _T("T-") },
	{ Modifier::Type_TouchpadSticky, _T("TS-") },
	{ Modifier::Type_Mod0,           _T("M0-") },
	{ Modifier::Type_Mod1,           _T("M1-") },
	{ Modifier::Type_Mod2,           _T("M2-") },
	{ Modifier::Type_Mod3,           _T("M3-") },
	{ Modifier::Type_Mod4,           _T("M4-") },
	{ Modifier::Type_Mod5,           _T("M5-") },
	{ Modifier::Type_Mod6,           _T("M6-") },
	{ Modifier::Type_Mod7,           _T("M7-") },
	{ Modifier::Type_Mod8,           _T("M8-") },
	{ Modifier::Type_Mod9,           _T("M9-") },
	{ Modifier::Type_Lock0,          _T("L0-") },
	{ Modifier::Type_Lock1,          _T("L1-") },
	{ Modifier::Type_Lock2,          _T("L2-") },
	{ Modifier::Type_Lock3,          _T("L3-") },
	{ Modifier::Type_Lock4,          _T("L4-") },
	{ Modifier::Type_Lock5,          _T("L5-") },
	{ Modifier::Type_Lock6,          _T("L6-") },
	{ Modifier::Type_Lock7,          _T("L7-") },
	{ Modifier::Type_Lock8,          _T("L8-") },
	{ Modifier::Type_Lock9,          _T("L9-") },
};


void CmdStreamReader::dumpModifier(tostream &out, const CmdModifier &mod)
{
	out << _T("{");
	bool first = true;
	for (size_t i = 0; i < NUMBER_OF(g_modNameTable); ++i) {
		uint64_t bit = static_cast<uint64_t>(1) << g_modNameTable[i].type;
		if (mod.dontcares & bit) {
			if (!first) out << _T(" ");
			out << _T("*") << g_modNameTable[i].name;
			first = false;
		} else if (mod.modifiers & bit) {
			if (!first) out << _T(" ");
			out << g_modNameTable[i].name;
			first = false;
		}
	}
	out << _T("}");
}


void CmdStreamReader::dumpArgument(tostream &out, const CmdArgument &arg)
{
	switch (arg.type) {
	case CmdArgument::String:
		out << _T("\"") << arg.stringValue << _T("\"");
		break;
	case CmdArgument::Number:
		out << arg.numberValue;
		break;
	case CmdArgument::Regexp:
		out << _T("/") << arg.stringValue << _T("/");
		break;
	case CmdArgument::KeySeqIdx:
		out << _T("@") << arg.keySeqIndex;
		break;
	case CmdArgument::ModSeq:
		out << _T("mod");
		dumpModifier(out, arg.modifierValue);
		break;
	}
}


void CmdStreamReader::dumpAction(tostream &out, const CmdAction &action,
								 int indent)
{
	for (int i = 0; i < indent; ++i)
		out << _T("  ");

	switch (action.type) {
	case CmdAction::Key:
		out << _T("KEY mod=");
		dumpModifier(out, action.modifier);
		out << _T(" \"") << action.name << _T("\"");
		break;
	case CmdAction::KeySeqRef:
		out << _T("REF mod=");
		dumpModifier(out, action.modifier);
		out << _T(" $") << action.name;
		break;
	case CmdAction::FuncCall:
		out << _T("FUNC mod=");
		dumpModifier(out, action.modifier);
		out << _T(" &") << action.name;
		if (!action.arguments.empty()) {
			out << _T("(");
			for (size_t i = 0; i < action.arguments.size(); ++i) {
				if (i > 0) out << _T(", ");
				dumpArgument(out, action.arguments[i]);
			}
			out << _T(")");
		}
		break;
	case CmdAction::SubSeq:
		out << _T("SUBSEQ mod=");
		dumpModifier(out, action.modifier);
		out << std::endl;
		for (const auto &sub : action.subActions)
			dumpAction(out, sub, indent + 1);
		return; // no newline after sub
	}
	out << std::endl;
}


static const _TCHAR *cmdIdToString(CmdId id)
{
	switch (id) {
	case CmdId::DefKeySeq:    return _T("DefKeySeq");
	case CmdId::DefKey:       return _T("DefKey");
	case CmdId::DefModifier:  return _T("DefModifier");
	case CmdId::DefSync:      return _T("DefSync");
	case CmdId::DefAlias:     return _T("DefAlias");
	case CmdId::DefSubstitute:return _T("DefSubstitute");
	case CmdId::DefOption:    return _T("DefOption");
	case CmdId::DefSymbol:    return _T("DefSymbol");
	case CmdId::KeymapDef:    return _T("KeymapDef");
	case CmdId::KeyAssign:    return _T("KeyAssign");
	case CmdId::KeyDefaultMod:return _T("KeyDefaultMod");
	case CmdId::EventAssign:  return _T("EventAssign");
	case CmdId::ModAssign:    return _T("ModAssign");
	case CmdId::KeySeqDef:    return _T("KeySeqDef");
	case CmdId::Reset:        return _T("Reset");
	case CmdId::Commit:       return _T("Commit");
	default:                  return _T("???");
	}
}


void CmdStreamReader::dump(std::istream &in, tostream &out)
{
	CmdStreamReader reader(in);
	CmdId cmdId;
	uint32_t cmdIndex = 0;
	uint32_t keySeqIndex = 0;

	out << _T("; Command stream dump") << std::endl << std::endl;

	while (reader.readNext(cmdId)) {
		out << _T("  ")
			<< std::setw(4) << std::setfill(_T('0')) << cmdIndex
			<< _T(": ");
		out << std::setw(14) << std::setfill(_T(' ')) << std::left
			<< cmdIdToString(cmdId);

		switch (cmdId) {
		case CmdId::DefKeySeq: {
			CmdKeySequence ks = reader.readKeySequence();
			out << _T("[") << keySeqIndex << _T("] name=\"")
				<< ks.name << _T("\" mode=") << (int)ks.mode
				<< std::endl;
			for (const auto &action : ks.actions)
				dumpAction(out, action, 3);
			keySeqIndex++;
			break;
		}
		case CmdId::DefKey: {
			auto data = reader.readDefKey();
			out << _T("names=[");
			for (size_t j = 0; j < data.names.size(); ++j) {
				if (j > 0) out << _T(", ");
				out << _T("\"") << data.names[j] << _T("\"");
			}
			out << _T("] scancodes=[");
			for (size_t j = 0; j < data.scanCodes.size(); ++j) {
				if (j > 0) out << _T(", ");
				out << _T("0x") << std::hex << data.scanCodes[j].scan
					<< std::dec;
				if (data.scanCodes[j].flags)
					out << _T("(flags=") << data.scanCodes[j].flags
						<< _T(")");
			}
			out << _T("]");
			break;
		}
		case CmdId::DefModifier: {
			auto data = reader.readDefModifier();
			out << _T("mod=\"") << data.modifierName << _T("\" keys=[");
			for (size_t j = 0; j < data.keyNames.size(); ++j) {
				if (j > 0) out << _T(", ");
				out << _T("\"") << data.keyNames[j] << _T("\"");
			}
			out << _T("]");
			break;
		}
		case CmdId::DefSync: {
			auto data = reader.readDefSync();
			out << _T("scancodes=[");
			for (size_t j = 0; j < data.scanCodes.size(); ++j) {
				if (j > 0) out << _T(", ");
				out << _T("0x") << std::hex << data.scanCodes[j].scan
					<< std::dec;
			}
			out << _T("]");
			break;
		}
		case CmdId::DefAlias: {
			auto data = reader.readDefAlias();
			out << _T("alias=\"") << data.aliasName
				<< _T("\" key=\"") << data.keyName << _T("\"");
			break;
		}
		case CmdId::DefSubstitute: {
			auto data = reader.readDefSubstitute();
			out << _T("lhs=[...] rhs=@") << data.rhsKeySeqIdx;
			break;
		}
		case CmdId::DefOption: {
			auto data = reader.readDefOption();
			out << _T("option=\"") << data.optionName << _T("\"");
			if (!data.qualifier.empty())
				out << _T(" qual=\"") << data.qualifier << _T("\"");
			out << _T(" value=\"") << data.value << _T("\"");
			break;
		}
		case CmdId::DefSymbol: {
			auto data = reader.readDefSymbol();
			out << _T("symbol=\"") << data.symbolName << _T("\"");
			break;
		}
		case CmdId::KeymapDef: {
			auto data = reader.readKeymapDef();
			out << _T("keyword=\"") << data.keyword
				<< _T("\" name=\"") << data.name << _T("\"");
			if (!data.windowClassName.empty())
				out << _T(" class=/") << data.windowClassName << _T("/");
			if (!data.windowOp.empty())
				out << _T(" op=") << data.windowOp;
			if (!data.windowTitleName.empty())
				out << _T(" title=/") << data.windowTitleName << _T("/");
			if (!data.parentName.empty())
				out << _T(" parent=\"") << data.parentName << _T("\"");
			if (data.defaultKeySeqIdx >= 0)
				out << _T(" default=@") << data.defaultKeySeqIdx;
			break;
		}
		case CmdId::KeyAssign: {
			auto data = reader.readKeyAssign();
			out << _T("lhs=[");
			for (size_t j = 0; j < data.lhsKeys.size(); ++j) {
				if (j > 0) out << _T(", ");
				out << _T("\"") << data.lhsKeys[j].keyName << _T("\"");
			}
			out << _T("] rhs=@") << data.rhsKeySeqIdx;
			break;
		}
		case CmdId::KeyDefaultMod: {
			auto data = reader.readKeyDefaultMod();
			out << _T("assign=... keyseq=...");
			break;
		}
		case CmdId::EventAssign: {
			auto data = reader.readEventAssign();
			out << _T("event=\"") << data.eventName
				<< _T("\" rhs=@") << data.rhsKeySeqIdx;
			break;
		}
		case CmdId::ModAssign: {
			auto data = reader.readModAssign();
			out << _T("main=\"") << data.mainModifierName
				<< _T("\" op=\"") << data.op << _T("\" keys=[");
			for (size_t j = 0; j < data.keys.size(); ++j) {
				if (j > 0) out << _T(", ");
				if (!data.keys[j].assignMode.empty())
					out << data.keys[j].assignMode;
				out << _T("\"") << data.keys[j].keyName << _T("\"");
			}
			out << _T("]");
			break;
		}
		case CmdId::KeySeqDef: {
			auto data = reader.readKeySeqDef();
			out << _T("idx=") << data.keySeqIdx;
			break;
		}
		case CmdId::Reset:
			break;
		case CmdId::Commit:
			break;
		}
		out << std::endl;
		cmdIndex++;
	}
}
