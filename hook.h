//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// hook.h


#ifndef _HOOK_H
#  define _HOOK_H

#  include "misc.h"
#  include <windef.h>

///
#  define HOOK_PIPE_NAME \
 L"\\\\.\\pipe\\GANAware\\mayu\\{4B22D464-7A4E-494b-982A-C2B2BBAAF9F3}" WIDEN(VERSION)
///
#  define NOTIFY_MAILSLOT_NAME \
L"\\\\.\\mailslot\\GANAware\\mayu\\{330F7914-EB5B-49be-ACCE-D2B8DF585B32}" WIDEN(VERSION)
///
#  define WM_MAYU_MESSAGE_NAME L"GANAware\\mayu\\WM_MAYU_MESSAGE"

///
enum MayuMessage {
	MayuMessage_notifyName,
	MayuMessage_funcRecenter,
	MayuMessage_funcSetImeStatus,
	MayuMessage_funcSetImeString,
};


///
struct Notify {
	///
	enum Type {
		Type_setFocus,				/// NotifySetFocus
		Type_name,					/// NotifySetFocus
		Type_lockState,				/// NotifyLockState
		Type_sync,					/// Notify
		Type_threadAttach,				/// NotifyThreadAttach
		Type_threadDetach,				/// NotifyThreadDetach
		Type_command64,				/// NotifyCommand64
		Type_command32,				/// NotifyCommand32
		Type_show,					/// NotifyShow
		Type_log,					/// NotifyLog
	};
	Type m_type;					///
	DWORD m_debugParam;				/// (for debug)
};


///
struct NotifySetFocus : public Notify {
	DWORD m_threadId;				///
	DWORD _m_hwnd;				///
	wchar_t m_className[GANA_MAX_PATH];		///
	wchar_t m_titleName[GANA_MAX_PATH];		///

	inline HWND getHwnd() const {
		return reinterpret_cast<HWND>(static_cast<uintptr_t>(_m_hwnd));
	}

	inline void setHwnd(HWND i_hwnd) {
		_m_hwnd = static_cast<DWORD>(reinterpret_cast<uintptr_t>(i_hwnd));
    }
};


///
struct NotifyLockState : public Notify {
	bool m_isNumLockToggled;			///
	bool m_isCapsLockToggled;			///
	bool m_isScrollLockToggled;			///
	bool m_isKanaLockToggled;			///
	bool m_isImeLockToggled;			///
	bool m_isImeCompToggled;			///
};


///
struct NotifyThreadAttach : public Notify {
	DWORD m_threadId;				///
};


///
struct NotifyThreadDetach : public Notify {
	DWORD m_threadId;				///
};


///
struct NotifyCommand32 : public Notify {
	DWORD _m_hwnd;				///
	UINT m_message;				///
	unsigned int m_wParam;				///
	long m_lParam;				///

	inline HWND getHwnd() const {
		return reinterpret_cast<HWND>(static_cast<uintptr_t>(_m_hwnd));
	}

	inline void setHwnd(HWND i_hwnd) {
		_m_hwnd = static_cast<DWORD>(reinterpret_cast<uintptr_t>(i_hwnd));
	}
};


///
struct NotifyCommand64 : public Notify {
	DWORD _m_hwnd;				///
	UINT m_message;				///
	unsigned __int64 m_wParam;				///
	__int64 m_lParam;				///

	inline HWND getHwnd() const {
		return reinterpret_cast<HWND>(static_cast<uintptr_t>(_m_hwnd));
	}

	inline void setHwnd(HWND i_hwnd) {
		_m_hwnd = static_cast<DWORD>(reinterpret_cast<uintptr_t>(i_hwnd));
	}
};


enum {
	NOTIFY_MESSAGE_SIZE = sizeof(NotifySetFocus),	///
};


///
struct NotifyShow : public Notify {
	///
	enum Show {
		Show_Normal,
		Show_Maximized,
		Show_Minimized,
	};
	Show m_show;					///
	bool m_isMDI;					///
};


///
struct NotifyLog : public Notify {
	wchar_t m_msg[GANA_MAX_PATH];			///
};


