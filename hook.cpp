//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// hook.cpp


#define _HOOK_CPP

#include "misc.h"

#include "hook.h"
#include "stringtool.h"

#include <locale.h>
#include <imm.h>
#include <richedit.h>
#include <vector>


///
#define HOOK_DATA_NAME L"{08D6E55C-5103-4e00-8209-A1C4AB13BBEF}" WIDEN(VERSION)
#ifdef _WIN64
#define HOOK_DATA_NAME_ARCH L"{290C0D51-8AEE-403d-9172-E43D46270996}" WIDEN(VERSION)
#else // !_WIN64
#define HOOK_DATA_NAME_ARCH L"{716A5DEB-CB02-4438-ABC8-D00E48673E45}" WIDEN(VERSION)
#endif // !_WIN64

/** how long to leave the mailslot alone after failing to open it.

    Only reached while no nyamy is running, which the hooks normally outlive
    by moments; this just keeps a nyamy that died without unhooking from
    costing every hooked process an open on every notification.
*/
static const DWORD kMailslotRetryMillisec = 500;

// Some applications use different values for below messages
// when double click of title bar.
#define SC_MAXIMIZE2 (SC_MAXIMIZE + 2)
#define SC_MINIMIZE2 (SC_MINIMIZE + 2)
#define SC_RESTORE2 (SC_RESTORE + 2)

// Debug Macros
#ifdef NDEBUG
#define HOOK_RPT0(msg)
#define HOOK_RPT1(msg, arg1)
#define HOOK_RPT2(msg, arg1, arg2)
#define HOOK_RPT3(msg, arg1, arg2, arg3)
#define HOOK_RPT4(msg, arg1, arg2, arg3, arg4)
#define HOOK_RPT5(msg, arg1, arg2, arg3, arg4, arg5)
#else
#define HOOK_RPT0(msg) if (g.m_isLogging) { _RPT0(_CRT_WARN, msg); }
#define HOOK_RPT1(msg, arg1) if (g.m_isLogging) { _RPT1(_CRT_WARN, msg, arg1); }
#define HOOK_RPT2(msg, arg1, arg2) if (g.m_isLogging) { _RPT2(_CRT_WARN, msg, arg1, arg2); }
#define HOOK_RPT3(msg, arg1, arg2, arg3) if (g.m_isLogging) { _RPT3(_CRT_WARN, msg, arg1, arg2, arg3); }
#define HOOK_RPT4(msg, arg1, arg2, arg3, arg4) if (g.m_isLogging) { _RPT4(_CRT_WARN, msg, arg1, arg2, arg3, arg4); }
#define HOOK_RPT5(msg, arg1, arg2, arg3, arg4, arg5) if (g.m_isLogging) { _RPT5(_CRT_WARN, msg, arg1, arg2, arg3, arg4, arg5); }
#endif

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Global Variables


DllExport HookData *g_hookData;			///

///
class HookDataArch
{
public:
	HHOOK m_hHookGetMessage;			///
	HHOOK m_hHookCallWndProc;			///
};

static HookDataArch *s_hookDataArch;

struct Globals {
	HANDLE m_hHookData;				///
	HANDLE m_hHookDataArch;			///
	HINSTANCE m_hInstDLL;				///
	UINT m_WM_MAYU_MESSAGE;			///
	bool m_isImeLock;				///
	bool m_isImeCompositioning;			///
	HHOOK m_hHookMouseProc;			///
	HHOOK m_hHookKeyboardProc;			///
	INPUT_DETOUR m_keyboardDetour;
	INPUT_DETOUR m_mouseDetour;
	Engine *m_engine;
	HWND m_hwndTaskTray;				///
	HANDLE m_hMailslot;
	DWORD m_mailslotRetryAt;			/// tick a failed reopen may be retried at
	bool m_isInitialized;
#ifdef HOOK_LOG_TO_FILE
	HANDLE m_logFile;
#endif // HOOK_LOG_TO_FILE
#ifndef NDEBUG
	bool m_isLogging;
	wchar_t m_moduleName[GANA_MAX_PATH];
#endif // !NDEBUG
};

static Globals g;

/** Hook state that belongs to one GUI thread rather than to the process.

    A process can run its user interface on more than one thread: WinUI 3 puts
    the top level window and the input control on different ones, and Windows
    attaches the input queues of threads related that way, so GetFocus() answers
    the same window whichever of them asks.

    Held in Globals, these two were one value for the whole process, and the
    threads overwrote each other:

    - m_hwndFocus is what notifySetFocus() compares against to decide whether
      anything changed.  Whichever thread reported the focus window first
      silenced every other thread's report of the same window, so nyamy learned
      the focus of one thread only.  It keys what it knows by the reporting
      thread, and looks it up by the thread owning the foreground window - a
      different thread here - so the lookup missed and the window specific
      keymaps stopped applying.  Which thread won the race varied from run to
      run, which is what made the symptom look intermittent.
    - m_isInMenu makes getClassNameTitleName() report MENU.  One thread opening
      a menu made every other thread's window report itself that way.
*/
struct ThreadLocals {
	HWND m_hwndFocus;				/// focus window this thread last reported
	bool m_isInMenu;				/// is this thread inside a menu loop ?
};

