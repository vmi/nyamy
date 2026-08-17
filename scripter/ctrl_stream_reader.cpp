//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ctrl_stream_reader.cpp


#include "../misc.h"
#include "ctrl_stream_reader.h"
#include "../errormessage.h"
#include "../stringtool.h"


//=============================================================================
// CtrlStreamReader - primitive helpers
//=============================================================================

CtrlStreamReader::CtrlStreamReader(std::istream &in) : m_in(in) {}


bool CtrlStreamReader::readNext(CtrlId &ctrlId)
{
	int ch = m_in.get();
	if (ch == std::char_traits<char>::eof())
		return false;
	ctrlId = static_cast<CtrlId>(static_cast<uint8_t>(ch));
	return true;
}


uint8_t CtrlStreamReader::readU8()
{
	int ch = m_in.get();
	if (ch == std::char_traits<char>::eof())
		throw ErrorMessage() << L"unexpected end of control stream";
	return static_cast<uint8_t>(ch);
}


uint16_t CtrlStreamReader::readU16()
{
	uint16_t lo = readU8();
	uint16_t hi = readU8();
	return lo | (hi << 8);
}


uint32_t CtrlStreamReader::readU32()
{
	uint32_t lo = readU16();
	uint32_t hi = readU16();
	return lo | (hi << 16);
}


uint64_t CtrlStreamReader::readU64()
{
	uint64_t lo = readU32();
	uint64_t hi = readU32();
	return lo | (hi << 32);
}


wstringi CtrlStreamReader::readString()
{
	uint16_t len = readU16();
	wstringi s;
	s.resize(len);
	for (uint16_t i = 0; i < len; ++i) {
		uint8_t lo = readU8();
		uint8_t hi = readU8();
		s[i] = static_cast<wchar_t>(lo | (hi << 8));
	}
	return s;
}


//=============================================================================
// CtrlStreamReader - command readers
//=============================================================================

CtrlArgsStart CtrlStreamReader::readStart()
{
	CtrlArgsStart data;
	data.configName = readString();
	data.configPath = readString();
	uint16_t count = readU16();
	for (uint16_t i = 0; i < count; ++i)
		data.symbols.insert(readString());
	data.logLevel = logLevelFromByte(readU8());
	return data;
}


LogLevel CtrlStreamReader::readSetLogLevel()
{
	return logLevelFromByte(readU8());
}


// One function argument, in the layout CtrlStreamWriter::writeFuncArg() writes
// (which is the layout of the command stream, tags included).  An unknown tag
// throws: the length of what follows depends on the tag, so guessing means
// reading the rest of the frame as an argument and blocking on a string that
// never arrives.
NYsFuncArg CtrlStreamReader::readFuncArg(CtrlArgsExecUserFunc *o_data)
{
	NYsFuncArg e;
	uint8_t tag = readU8();		// FuncArgTag values == NYsType values
	switch (static_cast<FuncArgTag>(tag)) {
	case FuncArgTag_String:
	case FuncArgTag_Regexp:
		e.type = (tag == FuncArgTag_String) ? NYsType_String : NYsType_Regexp;
		e.str = to_UTF8(std::wstring(readString()));
		break;
	case FuncArgTag_Number:
		e.type = NYsType_Number;
		e.numval = static_cast<int32_t>(readU32());
		break;
	case FuncArgTag_KeySeqIdx:
		e.type = NYsType_KeySeqIdx;
		e.numval = static_cast<int64_t>(readU32());
		break;
	case FuncArgTag_ModifierSpec:
		e.type = NYsType_ModifierSpec;
		e.numval  = static_cast<int64_t>(readU64());
		e.numval2 = static_cast<int64_t>(readU64());
		break;
	case FuncArgTag_TokenSeq: {
		e.type = NYsType_TokenSeq;
		uint16_t count = readU16();
		o_data->tokenSeqs.push_back(std::make_unique<NYsStrs>());
		NYsStrs *ss = o_data->tokenSeqs.back().get();
		for (uint16_t i = 0; i < count; ++i)
			ss->strs.push_back(to_UTF8(std::wstring(readString())));
		e.strs = ss;
		break;
	}
	default:
		throw ErrorMessage()
			<< L"ExecUserFunc: unknown argument tag " << static_cast<int>(tag);
	}
	return e;
}


CtrlArgsExecUserFunc CtrlStreamReader::readExecUserFunc()
{
	CtrlArgsExecUserFunc data;
	data.name = readString();
	uint16_t argCount = readU16();
	data.tokenSeqs.reserve(argCount);
	for (uint16_t i = 0; i < argCount; ++i)
		data.args.entries.push_back(readFuncArg(&data));
	data.context.scanCode    = readU8();
	data.context.extended    = (readU8() != 0);
	data.context.windowClass = std::wstring(readString());
	data.context.windowTitle = std::wstring(readString());
	return data;
}
