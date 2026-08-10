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

void CtrlStreamWriter::writeStart(const wstringi &configName,
                                  const wstringi &configPath,
                                  const Symbols  &syms,
                                  LogLevel        logLevel)
{
	writeU8(static_cast<uint8_t>(CtrlId::Start));
	writeString(configName);
	writeString(configPath);
	writeU16(static_cast<uint16_t>(syms.size()));
	for (const auto &sym : syms)
		writeString(sym);
	writeU8(static_cast<uint8_t>(logLevel));
	m_out.flush();
}


void CtrlStreamWriter::writeSetLogLevel(LogLevel logLevel)
{
	writeU8(static_cast<uint8_t>(CtrlId::SetLogLevel));
	writeU8(static_cast<uint8_t>(logLevel));
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
		std::visit(overloaded{
			[&](const FuncArgNumber& a) {
				writeU8(FuncArgTag_Number);
				uint64_t v = static_cast<uint64_t>(a);
				for (int i = 0; i < 8; ++i)
					writeU8(static_cast<uint8_t>((v >> (8 * i)) & 0xFF));
			},
			[&](const FuncArgString& a) {
				writeU8(FuncArgTag_String);
				writeString(a);
			},
			[](auto&&) {},
		}, arg);
	}
	writeU8(ctx.scanCode);
	writeU8(ctx.extended ? 1 : 0);
	writeString(wstringi(ctx.windowClass));
	writeString(wstringi(ctx.windowTitle));
	m_out.flush();
}