static thread_local ThreadLocals t;


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Prototypes


static void notifyThreadDetach();
static void notifyShow(NotifyShow::Show i_show, bool i_isMDI);
static bool openMailslot();
static bool mapHookData(bool i_isYamy);
static void unmapHookData();
static bool initialize(bool i_isYamy);
static bool notify(void *i_data, size_t i_dataSize);


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Functions


#ifdef HOOK_LOG_TO_FILE
static void WriteToLog(const char *data)
{
	char buf[1024];
	DWORD count;

	WideCharToMultiByte(CP_THREAD_ACP, 0, g.m_moduleName, -1, buf, NUMBER_OF(buf), NULL, NULL);
	strcat(buf, ": ");
	strcat(buf, data);
	SetFilePointer(g.m_logFile, 0, NULL, FILE_END);
	WriteFile(g.m_logFile, buf, strlen(buf), &count, NULL);
	FlushFileBuffers(g.m_logFile);
}
#else // !HOOK_LOG_TO_FILE
#define WriteToLog(data)
#endif // !HOOK_LOG_TO_FILE

bool initialize(bool i_isYamy)
{
#ifndef NDEBUG
	wchar_t path[GANA_MAX_PATH];
	GetModuleFileName(NULL, path, GANA_MAX_PATH);
	_wsplitpath_s(path, NULL, 0, NULL, 0, g.m_moduleName, GANA_MAX_PATH, NULL, 0);
	if (_wcsnicmp(g.m_moduleName, L"Dbgview", sizeof(L"Dbgview")/sizeof(wchar_t)) != 0 &&
			_wcsnicmp(g.m_moduleName, L"windbg", sizeof(L"windbg")/sizeof(wchar_t)) != 0) {
		g.m_isLogging = true;
	}
#endif // !NDEBUG
#ifdef HOOK_LOG_TO_FILE
	wchar_t logFileName[GANA_MAX_PATH];
	GetEnvironmentVariable(L"USERPROFILE", logFileName, NUMBER_OF(logFileName));
	wcsncat(logFileName, L"\\AppData\\LocalLow\\nyamydll.txt", wcslen(L"\\AppData\\LocalLow\\nyamydll.txt"));
	g.m_logFile = CreateFile(logFileName, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
		OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
#endif // HOOK_LOG_TO_FILE
	WriteToLog("try to open mailslot\r\n");
	if (openMailslot()) {
		WriteToLog("open mailslot OK\r\n");
	} else {
		WriteToLog("open mailslot NG\r\n");
	}
	if (!mapHookData(i_isYamy))
		return false;
	_wsetlocale(LC_ALL, L"");
	g.m_WM_MAYU_MESSAGE =
		RegisterWindowMessage(addSessionId(WM_MAYU_MESSAGE_NAME).c_str());
	g.m_hwndTaskTray = g_hookData->getHwndTaskTray();
	if (!i_isYamy) {
		NotifyThreadAttach ntd;
		ntd.m_type = Notify::Type_threadAttach;
		ntd.m_threadId = GetCurrentThreadId();
		notify(&ntd, sizeof(ntd));
	}
	g.m_isInitialized = true;
	return true;
}

/// EntryPoint
BOOL WINAPI DllMain(HINSTANCE i_hInstDLL, DWORD i_fdwReason,
					LPVOID /* i_lpvReserved */)
{
	switch (i_fdwReason) {
	case DLL_PROCESS_ATTACH: {
#ifndef NDEBUG
		g.m_isLogging = false;
#endif // !NDEBUG
		g.m_isInitialized = false;
		g.m_hInstDLL = i_hInstDLL;
		// zero initialized statics would read as a valid handle here, and a
		// thread detaching before initialize() does reach notify()
		g.m_hMailslot = INVALID_HANDLE_VALUE;
		g.m_mailslotRetryAt = 0;
		break;
	}
	case DLL_THREAD_ATTACH:
		break;
	case DLL_PROCESS_DETACH:
		notifyThreadDetach();
		unmapHookData();
		if (g.m_hMailslot != INVALID_HANDLE_VALUE) {
			CloseHandle(g.m_hMailslot);
			g.m_hMailslot = INVALID_HANDLE_VALUE;
		}
#ifdef HOOK_LOG_TO_FILE
		if (g.m_logFile != INVALID_HANDLE_VALUE) {
			CloseHandle(g.m_logFile);
			g.m_logFile = INVALID_HANDLE_VALUE;
		}
#endif // HOOK_LOG_TO_FILE
		break;
	case DLL_THREAD_DETACH:
		notifyThreadDetach();
		break;
	default:
		break;
	}
	return TRUE;
}


/// map hook data
static bool mapHookData(bool i_isYamy)
{
	DWORD access = FILE_MAP_READ;

	if (i_isYamy) {
		access |= FILE_MAP_WRITE;
		g.m_hHookData = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,	0, sizeof(HookData), addSessionId(HOOK_DATA_NAME).c_str());
		g.m_hHookDataArch = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,	0, sizeof(HookDataArch), addSessionId(HOOK_DATA_NAME_ARCH).c_str());
	} else {
		g.m_hHookData = OpenFileMapping(access, FALSE, addSessionId(HOOK_DATA_NAME).c_str());
		g.m_hHookDataArch = OpenFileMapping(access, FALSE, addSessionId(HOOK_DATA_NAME_ARCH).c_str());
	}

	if (g.m_hHookData == NULL || g.m_hHookDataArch == NULL) {
		unmapHookData();
		return false;
	}

	g_hookData = (HookData *)MapViewOfFile(g.m_hHookData, access, 0, 0, sizeof(HookData));
	s_hookDataArch = (HookDataArch *)MapViewOfFile(g.m_hHookDataArch, access, 0, 0, sizeof(HookDataArch));
	if (g_hookData == NULL || s_hookDataArch == NULL) {
		unmapHookData();
		return false;
	}

	return true;
}


