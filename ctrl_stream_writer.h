//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ctrl_stream_writer.h


#ifndef _CTRL_STREAM_WRITER_H
#  define _CTRL_STREAM_WRITER_H


#  include "ctrl_stream.h"
#  include <ostream>
#  include <vector>


//=============================================================================
// CtrlStreamWriter - writes control commands to an output stream
//=============================================================================

class CtrlStreamWriter
{
public:
	explicit CtrlStreamWriter(std::ostream &out);

	/// Send a start request with config name, path, and symbol set
	void writeStart(const wstringi &configName,
	                const wstringi &configPath,
	                const Symbols  &syms);

	/// Send a quit request
	void writeQuit();

	/// Send an ExecUserFunc request to scripter
	void writeExecUserFunc(const wstringi &funcName,
	                       const std::vector<FuncArg> &args,
	                       const TriggerInfo &ctx);

private:
	std::ostream &m_out;

	// Primitive writers (little-endian, same convention as CmdStreamWriter)
	void writeU8(uint8_t v);
	void writeU16(uint16_t v);
	void writeString(const wstringi &s);
};


#endif // !_CTRL_STREAM_WRITER_H
