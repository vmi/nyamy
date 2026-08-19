//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// dlglog.h


#ifndef _DLGLOG_H
#  define _DLGLOG_H

#  include <windows.h>
#  include "msgstream.h"


//
INT_PTR CALLBACK dlgLog_dlgProc(
	HWND i_hwnd, UINT i_message, WPARAM i_wParam, LPARAM i_lParam);

enum {
	///
	WM_APP_dlglogNotify = WM_APP + 115,
};

/** How much text the log keeps, in wide characters, when nyamy.ini's
    logMaxSize is absent or invalid.

    This is the size of the one buffer LogBuffer allocates at startup, and
    nothing else scales with it: the edit control shows what that buffer
    holds rather than being the buffer itself, so raising it costs memory and
    not the price of every appended line. */
const size_t kLogEditMaxChars = 20000;

enum DlgLogNotify {
	DlgLogNotify_logCleared,			///
	/** The "detail" box was toggled.  lParam is the new LogLevel; the
	    tasktray window forwards it to the scripter. */
	DlgLogNotify_thresholdChanged,
};

class LogBuffer;

/// parameters for "Investigate" dialog box
class DlgLogData {
public:
	womsgstream *m_log;				/// log stream
	LogBuffer *m_logBuffer;			/// the text the dialog shows
	HWND m_hwndTaskTray;				/// tasktray window
};

#endif // !_DLGLOG_H