/// unmap hook data
static void unmapHookData()
{
	if (g_hookData)
		UnmapViewOfFile(g_hookData);
	g_hookData = NULL;
	if (g.m_hHookData)
		CloseHandle(g.m_hHookData);
	g.m_hHookData = NULL;
	if (s_hookDataArch)
		UnmapViewOfFile(s_hookDataArch);
	s_hookDataArch = NULL;
	if (g.m_hHookDataArch)
		CloseHandle(g.m_hHookDataArch);
	g.m_hHookDataArch = NULL;
}


/** open the client side of nyamy's mailslot.

    Also used to pick up the mailslot of a *new* nyamy: this DLL outlives the
    nyamy that got it injected, because Windows unloads a global hook DLL only
    when its process next retrieves a message.
*/
static bool openMailslot()
{
	if (g.m_hMailslot != INVALID_HANDLE_VALUE)
		return true;

	// A nyamy that is simply not running would otherwise have every hooked
	// process attempt this on every notification.
	DWORD now = GetTickCount();
	if (g.m_mailslotRetryAt != 0 &&
			static_cast<LONG>(now - g.m_mailslotRetryAt) < 0)
		return false;

	g.m_hMailslot =
		CreateFile(addSessionId(NOTIFY_MAILSLOT_NAME).c_str(), GENERIC_WRITE,
				   FILE_SHARE_READ | FILE_SHARE_WRITE,
				   (SECURITY_ATTRIBUTES *)NULL, OPEN_EXISTING,
				   FILE_ATTRIBUTE_NORMAL, (HANDLE)NULL);
	if (g.m_hMailslot == INVALID_HANDLE_VALUE) {
		HOOK_RPT2("MAYU: %S open mailslot failed(0x%08x)\r\n",
				  g.m_moduleName, GetLastError());
		g.m_mailslotRetryAt = now + kMailslotRetryMillisec;
		return false;
	}
	HOOK_RPT1("MAYU: %S open mailslot successed\r\n", g.m_moduleName);
	g.m_mailslotRetryAt = 0;
	return true;
}


