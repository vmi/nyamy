//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ctrl_stream_reader.h


#ifndef _CTRL_STREAM_READER_H
#  define _CTRL_STREAM_READER_H


#  include "../ctrl_stream.h"
#  include "nys_types.h"
#  include <istream>


/// Payload of the Start control command (pairs with CtrlId::Start)
struct CtrlArgsStart {
	wstringi configName;  // config profile name (may be empty)
	wstringi configPath;  // config file path (may be empty)
	Symbols  symbols;
};


/// Payload of the ExecUserFunc control command (pairs with CtrlId::ExecUserFunc)
struct CtrlArgsExecUserFunc {
	wstringi    name;
	NYsFuncArgs  args;     // move-only
	TriggerInfo context;
};


//=============================================================================
// CtrlStreamReader - reads control commands from an input stream
//=============================================================================

class CtrlStreamReader
{
public:
	explicit CtrlStreamReader(std::istream &in);

	/// Read the next control command ID. Returns false on EOF.
	bool readNext(CtrlId &ctrlId);

	/// Read the payload of a Start command (call after readNext returns Start)
	CtrlArgsStart readStart();

	/// Read the payload of an ExecUserFunc command (call after readNext returns ExecUserFunc)
	CtrlArgsExecUserFunc readExecUserFunc();

private:
	std::istream &m_in;

	// Primitive readers (little-endian, same convention as CmdStreamReader)
	uint8_t readU8();
	uint16_t readU16();
	wstringi readString();
};


#endif // !_CTRL_STREAM_READER_H
