//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ctrl_stream.h


#ifndef _CTRL_STREAM_H
#  define _CTRL_STREAM_H


#  include "scripter_types.h"
#  include "trigger_info.h"
#  include "symbols.h"
#  include <cstdint>


/// Control command IDs (nyamy -> scripter direction)
enum class CtrlId : uint8_t {
	Start       = 0x01,  ///< (Re)compile with the given symbols; sent on every scripter startup
	ExecUserFunc = 0x02, ///< Engine -> scripter: invoke user-defined function
	Quit        = 0xFF,  ///< Terminate scripter
};


//=============================================================================
// Shutdown timeouts
//
// A script that never returns cannot be interrupted: mruby offers no way into
// a running VM without a compile-time option, and a blocking call has no
// cancellation point at all.  Killing the scripter process is therefore the
// only way to stop one, and it is also what releases nyamy's reader threads -
// they are parked in a synchronous ReadFile on an anonymous pipe, which only
// returns once the write end is closed.
//
// Two layers do the killing, and the order between them is what these
// constants encode: the scripter terminates itself first, and nyamy's
// TerminateProcess stays reserved for a scripter that cannot - a foreign
// implementation launched through the ini "cmdLine" setting, one wedged before
// its ctrl thread started, or one held alive by a Windows Error Reporting
// dialog.
//=============================================================================

/// Milliseconds the scripter waits for a running script after it observes Quit
/// (or ctrl-pipe EOF) before terminating itself.
/// Passed to nys_set_quit_timeout() by the scripter's main().
const uint32_t kScripterQuitTimeoutMillisec = 3000;

/// Milliseconds nyamy waits for the scripter to exit on its own after Quit.
const uint32_t kScripterQuitGraceMillisec = 5000;

/// Milliseconds nyamy waits after TerminateProcess: the reader threads only
/// have to return from the one ReadFile that closing the write ends released.
const uint32_t kScripterKillWaitMillisec = 2000;

static_assert(kScripterQuitGraceMillisec > kScripterQuitTimeoutMillisec,
              "nyamy must give the scripter time to terminate itself first");



#endif // !_CTRL_STREAM_H
