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

/** How much text the log edit control keeps, when nyamy.ini's logMaxSize is
    absent or invalid.  Appending is O(buffer length): measured 137 us at
    10000 characters against 325 us at 60000, so this is a direct trade of
    scrollback against the cost of every single log line. */
const size_t kLogEditMaxChars = 20000;

enum DlgLogNotify {
	DlgLogNotify_logCleared,			///
	/** The "detail" box was toggled.  lParam is the new LogLevel; the
	    tasktray window forwards it to the scripter. */
	DlgLogNotify_thresholdChanged,
};

/// parameters for "Investigate" dialog box
class DlgLogData {
public:
	womsgstream *m_log;				/// log stream
	HWND m_hwndTaskTray;				/// tasktray window
};

#endif // !_DLGLOG_H
