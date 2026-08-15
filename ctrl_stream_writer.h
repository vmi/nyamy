//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ctrl_stream_writer.h


#ifndef _CTRL_STREAM_WRITER_H
#  define _CTRL_STREAM_WRITER_H


#  include "ctrl_stream.h"
#  include <ostream>
#  include <string>
#  include <vector>


//=============================================================================
// CtrlStreamWriter - writes control commands to an output stream
//=============================================================================

class CtrlStreamWriter
{
public:
	explicit CtrlStreamWriter(std::ostream &out);

	/// Send a start request with config name, path, symbol set and log threshold
	void writeStart(const wstringi &configName,
	                const wstringi &configPath,
	                const Symbols  &syms,
	                LogLevel        logLevel);

	/// Send a new log threshold
	void writeSetLogLevel(LogLevel logLevel);

	/// Send a quit request
	void writeQuit();

	/// Send an ExecUserFunc request to scripter
	void writeExecUserFunc(const wstringi &funcName,
	                       const std::vector<FuncArg> &args,
	                       const TriggerInfo &ctx);

private:
	std::ostream &m_out;

	/// While set, the primitives append here instead of writing to the stream.
	/// writeExecUserFunc() uses it to serialize the arguments before the count
	/// that describes them is committed to the stream.
	std::string *m_buf = nullptr;

	// Primitive writers (little-endian, same convention as CmdStreamWriter)
	void writeU8(uint8_t v);
	void writeU16(uint16_t v);
	void writeU32(uint32_t v);
	void writeI32(int32_t v);
	void writeU64(uint64_t v);
	void writeString(const wstringi &s);
	void writeFuncArg(const FuncArg &arg);
};


#endif // !_CTRL_STREAM_WRITER_H
