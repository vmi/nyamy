//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ctrl_stream.h


#ifndef _CTRL_STREAM_H
#  define _CTRL_STREAM_H


#  include "trigger_info.h"
#  include "symbols.h"
#  include <cstdint>
#  include <variant>
#  include <vector>


/// Control command IDs (yamy -> scripter direction)
enum class CtrlId : uint8_t {
	Start       = 0x01,  ///< (Re)compile with the given symbols; sent on every scripter startup
	ExecUserFunc = 0x02, ///< Engine -> scripter: invoke user-defined function
	Quit        = 0xFF,  ///< Terminate scripter
};


/// FuncArg: function argument (numeric or string)
using FuncArg = std::variant<int64_t, std::wstring>;



#endif // !_CTRL_STREAM_H
