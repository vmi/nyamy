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


void CtrlStreamWriter::writeString(const wstringi &s)
{
	uint16_t len = static_cast<uint16_t>(s.size());
	writeU16(len);
	for (size_t i = 0; i < len; ++i) {
		wchar_t ch = s[i];
		writeU8(static_cast<uint8_t>(ch & 0xFF));
		writeU8(static_cast<uint8_t>((ch >> 8) & 0xFF));
	}
}


//=============================================================================
// CtrlStreamWriter - command writers
//=============================================================================

void CtrlStreamWriter::writeStart(const Symbols &syms)
{
	writeU8(static_cast<uint8_t>(CtrlId::Start));
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


void CtrlStreamWriter::writeExecUserFunc(const wstringi &funcName,
                                         const std::vector<FuncArg> &args,
                                         const TriggerInfo &ctx)
{
	writeU8(static_cast<uint8_t>(CtrlId::ExecUserFunc));
	writeString(funcName);
	writeU16(static_cast<uint16_t>(args.size()));
	for (const auto &arg : args) {
		if (std::holds_alternative<int64_t>(arg)) {
			writeU8(0x00);
			uint64_t v = static_cast<uint64_t>(std::get<int64_t>(arg));
			for (int i = 0; i < 8; ++i)
				writeU8(static_cast<uint8_t>((v >> (8 * i)) & 0xFF));
		} else {
			writeU8(0x01);
			writeString(wstringi(std::get<std::wstring>(arg)));
		}
	}
	writeU8(ctx.scanCode);
	writeU8(ctx.extended ? 1 : 0);
	writeString(wstringi(ctx.windowClass));
	writeString(wstringi(ctx.windowTitle));
	m_out.flush();
}
