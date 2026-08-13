//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// windowstool.cpp


#include "misc.h"

#include "windowstool.h"
#include <vector>

#include <windowsx.h>
#include <malloc.h>
#include <shlwapi.h>
#include <shellscalingapi.h>
#pragma comment(lib, "shcore.lib")


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Global variables


// instance handle of this application
HINSTANCE g_hInst = NULL;


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Functions


// load resource string
std::wstring loadString(UINT i_id)
{
	wchar_t buf[1024];
	if (LoadString(g_hInst, i_id, buf, NUMBER_OF(buf)))
		return std::wstring(buf);
	else
		return L"";
}


// load small icon resource
HICON loadSmallIcon(UINT i_id, UINT i_dpi)
{
	// SM_CXSMICON is 16 at 96 dpi, which is what this used to hardcode
	int cx = GetSystemMetricsForDpi(SM_CXSMICON, i_dpi);
	int cy = GetSystemMetricsForDpi(SM_CYSMICON, i_dpi);
	return reinterpret_cast<HICON>(
			   LoadImage(g_hInst, MAKEINTRESOURCE(i_id), IMAGE_ICON, cx, cy, 0));
}


// load big icon resource
HICON loadBigIcon(UINT i_id, UINT i_dpi)
{
	// SM_CXICON is 32 at 96 dpi, which is what this used to hardcode
	int cx = GetSystemMetricsForDpi(SM_CXICON, i_dpi);
	int cy = GetSystemMetricsForDpi(SM_CYICON, i_dpi);
	return reinterpret_cast<HICON>(
			   LoadImage(g_hInst, MAKEINTRESOURCE(i_id), IMAGE_ICON, cx, cy, 0));
}


// set small icon to the specified window.
// @return handle of previous icon or NULL
HICON setSmallIcon(HWND i_hwnd, UINT i_id)
{
	HICON hicon = (i_id == static_cast<UINT>(-1))
				  ? NULL : loadSmallIcon(i_id, GetDpiForWindow(i_hwnd));
	return reinterpret_cast<HICON>(
			   SendMessage(i_hwnd, WM_SETICON, static_cast<WPARAM>(ICON_SMALL),
						   reinterpret_cast<LPARAM>(hicon)));
}


// set big icon to the specified window.
// @return handle of previous icon or NULL
HICON setBigIcon(HWND i_hwnd, UINT i_id)
{
	HICON hicon = (i_id == static_cast<UINT>(-1))
				  ? NULL : loadBigIcon(i_id, GetDpiForWindow(i_hwnd));
	return reinterpret_cast<HICON>(
			   SendMessage(i_hwnd, WM_SETICON, static_cast<WPARAM>(ICON_BIG),
						   reinterpret_cast<LPARAM>(hicon)));
}


// remove icon from a window that is set by setSmallIcon
void unsetSmallIcon(HWND i_hwnd)
{
	HICON hicon = setSmallIcon(i_hwnd, static_cast<UINT>(-1));
	if (hicon)
		CHECK_TRUE( DestroyIcon(hicon) );
}


// remove icon from a window that is set by setBigIcon
void unsetBigIcon(HWND i_hwnd)
{
	HICON hicon = setBigIcon(i_hwnd, static_cast<UINT>(-1));
	if (hicon)
		CHECK_TRUE( DestroyIcon(hicon) );
}


// resize the window (it does not move the window)
bool resizeWindow(HWND i_hwnd, int i_w, int i_h, bool i_doRepaint)
{
	UINT flag = SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOZORDER;
	if (!i_doRepaint)
		flag |= SWP_NOREDRAW;
	return !!SetWindowPos(i_hwnd, NULL, 0, 0, i_w, i_h, flag);
}


// get rect of the window in client coordinates
// @return rect of the window in client coordinates
bool getChildWindowRect(HWND i_hwnd, RECT *o_rc)
{
	if (!GetWindowRect(i_hwnd, o_rc))
		return false;
	POINT p = { o_rc->left, o_rc->top };
	HWND phwnd = GetParent(i_hwnd);
	if (!phwnd)
		return false;
	if (!ScreenToClient(phwnd, &p))
		return false;
	o_rc->left = p.x;
	o_rc->top = p.y;
	p.x = o_rc->right;
	p.y = o_rc->bottom;
	ScreenToClient(phwnd, &p);
	o_rc->right = p.x;
	o_rc->bottom = p.y;
	return true;
}


