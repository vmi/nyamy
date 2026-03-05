//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ctrl_stream_reader.cpp


#include "../misc.h"
#include "ctrl_stream_reader.h"
#include "../errormessage.h"


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

Symbols CtrlStreamReader::readReload()
{
	Symbols syms;
	uint16_t count = readU16();
	for (uint16_t i = 0; i < count; ++i)
		syms.insert(readString());
	return syms;
}
