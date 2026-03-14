//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ctrl_stream.h


#ifndef _CTRL_STREAM_H
#  define _CTRL_STREAM_H


#  include "symbols.h"
#  include <cstdint>


/// Control command IDs (yamy -> scripter direction)
enum class CtrlId : uint8_t {
	Start = 0x01,  ///< (Re)compile with the given symbols; sent on every scripter startup
	Quit  = 0xFF,  ///< Terminate scripter
};


#endif // !_CTRL_STREAM_H