/** notify

    This runs inside the message hook of whatever process we are injected in,
    so it must not block.  A local mailslot write does not.  WM_COPYDATA is
    what is left for a process that could not open the mailslot at all, and it
    is a synchronous send - hence the timeout on it, and hence it not being
    the first choice.
*/
bool notify(void *i_data, size_t i_dataSize)
{
	COPYDATASTRUCT cd;
	DWORD_PTR result;

	DWORD len;
	if (openMailslot()) {
		if (WriteFile(g.m_hMailslot, i_data, (DWORD) i_dataSize, &len, NULL))
			return true;

		// The nyamy that owned this mailslot has gone.  Its successor made a
		// new one under the same name, which our handle knows nothing about -
		// so drop the handle, take the new one and send this message again.
		// Without the resend the first notification after a restart is lost,
		// and notifySetFocus() only fires on a change, so the focus would
		// stay wrong until the user moved away from that window and back.
		HOOK_RPT2("MAYU: %S WriteFile to mailslot failed(0x%08x), reopening\r\n",
				  g.m_moduleName, GetLastError());
		CloseHandle(g.m_hMailslot);
		g.m_hMailslot = INVALID_HANDLE_VALUE;
		g.m_mailslotRetryAt = 0;		// a restart is worth an attempt now
		if (openMailslot() &&
				WriteFile(g.m_hMailslot, i_data, (DWORD) i_dataSize, &len, NULL))
			return true;
		// fall through to WM_COPYDATA: there may be a nyamy with no mailslot
	}

	cd.dwData = reinterpret_cast<Notify *>(i_data)->m_type;
	cd.cbData = static_cast<DWORD>(i_dataSize);
	cd.lpData = i_data;
	// Read the window back rather than trust what initialize() saw: a nyamy
	// that started after us has a different one, and HookData is where it
	// publishes it.
	HWND hwndTaskTray = g_hookData ? g_hookData->getHwndTaskTray() : NULL;
	if (hwndTaskTray == NULL || cd.dwData == Notify::Type_threadDetach)
		return false;
	if (!SendMessageTimeout(hwndTaskTray,
							WM_COPYDATA, NULL, reinterpret_cast<LPARAM>(&cd),
							SMTO_ABORTIFHUNG | SMTO_NORMAL, 5000, &result)) {
		_RPT0(_CRT_WARN, "MAYU: SendMessageTimeout() timeouted\r\n");
		return false;
	}
	return true;
}


/// get class name and title name
static void getClassNameTitleName(HWND i_hwnd, bool i_isInMenu,
								  wstringi *o_className,
								  std::wstring *o_titleName)
{
	wstringi &className = *o_className;
	std::wstring &titleName = *o_titleName;

	bool isTheFirstTime = true;

	if (i_isInMenu) {
		className = titleName = L"MENU";
		isTheFirstTime = false;
	}

	while (true) {
		wchar_t buf[MAX(GANA_MAX_PATH, GANA_MAX_ATOM_LENGTH)];

		// get class name
		if (i_hwnd)
			GetClassName(i_hwnd, buf, NUMBER_OF(buf));
		else
			GetModuleFileName(GetModuleHandle(NULL), buf, NUMBER_OF(buf));
		buf[NUMBER_OF(buf) - 1] = L'\0';
		if (isTheFirstTime)
			className = buf;
		else
			className = wstringi(buf) + L":" + className;

		// get title name.  When i_hwnd is NULL, buf still holds the module file
		// name the class name step just fetched, which needs no escaping.
		std::wstring title;
		if (i_hwnd) {
			GetWindowText(i_hwnd, buf, NUMBER_OF(buf));
			buf[NUMBER_OF(buf) - 1] = L'\0';
			title = escapeControlChars(buf);
		} else {
			title = buf;
		}
		if (isTheFirstTime)
			titleName = title;
		else
			titleName = title + L":" + titleName;

		// next loop or exit
		if (!i_hwnd)
			break;
		i_hwnd = GetParent(i_hwnd);
		isTheFirstTime = false;
	}
}


/// update show
static void updateShow(HWND i_hwnd, NotifyShow::Show i_show)
{
	bool isMDI = false;

	if (!i_hwnd)
		return;

	LONG_PTR style = GetWindowLongPtr(i_hwnd, GWL_STYLE);
	if ((style & (WS_MAXIMIZEBOX | WS_MINIMIZEBOX)) == 0)
		return; // ignore window that has neither maximize or minimize button

	if (style & WS_CHILD) {
		LONG_PTR exStyle = GetWindowLongPtr(i_hwnd, GWL_EXSTYLE);
		if (exStyle & WS_EX_MDICHILD) {
			isMDI = true;
		} else
			return; // ignore non-MDI child window case
	}

	notifyShow(i_show, isMDI);
}


/// notify WM_Targetted
static void notifyName(HWND i_hwnd, Notify::Type i_type = Notify::Type_name)
{
	wstringi className;
	std::wstring titleName;
	getClassNameTitleName(i_hwnd, t.m_isInMenu, &className, &titleName);

	NotifySetFocus nfc;
	nfc.m_type = i_type;
	nfc.m_threadId = GetCurrentThreadId();
	nfc.setHwnd(i_hwnd);
	wcslcpy(nfc.m_className, className.c_str(), NUMBER_OF(nfc.m_className));
	wcslcpy(nfc.m_titleName, titleName.c_str(), NUMBER_OF(nfc.m_titleName));

	notify(&nfc, sizeof(nfc));
}