// The 32-bit hook DLL, the 64-bit hook DLL and nyamy.exe write and read
// these over one channel, so each of them has to have the same layout in
// every build.  Pointer sized members are what breaks that - hence the DWORD
// plus accessors for every window handle above.  Pin the sizes down here so
// a member added without that care fails to build rather than to parse.
static_assert(sizeof(Notify) == 8, "Notify layout is not build independent");
static_assert(sizeof(NotifySetFocus) == 4176, "NotifySetFocus layout is not build independent");
static_assert(sizeof(NotifyLockState) == 16, "NotifyLockState layout is not build independent");
static_assert(sizeof(NotifyThreadAttach) == 12, "NotifyThreadAttach layout is not build independent");
static_assert(sizeof(NotifyThreadDetach) == 12, "NotifyThreadDetach layout is not build independent");
static_assert(sizeof(NotifyCommand32) == 24, "NotifyCommand32 layout is not build independent");
static_assert(sizeof(NotifyCommand64) == 32, "NotifyCommand64 layout is not build independent");
static_assert(sizeof(NotifyShow) == 16, "NotifyShow layout is not build independent");
static_assert(sizeof(NotifyLog) == 2088, "NotifyLog layout is not build independent");

// the receiver reads into a buffer of this size, so nothing may exceed it
static_assert(sizeof(NotifyLog) <= NOTIFY_MESSAGE_SIZE &&
			  sizeof(NotifyShow) <= NOTIFY_MESSAGE_SIZE,
			  "a notification no longer fits in NOTIFY_MESSAGE_SIZE");


///
enum MouseHookType : int {
	MouseHookType_None = 0,				/// none
	MouseHookType_Wheel = 1 << 0,			/// wheel
	MouseHookType_WindowMove = 1 << 1,		/// window move
};

class Engine;
using INPUT_DETOUR = unsigned int (WINAPI *)(Engine *i_engine, WPARAM i_wParam, LPARAM i_lParam);

///
class HookData
{
public:
	USHORT m_syncKey;				///
	bool m_syncKeyIsExtended;			///
	bool m_doesNotifyCommand;			///
	DWORD _m_hwndTaskTray;				///
	bool m_correctKanaLockHandling;		/// does use KL- ?
	MouseHookType m_mouseHookType;		///
	int m_mouseHookParam;			///
	DWORD _m_hwndMouseHookTarget;		///
	POINT m_mousePos;				///

	inline HWND getHwndTaskTray() const {
		return reinterpret_cast<HWND>(static_cast<uintptr_t>(_m_hwndTaskTray));
    }

	inline void setHwndTaskTray(HWND i_hwnd) {
		_m_hwndTaskTray = static_cast<DWORD>(reinterpret_cast<uintptr_t>(i_hwnd));
	}

	inline HWND getHwndMouseHookTarget() const {
		return reinterpret_cast<HWND>(static_cast<uintptr_t>(_m_hwndMouseHookTarget));
    }

	inline void setHwndMouseHookTarget(HWND i_hwnd) {
		_m_hwndMouseHookTarget = static_cast<DWORD>(reinterpret_cast<uintptr_t>(i_hwnd));
	}
};


///
#  define DllExport __declspec(dllexport)
///
#  define DllImport __declspec(dllimport)


#  ifndef _HOOK_CPP
// hook_stub.cpp's DllExport definitions are linked into this same binary
// (unit tests), not a separate DLL; a dllimport decl here would make the
// linker warn LNK4217 about importing a locally defined symbol.
#    ifdef HOOK_STATIC_LINK
#      define HookDllImport
#    else
#      define HookDllImport DllImport
#    endif // HOOK_STATIC_LINK
extern HookDllImport HookData *g_hookData;
extern HookDllImport int installMessageHook(HWND i_hwndTaskTray);
extern HookDllImport int uninstallMessageHook();
extern HookDllImport int installKeyboardHook(INPUT_DETOUR i_keyboardDetour, Engine *i_engine, bool i_install);
extern HookDllImport int installMouseHook(INPUT_DETOUR i_mouseDetour, Engine *i_engine, bool i_install);
extern HookDllImport bool notify(void *data, size_t sizeof_data);
extern HookDllImport void notifyLockState();
extern HookDllImport void emergencyUnhookAll();
#  endif // !_HOOK_CPP


#endif // !_HOOK_H
