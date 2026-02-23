//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ctrl_stream_writer.cpp


#include "misc.h"
#include "ctrl_stream_writer.h"


//=============================================================================
// CtrlStreamWriter - primitive helpers
//=============================================================================

CtrlStreamWriter::CtrlStreamWriter(std::ostream &out) : m_out(out) {}


void CtrlStreamWriter::writeU8(uint8_t v)
{
	m_out.put(static_cast<char>(v));
}


void CtrlStreamWriter::writeU16(uint16_t v)
{
	writeU8(static_cast<uint8_t>(v & 0xFF));
	writeU8(static_cast<uint8_t>((v >> 8) & 0xFF));
}


void CtrlStreamWriter::writeString(const tstringi &s)
{
	uint16_t len = static_cast<uint16_t>(s.size());
	writeU16(len);
	for (size_t i = 0; i < len; ++i) {
		_TCHAR ch = s[i];
		writeU8(static_cast<uint8_t>(ch & 0xFF));
		writeU8(static_cast<uint8_t>((ch >> 8) & 0xFF));
	}
}


//=============================================================================
// CtrlStreamWriter - command writers
//=============================================================================

void CtrlStreamWriter::writeReload(const Symbols &syms)
{
	writeU8(static_cast<uint8_t>(CtrlId::Reload));
	writeU16(static_cast<uint16_t>(syms.size()));
	for (const auto &sym : syms)
		writeString(sym);
	m_out.flush();
}


void CtrlStreamWriter::writeQuit()
{
	writeU8(static_cast<uint8_t>(CtrlId::Quit));
	m_out.flush();
}