/// notify WM_SETFOCUS
static void notifySetFocus(bool i_doesForce = false)
{
	HWND hwnd = GetFocus();
	if (i_doesForce || hwnd != t.m_hwndFocus) {
		t.m_hwndFocus = hwnd;
		// A thread with no focus window says nothing about it that nyamy can
		// use, and nyamy drops such a notification on arrival.  This happens on
		// every activation - WM_ACTIVATEAPP reaches us before the focus is
		// assigned, WM_SETFOCUS follows some milliseconds later with the real
		// window - and once per UI thread, so sending it is a mailslot write
		// per thread per activation spent on nothing.  The assignment above
		// still happens, so the real window that follows is seen as a change.
		if (hwnd)
			notifyName(hwnd, Notify::Type_setFocus);
	}
}


/// notify sync
static void notifySync()
{
	Notify n;
	n.m_type = Notify::Type_sync;
	notify(&n, sizeof(n));
}


/// notify DLL_THREAD_DETACH
static void notifyThreadDetach()
{
	NotifyThreadDetach ntd;
	ntd.m_type = Notify::Type_threadDetach;
	ntd.m_threadId = GetCurrentThreadId();
	notify(&ntd, sizeof(ntd));
}


/// notify WM_COMMAND, WM_SYSCOMMAND
static void notifyCommand(
	HWND i_hwnd, UINT i_message, WPARAM i_wParam, LPARAM i_lParam)
{
	if (g_hookData->m_doesNotifyCommand) {
#ifdef _WIN64
		NotifyCommand64 ntc;
		ntc.m_type = Notify::Type_command64;
#else // !_WIN64
		NotifyCommand32 ntc;
		ntc.m_type = Notify::Type_command32;
#endif // !_WIN64
		ntc.setHwnd(i_hwnd);
		ntc.m_message = i_message;
		ntc.m_wParam = i_wParam;
		ntc.m_lParam = i_lParam;
		notify(&ntc, sizeof(ntc));
	}
}


/// notify show of current window
static void notifyShow(NotifyShow::Show i_show, bool i_isMDI)
{
	NotifyShow ns;
	ns.m_type = Notify::Type_show;
	ns.m_show = i_show;
	ns.m_isMDI = i_isMDI;
	notify(&ns, sizeof(ns));
}


/// &Recenter
static void funcRecenter(HWND i_hwnd)
{
	wchar_t buf[MAX(GANA_MAX_PATH, GANA_MAX_ATOM_LENGTH)];
	GetClassName(i_hwnd, buf, NUMBER_OF(buf));
	bool isEdit;
	if (_wcsicmp(buf, L"Edit") == 0)
		isEdit = true;
	else if (_wcsnicmp(buf, L"RichEdit", 8) == 0)
		isEdit = false;
	else
		return;	// this function only works for Edit control

	LONG_PTR style = GetWindowLongPtr(i_hwnd, GWL_STYLE);
	if (!(style & ES_MULTILINE))
		return;	// this function only works for multi line Edit control

	RECT rc;
	GetClientRect(i_hwnd, &rc);
	POINTL p = { (rc.right + rc.left) / 2, (rc.top + rc.bottom) / 2 };
	int line;
	if (isEdit) {
		line = static_cast<int>(SendMessage(i_hwnd, EM_CHARFROMPOS, 0, MAKELPARAM(p.x, p.y)));
		line = HIWORD(line);
	} else {
		int ci = static_cast<int>(SendMessage(i_hwnd, EM_CHARFROMPOS, 0, (LPARAM)&p));
		line = static_cast<int>(SendMessage(i_hwnd, EM_EXLINEFROMCHAR, 0, ci));
	}
	int caretLine = static_cast<int>(SendMessage(i_hwnd, EM_LINEFROMCHAR, static_cast<WPARAM>(-1), 0));
	SendMessage(i_hwnd, EM_LINESCROLL, 0, caretLine - line);
}


// &SetImeStatus
static void funcSetImeStatus(HWND i_hwnd, int i_status)
{
	HIMC hIMC;

	hIMC = ImmGetContext(i_hwnd);
	if (hIMC == INVALID_HANDLE_VALUE)
		return;

	if (i_status < 0)
		i_status = static_cast<int>(!ImmGetOpenStatus(hIMC));

	ImmSetOpenStatus(hIMC, i_status);
	ImmReleaseContext(i_hwnd, hIMC);
}