// get toplevel (non-child) window
HWND getToplevelWindow(HWND i_hwnd, bool *io_isMDI)
{
	while (i_hwnd) {
		LONG_PTR style = GetWindowLongPtr(i_hwnd, GWL_STYLE);
		if ((style & WS_CHILD) == 0)
			break;
		if (io_isMDI && *io_isMDI) {
			LONG_PTR exStyle = GetWindowLongPtr(i_hwnd, GWL_EXSTYLE);
			if (exStyle & WS_EX_MDICHILD)
				return i_hwnd;
		}
		i_hwnd = GetParent(i_hwnd);
	}
	if (io_isMDI)
		*io_isMDI = false;
	return i_hwnd;
}


// move window asynchronously
void asyncMoveWindow(HWND i_hwnd, int i_x, int i_y)
{
	SetWindowPos(i_hwnd, NULL, i_x, i_y, 0, 0,
				 SWP_ASYNCWINDOWPOS | SWP_NOACTIVATE | SWP_NOOWNERZORDER |
				 SWP_NOSIZE | SWP_NOZORDER);
}


// move window asynchronously
void asyncMoveWindow(HWND i_hwnd, int i_x, int i_y, int i_w, int i_h)
{
	SetWindowPos(i_hwnd, NULL, i_x, i_y, i_w, i_h,
				 SWP_ASYNCWINDOWPOS | SWP_NOACTIVATE | SWP_NOOWNERZORDER |
				 SWP_NOZORDER);
}


// resize asynchronously
void asyncResize(HWND i_hwnd, int i_w, int i_h)
{
	SetWindowPos(i_hwnd, NULL, 0, 0, i_w, i_h,
				 SWP_ASYNCWINDOWPOS | SWP_NOACTIVATE | SWP_NOOWNERZORDER |
				 SWP_NOMOVE | SWP_NOZORDER);
}


// get dll version
DWORD getDllVersion(const wchar_t *i_dllname)
{
	DWORD dwVersion = 0;

	if (HINSTANCE hinstDll = LoadLibrary(i_dllname)) {
		DLLGETVERSIONPROC pDllGetVersion
		= (DLLGETVERSIONPROC)GetProcAddress(hinstDll, "DllGetVersion");
		/* Because some DLLs may not implement this function, you
		 * must test for it explicitly. Depending on the particular
		 * DLL, the lack of a DllGetVersion function may
		 * be a useful indicator of the version.
		 */
		if (pDllGetVersion) {
			DLLVERSIONINFO dvi;
			ZeroMemory(&dvi, sizeof(dvi));
			dvi.cbSize = sizeof(dvi);

			HRESULT hr = (*pDllGetVersion)(&dvi);
			if (SUCCEEDED(hr))
				dwVersion = PACKVERSION(dvi.dwMajorVersion, dvi.dwMinorVersion);
		}

		FreeLibrary(hinstDll);
	}
	return dwVersion;
}


