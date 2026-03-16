//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// trigger_info.h
//
// TriggerInfo: snapshot of trigger key + focus window at the moment a
// user-defined function is invoked.  Passed Engine -> scripter via
// ExecUserFunc, echo-backed scripter -> Engine via ExecKeySeq, and used
// by the engine to reconstruct a Current for ad-hoc key sequence dispatch.


#ifndef _TRIGGER_INFO_H
#  define _TRIGGER_INFO_H

#  include <cstdint>
#  include <string>


/// Snapshot of the keyboard/focus state when a user function was triggered.
struct TriggerInfo {
	uint8_t      scanCode;      ///< trigger key scan code (0 = unknown)
	bool         extended;      ///< E0 prefix presence
	std::wstring windowClass;   ///< focus window class name
	std::wstring windowTitle;   ///< focus window title
};


#endif // !_TRIGGER_INFO_H