// &SetImeString
static void funcSetImeString(HWND i_hwnd, int i_size)
{
	std::vector<wchar_t> buf(i_size);
	DWORD len = 0;
	wchar_t ImeDesc[GANA_MAX_ATOM_LENGTH];
	UINT ImeDescLen;
	DWORD error;
	DWORD denom = 1;
	HANDLE hPipe
	= CreateFile(addSessionId(HOOK_PIPE_NAME).c_str(), GENERIC_READ,
				 FILE_SHARE_READ, (SECURITY_ATTRIBUTES *)NULL,
				 OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, (HANDLE)NULL);
	error = ReadFile(hPipe, buf.data(), i_size, &len, NULL);
	CloseHandle(hPipe);

	ImeDescLen = ImmGetDescription(GetKeyboardLayout(0),
								   ImeDesc, NUMBER_OF(ImeDesc));
	if (wcsncmp(ImeDesc, L"SKKIME", ImeDescLen) > 0)
		denom = sizeof(wchar_t);

	HIMC hIMC = ImmGetContext(i_hwnd);
	if (hIMC == INVALID_HANDLE_VALUE)
		return;

	int status = ImmGetOpenStatus(hIMC);
	ImmSetCompositionString(hIMC, SCS_SETSTR, buf.data(), len / denom, NULL, 0);
	ImmNotifyIME(hIMC, NI_COMPOSITIONSTR, CPS_COMPLETE, 0);
	if (!status)
		ImmSetOpenStatus(hIMC, status);
	ImmReleaseContext(i_hwnd, hIMC);
}

/// notify lock state
/*DllExport*/
void notifyLockState(int i_cause)
{
	NotifyLockState n;
	n.m_type = Notify::Type_lockState;
	n.m_isNumLockToggled = !!(GetKeyState(VK_NUMLOCK) & 1);
	n.m_isCapsLockToggled = !!(GetKeyState(VK_CAPITAL) & 1);
	n.m_isScrollLockToggled = !!(GetKeyState(VK_SCROLL) & 1);
	n.m_isKanaLockToggled = !!(GetKeyState(VK_KANA) & 1);
	n.m_isImeLockToggled = g.m_isImeLock;
	n.m_isImeCompToggled = g.m_isImeCompositioning;
	n.m_debugParam = i_cause;
	notify(&n, sizeof(n));
}

DllExport void notifyLockState()
{
	notifyLockState(9);
}


/// hook of GetMessage
LRESULT CALLBACK getMessageProc(int i_nCode, WPARAM i_wParam, LPARAM i_lParam)
{
	if (!g.m_isInitialized)
		initialize(false);

	if (!g_hookData)
		return 0;

	MSG &msg = (*(MSG *)i_lParam);

	if (i_wParam != PM_REMOVE)
		goto finally;

	switch (msg.message) {
	case WM_COMMAND:
	case WM_SYSCOMMAND:
		notifyCommand(msg.hwnd, msg.message, msg.wParam, msg.lParam);
		break;
	case WM_KEYDOWN:
	case WM_KEYUP:
	case WM_SYSKEYDOWN:
	case WM_SYSKEYUP: {
		if (HIMC hIMC = ImmGetContext(msg.hwnd)) {
			bool prev = g.m_isImeLock;
			g.m_isImeLock = !!ImmGetOpenStatus(hIMC);
			ImmReleaseContext(msg.hwnd, hIMC);
			if (prev != g.m_isImeLock) {
				notifyLockState(1);
			}
		}
		int nVirtKey = (int)msg.wParam;
		// int repeatCount = (msg.lParam & 0xffff);
		BYTE scanCode   = (BYTE)((msg.lParam >> 16) & 0xff);
		bool isExtended = !!(msg.lParam & (1 << 24));
		// bool isAltDown  = !!(msg.lParam & (1 << 29));
		// bool isKeyup    = !!(msg.lParam & (1 << 31));

		if (nVirtKey == VK_CAPITAL ||
				nVirtKey == VK_NUMLOCK ||
				nVirtKey == VK_KANA ||
				nVirtKey == VK_SCROLL)
			notifyLockState(2);
		// m_syncKey is 0 whenever no &Sync is outstanding.  The test matters:
		// keybd_event() reports scan code 0 for a virtual key MapVirtualKey()
		// cannot translate, which would otherwise look like the sync key.
		else if (g_hookData->m_syncKey != 0 &&
				 scanCode == g_hookData->m_syncKey &&
				 isExtended == g_hookData->m_syncKeyIsExtended)
			notifySync();
		break;
	}
	case WM_IME_STARTCOMPOSITION:
		g.m_isImeCompositioning = true;
		notifyLockState(3);
		break;
	case WM_IME_ENDCOMPOSITION:
		g.m_isImeCompositioning = false;
		notifyLockState(4);
		break;
	default:
		if (i_wParam == PM_REMOVE && msg.message == g.m_WM_MAYU_MESSAGE) {
			switch (msg.wParam) {
			case MayuMessage_notifyName:
				notifyName(msg.hwnd);
				break;
			case MayuMessage_funcRecenter:
				funcRecenter(msg.hwnd);
				break;
			case MayuMessage_funcSetImeStatus:
				funcSetImeStatus(msg.hwnd, (int)msg.lParam);
				break;
			case MayuMessage_funcSetImeString:
				funcSetImeString(msg.hwnd, (int)msg.lParam);
				break;
			}
		}
		break;
	}
finally:
	return CallNextHookEx(s_hookDataArch->m_hHookGetMessage,
						  i_nCode, i_wParam, i_lParam);
}