// workaround of SetForegroundWindow
bool setForegroundWindow(HWND i_hwnd)
{
	int nForegroundID = GetWindowThreadProcessId(GetForegroundWindow(), NULL);
	int nTargetID = GetWindowThreadProcessId(i_hwnd, NULL);

	//if (!AttachThreadInput(nTargetID, nForegroundID, TRUE))
	//return false;
	AttachThreadInput(nTargetID, nForegroundID, TRUE);

	DWORD sp_time = 0;
	SystemParametersInfo(SPI_GETFOREGROUNDLOCKTIMEOUT, 0, &sp_time, 0);
	SystemParametersInfo(SPI_SETFOREGROUNDLOCKTIMEOUT, 0, (void *)0, 0);

	SetForegroundWindow(i_hwnd);

	SystemParametersInfo(SPI_SETFOREGROUNDLOCKTIMEOUT, 0, reinterpret_cast<void *>(static_cast<uintptr_t>(sp_time)), 0);

	AttachThreadInput(nTargetID, nForegroundID, FALSE);
	return true;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// DPI


// query one DPI flavour of a monitor; 0 when it cannot be obtained
//
// GetDpiForMonitor lives in shcore.dll, which is linked statically.  Resolving
// it at run time would protect nothing: this binary already imports
// GetDpiForWindow and GetSystemMetricsForDpi from USER32, and the manifest asks
// for Per-Monitor v2, so the process cannot load below Windows 10 1703 anyway.
static UINT monitorDpi(HMONITOR i_hmonitor, MONITOR_DPI_TYPE i_type)
{
	if (!i_hmonitor)
		return 0;
	UINT dpiX = 0, dpiY = 0;
	if (FAILED(GetDpiForMonitor(i_hmonitor, i_type, &dpiX, &dpiY)))
		return 0;
	// only the x axis is reported: Windows has never shipped a monitor whose
	// two axes differ, and a single number keeps the callers readable
	return dpiX;
}


// DPI of a monitor, falling back to 96 so that a failure scales nothing
static UINT effectiveDpi(HMONITOR i_hmonitor)
{
	UINT dpi = monitorDpi(i_hmonitor, MDT_EFFECTIVE_DPI);
	return dpi ? dpi : USER_DEFAULT_SCREEN_DPI;
}


// DPI of the monitor a point falls on
UINT dpiForPoint(POINT i_pt)
{
	return effectiveDpi(MonitorFromPoint(i_pt, MONITOR_DEFAULTTONEAREST));
}


// DPI of the monitor a window sits on
UINT dpiForWindowMonitor(HWND i_hwnd)
{
	return effectiveDpi(monitorFromWindow(i_hwnd, MONITOR_DEFAULTTONEAREST));
}


// scale a length written for 96 dpi to what i_dpi calls for
int scaleFromLogical(int i_px, UINT i_dpi)
{
	if (i_px == 0 || i_dpi == USER_DEFAULT_SCREEN_DPI)
		return i_px;

	int scaled = MulDiv(i_px, static_cast<int>(i_dpi),
						USER_DEFAULT_SCREEN_DPI);
	// MulDiv rounds to nearest, so a 1 px value scaled by 1.25 lands back on 1
	// - fine - but scaling down could take it to 0 and silently turn a nudge
	// into a no-op.  Keep the sign and at least the smallest step.
	if (scaled == 0)
		return (i_px < 0) ? -1 : 1;
	return scaled;
}



//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// edit control


// get edit control's text size
// @return bytes of text
size_t editGetTextBytes(HWND i_hwnd)
{
	return Edit_GetTextLength(i_hwnd);
}


// delete a line
void editDeleteLine(HWND i_hwnd, size_t i_n)
{
	int len = Edit_LineLength(i_hwnd, i_n);
	if (len < 0)
		return;
	len += 2;
	int index = Edit_LineIndex(i_hwnd, i_n);
	Edit_SetSel(i_hwnd, index, index + len);
	Edit_ReplaceSel(i_hwnd, L"");
}


// insert text at last
void editInsertTextAtLast(HWND i_hwnd, const std::wstring &i_text,
						  size_t i_threshold)
{
	if (i_text.empty())
		return;

	size_t len = editGetTextBytes(i_hwnd);

	if (i_threshold < len) {
		// Drop the oldest third-plus, but on a line boundary: cutting at a raw
		// character offset leaves a truncated line at the top of the log.
		int cut = Edit_LineFromChar(i_hwnd, static_cast<int>(len / 3 * 2));
		int index = Edit_LineIndex(i_hwnd, cut);
		if (index <= 0)
			index = static_cast<int>(len / 3 * 2);
		Edit_SetSel(i_hwnd, 0, index);
		Edit_ReplaceSel(i_hwnd, L"");
		len = editGetTextBytes(i_hwnd);
	}

	Edit_SetSel(i_hwnd, len, len);

	// \n -> \r\n
	std::vector<wchar_t> buf(i_text.size() * 2 + 1);
	wchar_t *d = buf.data();
	const wchar_t *str = i_text.c_str();
	for (const wchar_t *s = str; s < str + i_text.size(); ++ s) {
		if (*s == L'\n')
			*d++ = L'\r';
		*d++ = *s;
	}
	*d = L'\0';

	Edit_ReplaceSel(i_hwnd, buf.data());
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Windows2000/XP specific API


// initialize layerd window
static BOOL WINAPI initalizeLayerdWindow(
	HWND i_hwnd, COLORREF i_crKey, BYTE i_bAlpha, DWORD i_dwFlags)
{
	HMODULE hModule = GetModuleHandle(L"user32.dll");
	if (!hModule) {
		return FALSE;
	}
	SetLayeredWindowAttributes_t proc =
		reinterpret_cast<SetLayeredWindowAttributes_t>(
			GetProcAddress(hModule, "SetLayeredWindowAttributes"));
	if (setLayeredWindowAttributes) {
		setLayeredWindowAttributes = proc;
		return setLayeredWindowAttributes(i_hwnd, i_crKey, i_bAlpha, i_dwFlags);
	} else {
		return FALSE;
	}
}


// SetLayeredWindowAttributes API
SetLayeredWindowAttributes_t setLayeredWindowAttributes
= initalizeLayerdWindow;


// emulate MonitorFromWindow API
static HMONITOR WINAPI emulateMonitorFromWindow(HWND hwnd, DWORD dwFlags)
{
	return reinterpret_cast<HMONITOR>(1); // dummy HMONITOR
}

// initialize MonitorFromWindow API
static HMONITOR WINAPI initializeMonitorFromWindow(HWND hwnd, DWORD dwFlags)
{
	HMODULE hModule = GetModuleHandle(L"user32.dll");
	if (!hModule)
		return FALSE;

	FARPROC proc = GetProcAddress(hModule, "MonitorFromWindow");
	if (proc)
		monitorFromWindow =
			reinterpret_cast<HMONITOR (WINAPI *)(HWND, DWORD)>(proc);
	else
		monitorFromWindow = emulateMonitorFromWindow;

	return monitorFromWindow(hwnd, dwFlags);
}

// MonitorFromWindow API
HMONITOR (WINAPI *monitorFromWindow)(HWND hwnd, DWORD dwFlags)
= initializeMonitorFromWindow;


// emulate GetMonitorInfo API
static BOOL WINAPI emulateGetMonitorInfo(HMONITOR hMonitor, LPMONITORINFO lpmi)
{
	if (lpmi->cbSize != sizeof(MONITORINFO))
		return FALSE;

	lpmi->rcMonitor.left = 0;
	lpmi->rcMonitor.top = 0;
	lpmi->rcMonitor.right = GetSystemMetrics(SM_CXFULLSCREEN);
	lpmi->rcMonitor.bottom = GetSystemMetrics(SM_CYFULLSCREEN);
	SystemParametersInfo(SPI_GETWORKAREA, 0,
						 reinterpret_cast<PVOID>(&lpmi->rcWork), FALSE);
	lpmi->dwFlags = MONITORINFOF_PRIMARY;

	return TRUE;
}

// initialize GetMonitorInfo API
static
BOOL WINAPI initializeGetMonitorInfo(HMONITOR hMonitor, LPMONITORINFO lpmi)
{
	HMODULE hModule = GetModuleHandle(L"user32.dll");
	if (!hModule)
		return FALSE;

	FARPROC proc = GetProcAddress(hModule, "GetMonitorInfoA");
	if (proc)
		getMonitorInfo =
			reinterpret_cast<BOOL (WINAPI *)(HMONITOR, LPMONITORINFO)>(proc);
	else
		getMonitorInfo = emulateGetMonitorInfo;

	return getMonitorInfo(hMonitor, lpmi);
}

// GetMonitorInfo API
BOOL (WINAPI *getMonitorInfo)(HMONITOR hMonitor, LPMONITORINFO lpmi)
= initializeGetMonitorInfo;


// enumalte EnumDisplayMonitors API
static BOOL WINAPI emulateEnumDisplayMonitors(
	HDC hdc, LPRECT lprcClip, MONITORENUMPROC lpfnEnum, LPARAM dwData)
{
	lpfnEnum(reinterpret_cast<HMONITOR>(1), hdc, lprcClip, dwData);
	return TRUE;
}

// initialize EnumDisplayMonitors API
static BOOL WINAPI initializeEnumDisplayMonitors(
	HDC hdc, LPRECT lprcClip, MONITORENUMPROC lpfnEnum, LPARAM dwData)
{
	HMODULE hModule = GetModuleHandle(L"user32.dll");
	if (!hModule)
		return FALSE;

	FARPROC proc = GetProcAddress(hModule, "EnumDisplayMonitors");
	if (proc)
		enumDisplayMonitors =
			reinterpret_cast<BOOL (WINAPI *)(HDC, LPRECT, MONITORENUMPROC, LPARAM)>
			(proc);
	else
		enumDisplayMonitors = emulateEnumDisplayMonitors;

	return enumDisplayMonitors(hdc, lprcClip, lpfnEnum, dwData);
}

// EnumDisplayMonitors API
BOOL (WINAPI *enumDisplayMonitors)
(HDC hdc, LPRECT lprcClip, MONITORENUMPROC lpfnEnum, LPARAM dwData)
= initializeEnumDisplayMonitors;


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Windows2000/XP specific API


static BOOL WINAPI
initializeWTSRegisterSessionNotification(HWND hWnd, DWORD dwFlags)
{
	LoadLibrary(L"wtsapi32.dll");
	HMODULE hModule = GetModuleHandle(L"wtsapi32.dll");
	if (!hModule) {
		return FALSE;
	}
	WTSRegisterSessionNotification_t proc =
		reinterpret_cast<WTSRegisterSessionNotification_t>(
			GetProcAddress(hModule, "WTSRegisterSessionNotification"));
	if (proc) {
		wtsRegisterSessionNotification = proc;
		return wtsRegisterSessionNotification(hWnd, dwFlags);
	} else {
		return 0;
	}
}

// WTSRegisterSessionNotification API
WTSRegisterSessionNotification_t wtsRegisterSessionNotification
= initializeWTSRegisterSessionNotification;


static BOOL WINAPI initializeWTSUnRegisterSessionNotification(HWND hWnd)
{
	HMODULE hModule = GetModuleHandle(L"wtsapi32.dll");
	if (!hModule) {
		return FALSE;
	}
	WTSUnRegisterSessionNotification_t proc =
		reinterpret_cast<WTSUnRegisterSessionNotification_t>(
			GetProcAddress(hModule, "WTSUnRegisterSessionNotification"));
	if (proc) {
		wtsUnRegisterSessionNotification = proc;
		return wtsUnRegisterSessionNotification(hWnd);
	} else {
		return 0;
	}
}

// WTSUnRegisterSessionNotification API
WTSUnRegisterSessionNotification_t wtsUnRegisterSessionNotification
= initializeWTSUnRegisterSessionNotification;


static DWORD WINAPI initializeWTSGetActiveConsoleSessionId(void)
{
	HMODULE hModule = GetModuleHandle(L"kernel32.dll");
	if (!hModule) {
		return FALSE;
	}
	WTSGetActiveConsoleSessionId_t proc =
		reinterpret_cast<WTSGetActiveConsoleSessionId_t>(
			GetProcAddress(hModule, "WTSGetActiveConsoleSessionId"));
	if (proc) {
		wtsGetActiveConsoleSessionId = proc;
		return wtsGetActiveConsoleSessionId();
	} else {
		return 0;
	}
}

// WTSGetActiveConsoleSessionId API
WTSGetActiveConsoleSessionId_t wtsGetActiveConsoleSessionId
= initializeWTSGetActiveConsoleSessionId;


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Utility

// PathRemoveFileSpec()
std::wstring pathRemoveFileSpec(const std::wstring &i_path)
{
	const wchar_t *str = i_path.c_str();
	const wchar_t *b = wcsrchr(str, L'\\');
	const wchar_t *s = wcsrchr(str, L'/');
	if (b && s)
		return std::wstring(str, MIN(b, s));
	if (b)
		return std::wstring(str, b);
	if (s)
		return std::wstring(str, s);
	if (const wchar_t *c = wcsrchr(str, L':'))
		return std::wstring(str, c + 1);
	return i_path;
}

BOOL checkWindowsVersion(DWORD i_major, DWORD i_minor)
{
    DWORDLONG conditionMask = 0;
    OSVERSIONINFOEX osvi;
	memset(&osvi, 0, sizeof(OSVERSIONINFOEX));

    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
    osvi.dwMajorVersion = i_major;
    osvi.dwMinorVersion = i_minor;

    VER_SET_CONDITION(conditionMask, VER_MAJORVERSION, VER_GREATER_EQUAL);
    VER_SET_CONDITION(conditionMask, VER_MINORVERSION, VER_GREATER_EQUAL);

    // Perform the test.
    return VerifyVersionInfo(&osvi, VER_MAJORVERSION | VER_MINORVERSION, conditionMask);
}
