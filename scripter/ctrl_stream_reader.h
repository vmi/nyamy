//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ctrl_stream_reader.h


#ifndef _CTRL_STREAM_READER_H
#  define _CTRL_STREAM_READER_H


#  include "../ctrl_stream.h"
#  include "nys_types.h"
#  include <istream>
#  include <memory>
#  include <vector>


/// Payload of the Start control command (pairs with CtrlId::Start)
struct CtrlArgsStart {
	wstringi configName;  // config profile name (may be empty)
	wstringi configPath;  // config file path (may be empty)
	Symbols  symbols;
	LogLevel logLevel;    // threshold in force when nyamy launched us
};


/// Payload of the ExecUserFunc control command (pairs with CtrlId::ExecUserFunc)
struct CtrlArgsExecUserFunc {
	wstringi    name;
	NYsFuncArgs  args;     // move-only
	TriggerInfo context;

	/// Backing store for the token-sequence arguments.  NYsFuncArg::strs does
	/// not own what it points at, and the ctrl stream is read outside the
	/// callback session that owns everything else, so the lists live here -
	/// for as long as the request itself does.  unique_ptr keeps the pointers
	/// valid however this vector grows or moves.
	std::vector<std::unique_ptr<NYsStrs>> tokenSeqs;
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

	/// Read the payload of a SetLogLevel command
	LogLevel readSetLogLevel();

private:
	std::istream &m_in;

	// Primitive readers (little-endian, same convention as CmdStreamReader)
	uint8_t readU8();
	uint16_t readU16();
	uint32_t readU32();
	uint64_t readU64();
	wstringi readString();

	/// Read one function argument into o_data (token sequences are appended to
	/// its backing store).  Throws on a tag it does not know.
	NYsFuncArg readFuncArg(CtrlArgsExecUserFunc *o_data);
};


#endif // !_CTRL_STREAM_READER_H