/// hook of SendMessage
LRESULT CALLBACK callWndProc(int i_nCode, WPARAM i_wParam, LPARAM i_lParam)
{
	if (!g.m_isInitialized)
		initialize(false);

	if (!g_hookData)
		return 0;

	CWPSTRUCT &cwps = *(CWPSTRUCT *)i_lParam;

	if (0 <= i_nCode) {
		switch (cwps.message) {
		case WM_ACTIVATEAPP:
		case WM_NCACTIVATE:
			if (i_wParam)
				notifySetFocus();
			break;
		case WM_SYSCOMMAND:
			switch (cwps.wParam) {
			case SC_MAXIMIZE:
			case SC_MAXIMIZE2:
				updateShow(cwps.hwnd, NotifyShow::Show_Maximized);
				break;
			case SC_MINIMIZE:
			case SC_MINIMIZE2:
				updateShow(cwps.hwnd, NotifyShow::Show_Minimized);
				break;
			case SC_RESTORE:
			case SC_RESTORE2:
				updateShow(cwps.hwnd, NotifyShow::Show_Normal);
				break;
			default:
				break;
			}
			[[fallthrough]];
		case WM_COMMAND:
			notifyCommand(cwps.hwnd, cwps.message, cwps.wParam, cwps.lParam);
			break;
		case WM_SIZE:
			switch (cwps.wParam) {
			case SIZE_MAXIMIZED:
				updateShow(cwps.hwnd, NotifyShow::Show_Maximized);
				break;
			case SIZE_MINIMIZED:
				updateShow(cwps.hwnd, NotifyShow::Show_Minimized);
				break;
			case SIZE_RESTORED:
				updateShow(cwps.hwnd, NotifyShow::Show_Normal);
				break;
			default:
				break;
			}
			break;
		case WM_MOUSEACTIVATE:
			notifySetFocus();
			break;
		case WM_ACTIVATE:
			if (LOWORD(cwps.wParam) != WA_INACTIVE) {
				notifySetFocus();
				if (HIWORD(cwps.wParam)) { // check minimized flag
					// minimized flag on
					notifyShow(NotifyShow::Show_Minimized, false);
					//notifyShow(NotifyShow::Show_Normal, true);
				}
			}
			break;
		case WM_ENTERMENULOOP:
			t.m_isInMenu = true;
			notifySetFocus(true);
			break;
		case WM_EXITMENULOOP:
			t.m_isInMenu = false;
			notifySetFocus(true);
			break;
		case WM_SETFOCUS:
			t.m_isInMenu = false;
			// for kana
			if (g_hookData->m_correctKanaLockHandling) {
				if (HIMC hIMC = ImmGetContext(cwps.hwnd)) {
					bool status = !!ImmGetOpenStatus(hIMC);
					// this code set the VK_KANA state correctly.
					ImmSetOpenStatus(hIMC, !status);
					ImmSetOpenStatus(hIMC, status);
					ImmReleaseContext(cwps.hwnd, hIMC);
				}
			}
			notifySetFocus();
			notifyLockState(5);
			break;
		case WM_IME_STARTCOMPOSITION:
			g.m_isImeCompositioning = true;
			notifyLockState(6);
			break;
		case WM_IME_ENDCOMPOSITION:
			g.m_isImeCompositioning = false;
			notifyLockState(7);
			break;
		case WM_IME_NOTIFY:
			if (cwps.wParam == IMN_SETOPENSTATUS)
				if (HIMC hIMC = ImmGetContext(cwps.hwnd)) {
					g.m_isImeLock = !!ImmGetOpenStatus(hIMC);
					ImmReleaseContext(cwps.hwnd, hIMC);
					notifyLockState(8);
				}
			break;
		}
	}
	return CallNextHookEx(s_hookDataArch->m_hHookCallWndProc, i_nCode,
						  i_wParam, i_lParam);
}


static LRESULT CALLBACK lowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	if (!g.m_isInitialized)
		initialize(false);

	if (!g_hookData || nCode < 0)
		goto through;

	if (g.m_mouseDetour && g.m_engine) {
		unsigned int result;
		result = g.m_mouseDetour(g.m_engine, wParam, lParam);
		if (result) {
			return 1;
		}
	}

through:
	return CallNextHookEx(g.m_hHookMouseProc,
						  nCode, wParam, lParam);
}


