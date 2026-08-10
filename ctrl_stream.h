//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ctrl_stream.h


#ifndef _CTRL_STREAM_H
#  define _CTRL_STREAM_H


#  include "log_level.h"
#  include "scripter_types.h"
#  include "trigger_info.h"
#  include "symbols.h"
#  include <cstdint>


/// Control command IDs (nyamy -> scripter direction)
enum class CtrlId : uint8_t {
	Start       = 0x01,  ///< (Re)compile with the given symbols; sent on every scripter startup
	ExecUserFunc = 0x02, ///< Engine -> scripter: invoke user-defined function
	SetLogLevel = 0x03,  ///< New log threshold (one LogLevel byte); sent when "detail" is toggled
	Quit        = 0xFF,  ///< Terminate scripter
};


//=============================================================================
// Shutdown timeouts
//
// A script that never returns cannot be interrupted: mruby offers no way into
// a running VM without a compile-time option, and a blocking call has no
// cancellation point at all.  Killing the scripter process is therefore the
// only way to stop one.
//
// Two layers do the killing, and the order between them is what these
// constants encode: the scripter terminates itself first, and nyamy's
// TerminateProcess stays reserved for a scripter that cannot - a foreign
// implementation launched through the ini "cmdLine" setting, one wedged before
// its ctrl thread started, or one held alive by a Windows Error Reporting
// dialog.
//
// nyamy's reader threads do not depend on any of this: their reads are
// overlapped and end on a stop event (ScripterManager::stopReaders).
//=============================================================================

/// Milliseconds the scripter waits for a running script after it observes Quit
/// (or ctrl-pipe EOF) before terminating itself.
/// Passed to nys_set_quit_timeout() by the scripter's main().
const uint32_t kScripterQuitTimeoutMillisec = 3000;

/// Milliseconds nyamy waits for the scripter to exit on its own after Quit.
const uint32_t kScripterQuitGraceMillisec = 5000;

/// Milliseconds nyamy waits after TerminateProcess for the process to go away.
const uint32_t kScripterKillWaitMillisec = 2000;

static_assert(kScripterQuitGraceMillisec > kScripterQuitTimeoutMillisec,
              "nyamy must give the scripter time to terminate itself first");



#endif // !_CTRL_STREAM_H
