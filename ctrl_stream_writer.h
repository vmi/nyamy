//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ctrl_stream_writer.h


#ifndef _CTRL_STREAM_WRITER_H
#  define _CTRL_STREAM_WRITER_H


#  include "ctrl_stream.h"
#  include <ostream>


//=============================================================================
// CtrlStreamWriter - writes control commands to an output stream
//=============================================================================

class CtrlStreamWriter
{
public:
	explicit CtrlStreamWriter(std::ostream &out);

	/// Send a reload request with the given symbol set
	void writeReload(const Symbols &syms);

	/// Send a quit request
	void writeQuit();

private:
	std::ostream &m_out;

	// Primitive writers (little-endian, same convention as CmdStreamWriter)
	void writeU8(uint8_t v);
	void writeU16(uint16_t v);
	void writeString(const tstringi &s);
};


#endif // !_CTRL_STREAM_WRITER_H