static LRESULT CALLBACK lowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	if (!g.m_isInitialized)
		initialize(false);

	if (!g_hookData || nCode < 0)
		goto through;

	if (g.m_keyboardDetour && g.m_engine) {
		unsigned int result;
		result = g.m_keyboardDetour(g.m_engine, wParam, lParam);
		if (result) {
			return 1;
		}
	}
through:
	return CallNextHookEx(g.m_hHookKeyboardProc,
						  nCode, wParam, lParam);
}


/// install message hook
DllExport int installMessageHook(HWND i_hwndTaskTray)
{
	if (!g.m_isInitialized)
		initialize(true);

	if (i_hwndTaskTray) {
		g_hookData->setHwndTaskTray(i_hwndTaskTray);
	}
	g.m_hwndTaskTray = g_hookData->getHwndTaskTray();
	s_hookDataArch->m_hHookGetMessage =
		SetWindowsHookEx(WH_GETMESSAGE, (HOOKPROC)getMessageProc,
						 g.m_hInstDLL, 0);
	s_hookDataArch->m_hHookCallWndProc =
		SetWindowsHookEx(WH_CALLWNDPROC, (HOOKPROC)callWndProc, g.m_hInstDLL, 0);
	return 0;
}


/// uninstall message hook
DllExport int uninstallMessageHook()
{
	if (s_hookDataArch->m_hHookGetMessage)
		UnhookWindowsHookEx(s_hookDataArch->m_hHookGetMessage);
	s_hookDataArch->m_hHookGetMessage = NULL;
	if (s_hookDataArch->m_hHookCallWndProc)
		UnhookWindowsHookEx(s_hookDataArch->m_hHookCallWndProc);
	s_hookDataArch->m_hHookCallWndProc = NULL;
	g.m_hwndTaskTray = 0;

	// Windows unloads a global-hook DLL from each injected process only
	// when that process next retrieves a message.  Idle GUI processes
	// would otherwise keep this DLL mapped indefinitely, blocking
	// relink of the DLL file.  Wake every GUI thread with a harmless
	// WM_NULL so the DLL gets unloaded promptly.
	SendNotifyMessage(HWND_BROADCAST, WM_NULL, 0, 0);
	return 0;
}


/// install keyboard hook
DllExport int installKeyboardHook(INPUT_DETOUR i_keyboardDetour, Engine *i_engine, bool i_install)
{
	if (i_install) {
		if (!g.m_isInitialized)
			initialize(true);

		g.m_keyboardDetour = i_keyboardDetour;
		g.m_engine = i_engine;
		g.m_hHookKeyboardProc =
			SetWindowsHookEx(WH_KEYBOARD_LL, (HOOKPROC)lowLevelKeyboardProc,
							 g.m_hInstDLL, 0);
	} else {
		if (g.m_hHookKeyboardProc)
			UnhookWindowsHookEx(g.m_hHookKeyboardProc);
		g.m_hHookKeyboardProc = NULL;
	}
	return 0;
}


/// install mouse hook
DllExport int installMouseHook(INPUT_DETOUR i_mouseDetour, Engine *i_engine, bool i_install)
{
	if (IsDebuggerPresent()) {
		// If the process is being debugged, installing a mouse hook may cause deadlock.
		return 0;
	}
	if (i_install) {
		if (!g.m_isInitialized)
			initialize(true);

		g.m_mouseDetour = i_mouseDetour;
		g.m_engine = i_engine;
		g_hookData->m_mouseHookType = MouseHookType_None;
		g.m_hHookMouseProc =
			SetWindowsHookEx(WH_MOUSE_LL, (HOOKPROC)lowLevelMouseProc,
							 g.m_hInstDLL, 0);
	} else {
		if (g.m_hHookMouseProc)
			UnhookWindowsHookEx(g.m_hHookMouseProc);
		g.m_hHookMouseProc = NULL;
	}
	return 0;
}


/// emergency unhook all hooks (safe to call from crash handlers)
DllExport void emergencyUnhookAll()
{
	if (g.m_hHookKeyboardProc) {
		UnhookWindowsHookEx(g.m_hHookKeyboardProc);
		g.m_hHookKeyboardProc = NULL;
	}
	if (g.m_hHookMouseProc) {
		UnhookWindowsHookEx(g.m_hHookMouseProc);
		g.m_hHookMouseProc = NULL;
	}
	if (s_hookDataArch) {
		if (s_hookDataArch->m_hHookGetMessage) {
			UnhookWindowsHookEx(s_hookDataArch->m_hHookGetMessage);
			s_hookDataArch->m_hHookGetMessage = NULL;
		}
		if (s_hookDataArch->m_hHookCallWndProc) {
			UnhookWindowsHookEx(s_hookDataArch->m_hHookCallWndProc);
			s_hookDataArch->m_hHookCallWndProc = NULL;
		}
	}
}
