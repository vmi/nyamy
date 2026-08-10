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


CtrlArgsExecUserFunc CtrlStreamReader::readExecUserFunc()
{
	CtrlArgsExecUserFunc data;
	data.name = readString();
	uint16_t argCount = readU16();
	for (uint16_t i = 0; i < argCount; ++i) {
		NYsFuncArg e;
		e.type = static_cast<NYsType>(readU8());  // FuncArgTag values == NYsType values
		if (e.type == NYsType_Number) {
			int32_t v = 0;
			for (int j = 0; j < 8; ++j)
				v |= static_cast<int32_t>(readU8()) << (8 * j);
			e.numval = v;
		} else {
			wstringi ws = readString();
			e.str = to_UTF8(std::wstring(ws));
		}
		data.args.entries.push_back(std::move(e));
	}
	data.context.scanCode    = readU8();
	data.context.extended    = (readU8() != 0);
	data.context.windowClass = std::wstring(readString());
	data.context.windowTitle = std::wstring(readString());
	return data;
}
