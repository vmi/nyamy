//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// windowstool.h


#ifndef _WINDOWSTOOL_H
#  define _WINDOWSTOOL_H


#  include "stringtool.h"
#  include <windows.h>


/// instance handle of this application
extern HINSTANCE g_hInst;


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// resource

/// load resource string
extern std::wstring loadString(UINT i_id);

/** load small icon resource at the size i_dpi calls for (it must be deleted by
    DestroyIcon()) */
extern HICON loadSmallIcon(UINT i_id, UINT i_dpi);

/** load big icon resource at the size i_dpi calls for (it must be deleted by
    DestroyIcon()) */
extern HICON loadBigIcon(UINT i_id, UINT i_dpi);


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// window

/// resize the window (it does not move the window)
extern bool resizeWindow(HWND i_hwnd, int i_w, int i_h, bool i_doRepaint);

/** get rect of the window in client coordinates.
    @return rect of the window in client coordinates */
extern bool getChildWindowRect(HWND i_hwnd, RECT *o_rc);

/** set small icon to the specified window.
    @return handle of previous icon or NULL */
extern HICON setSmallIcon(HWND i_hwnd, UINT i_id);

/** set big icon to the specified window.
    @return handle of previous icon or NULL */
extern HICON setBigIcon(HWND i_hwnd, UINT i_id);

/// remove icon from a window that is set by setSmallIcon
extern void unsetSmallIcon(HWND i_hwnd);

/// remove icon from a window that is set by setBigIcon
extern void unsetBigIcon(HWND i_hwnd);

/// get toplevel (non-child) window
extern HWND getToplevelWindow(HWND i_hwnd, bool *io_isMDI);

/// move window asynchronously
extern void asyncMoveWindow(HWND i_hwnd, int i_x, int i_y);

/// move window asynchronously
extern void asyncMoveWindow(HWND i_hwnd, int i_x, int i_y, int i_w, int i_h);

/// resize asynchronously
extern void asyncResize(HWND i_hwnd, int i_w, int i_h);

/// get dll version
extern DWORD getDllVersion(const wchar_t *i_dllname);
#define PACKVERSION(major, minor) MAKELONG(minor, major)

// workaround of SetForegroundWindow
extern bool setForegroundWindow(HWND i_hwnd);


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// DPI

/** DPI of the monitor a point falls on, or 96 if it cannot be determined.

    A failure therefore scales nothing rather than guessing. */
extern UINT dpiForPoint(POINT i_pt);

/** DPI of the monitor a window sits on, or 96 if it cannot be determined.

    Not the same as GetDpiForWindow(): that answers what DPI the window is
    being rendered at, which for a DPI unaware target process is 96 whatever
    monitor it is on.  Coordinates nyamy hands to SetWindowPos are in the
    desktop's own space, so it is the monitor that decides the scale. */
extern UINT dpiForWindowMonitor(HWND i_hwnd);

/** Scale a length written for 96 dpi to what i_dpi calls for.

    Anything the code states as a pixel count - a caret width, a metric that
    predates GetSystemMetricsForDpi, a size out of the ini or the config file -
    is a 96 dpi length by convention and goes through here before it reaches
    the screen.  At 96 dpi this is the identity, which is what keeps the
    primary monitor a usable regression baseline.

    A non-zero length never scales down to nothing: a 1 px nudge stays a nudge
    rather than becoming a no-op. */
extern int scaleFromLogical(int i_px, UINT i_dpi);



//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// dialog

/// get/set GWL_USERDATA
template <class T> inline T getUserData(HWND i_hwnd, T *i_wc)
{
	return (*i_wc = reinterpret_cast<T>(GetWindowLongPtr(i_hwnd, GWLP_USERDATA)));
}

///
template <class T> inline T setUserData(HWND i_hwnd, T i_wc)
{
	SetWindowLongPtr(i_hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(i_wc));
	return i_wc;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// RECT

///
inline int rcWidth(const RECT *i_rc)
{
	return i_rc->right - i_rc->left;
}

///
inline int rcHeight(const RECT *i_rc)
{
	return i_rc->bottom - i_rc->top;
}

///
inline bool isRectInRect(const RECT *i_rcin, const RECT *i_rcout)
{
	return (i_rcout->left <= i_rcin->left &&
			i_rcin->right <= i_rcout->right &&
			i_rcout->top <= i_rcin->top &&
			i_rcin->bottom <= i_rcout->bottom);
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// edit control

/// returns bytes of text
extern size_t editGetTextBytes(HWND i_hwnd);

/// delete a line
extern void editDeleteLine(HWND i_hwnd, size_t i_n);

/// insert text at last
extern void editInsertTextAtLast(HWND i_hwnd, const std::wstring &i_text,
									 size_t i_threshold);


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Windows2000/XP specific API

/// SetLayeredWindowAttributes API
using SetLayeredWindowAttributes_t = BOOL (WINAPI *)
(HWND hwnd, COLORREF crKey, BYTE bAlpha, DWORD dwFlags);
extern SetLayeredWindowAttributes_t setLayeredWindowAttributes;

/// MonitorFromWindow API
extern HMONITOR (WINAPI *monitorFromWindow)(HWND hwnd, DWORD dwFlags);

/// GetMonitorInfo API
extern BOOL (WINAPI *getMonitorInfo)(HMONITOR hMonitor, LPMONITORINFO lpmi);

/// EnumDisplayMonitors API
extern BOOL (WINAPI *enumDisplayMonitors)
	(HDC hdc, LPRECT lprcClip, MONITORENUMPROC lpfnEnum, LPARAM dwData);


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// WindowsXP specific API

/// WTSRegisterSessionNotification API
using WTSRegisterSessionNotification_t = BOOL (WINAPI *)
(HWND hWnd, DWORD dwFlags);
extern WTSRegisterSessionNotification_t wtsRegisterSessionNotification;

/// WTSUnRegisterSessionNotification API
using WTSUnRegisterSessionNotification_t = BOOL (WINAPI *)(HWND hWnd);
extern WTSUnRegisterSessionNotification_t wtsUnRegisterSessionNotification;

/// WTSGetActiveConsoleSessionId API
using WTSGetActiveConsoleSessionId_t = DWORD (WINAPI *)(void);
extern WTSGetActiveConsoleSessionId_t wtsGetActiveConsoleSessionId;

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Utility

// PathRemoveFileSpec()
std::wstring pathRemoveFileSpec(const std::wstring &i_path);

// check Windows version i_major.i_minor or later
BOOL checkWindowsVersion(DWORD i_major, DWORD i_minor);

#endif // _WINDOWSTOOL_H
