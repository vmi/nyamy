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
	if (m_buf)
		m_buf->push_back(static_cast<char>(v));
	else
		m_out.put(static_cast<char>(v));
}


void CtrlStreamWriter::writeU16(uint16_t v)
{
	writeU8(static_cast<uint8_t>(v & 0xFF));
	writeU8(static_cast<uint8_t>((v >> 8) & 0xFF));
}


void CtrlStreamWriter::writeU32(uint32_t v)
{
	writeU8(static_cast<uint8_t>(v & 0xFF));
	writeU8(static_cast<uint8_t>((v >> 8) & 0xFF));
	writeU8(static_cast<uint8_t>((v >> 16) & 0xFF));
	writeU8(static_cast<uint8_t>((v >> 24) & 0xFF));
}


void CtrlStreamWriter::writeI32(int32_t v)
{
	writeU32(static_cast<uint32_t>(v));
}


void CtrlStreamWriter::writeU64(uint64_t v)
{
	writeU32(static_cast<uint32_t>(v & 0xFFFFFFFF));
	writeU32(static_cast<uint32_t>((v >> 32) & 0xFFFFFFFF));
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


// Same tags and layout as CmdStreamWriter::writeArgument(), so an argument
// means the same thing whichever way it travels.  The visit deliberately has
// no catch-all: a FuncArg alternative that is not handled here has to fail to
// compile rather than be dropped from the stream.
void CtrlStreamWriter::writeFuncArg(const FuncArg &arg)
{
	std::visit(overloaded{
		[&](const FuncArgString&       a) { writeU8(FuncArgTag_String);       writeString(a); },
		[&](const FuncArgNumber&       a) { writeU8(FuncArgTag_Number);       writeI32(a); },
		[&](const FuncArgRegexp&       a) { writeU8(FuncArgTag_Regexp);       writeString(a.str()); },
		[&](const FuncArgKeySeqIdx&    a) { writeU8(FuncArgTag_KeySeqIdx);    writeU32(a); },
		[&](const FuncArgModifierSpec& a) {
			writeU8(FuncArgTag_ModifierSpec);
			writeU64(a.modifiers);
			writeU64(a.dontcares);
		},
		[&](const FuncArgTokenSeq&     a) {
			writeU8(FuncArgTag_TokenSeq);
			writeU16(static_cast<uint16_t>(a.size()));
			for (const auto &t : a)
				writeString(t);
		},
	}, arg);
}


void CtrlStreamWriter::writeExecUserFunc(const wstringi &funcName,
                                         const std::vector<FuncArg> &args,
                                         const TriggerInfo &ctx)
{
	// Serialize the arguments before the count that describes them reaches the
	// stream.  The reader cannot recover if the two disagree: it would read the
	// bytes that follow as an argument and then block waiting for the rest of a
	// string that is never coming, taking every later request down with it.
	std::string payload;
	m_buf = &payload;
	for (const auto &arg : args)
		writeFuncArg(arg);
	m_buf = nullptr;

	writeU8(static_cast<uint8_t>(CtrlId::ExecUserFunc));
	writeString(funcName);
	writeU16(static_cast<uint16_t>(args.size()));
	m_out.write(payload.data(), static_cast<std::streamsize>(payload.size()));
	writeU8(ctx.scanCode);
	writeU8(ctx.extended ? 1 : 0);
	writeString(wstringi(ctx.windowClass));
	writeString(wstringi(ctx.windowTitle));
	m_out.flush();
}
