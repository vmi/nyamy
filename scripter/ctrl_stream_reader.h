//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ctrl_stream_reader.h


#ifndef _CTRL_STREAM_READER_H
#  define _CTRL_STREAM_READER_H


#  include "../ctrl_stream.h"
#  include <istream>
#  include <vector>


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
	Symbols readStart();

	/// Read the payload of an ExecUserFunc command (call after readNext returns ExecUserFunc)
	struct ExecUserFuncData {
		wstringi              name;
		std::vector<FuncArg>  args;
		TriggerInfo           context;
	};
	ExecUserFuncData readExecUserFunc();

private:
	std::istream &m_in;

	// Primitive readers (little-endian, same convention as CmdStreamReader)
	uint8_t readU8();
	uint16_t readU16();
	wstringi readString();
};


#endif // !_CTRL_STREAM_READER_H
