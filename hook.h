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
	HWND m_hwnd;					///
	UINT m_message;				///
	unsigned int m_wParam;				///
	long m_lParam;				///
};


///
struct NotifyCommand64 : public Notify {
	HWND m_hwnd;					///
	UINT m_message;				///
	unsigned __int64 m_wParam;				///
	__int64 m_lParam;				///
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


///
enum MouseHookType {
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
extern DllImport HookData *g_hookData;
extern DllImport int installMessageHook(HWND i_hwndTaskTray);
extern DllImport int uninstallMessageHook();
extern DllImport int installKeyboardHook(INPUT_DETOUR i_keyboardDetour, Engine *i_engine, bool i_install);
extern DllImport int installMouseHook(INPUT_DETOUR i_mouseDetour, Engine *i_engine, bool i_install);
extern DllImport bool notify(void *data, size_t sizeof_data);
extern DllImport void notifyLockState();
extern DllImport void emergencyUnhookAll();
#  endif // !_HOOK_CPP


#endif // !_HOOK_H
