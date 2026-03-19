#include <list>
#include <windows.h>
#include "registry.h"

using FpGetModuleHandleW = HMODULE (WINAPI *)(LPCWSTR);
using FpGetProcAddress = FARPROC (WINAPI *)(HMODULE, LPCSTR);
using FpUpdatePerUserSystemParameters4 = BOOL (WINAPI *)(BOOL);
using FpUpdatePerUserSystemParameters8 = BOOL (WINAPI *)(DWORD, BOOL);
using FpOpenProcess = HANDLE (WINAPI *)(DWORD, BOOL, DWORD);
using FpOpenProcessToken = BOOL (WINAPI *)(HANDLE, DWORD, PHANDLE);
using FpImpersonateLoggedOnUser = BOOL (WINAPI *)(HANDLE);
using FpRevertToSelf = BOOL (WINAPI *)(VOID);
using FpCloseHandle = BOOL (WINAPI *)(HANDLE);

struct InjectInfo {
	DWORD isVistaOrLater_;
	DWORD pid_;
	WCHAR advapi32_[64];
	CHAR impersonateLoggedOnUser_[32];
	CHAR revertToSelf_[32];
	CHAR openProcessToken_[32];
	FpGetModuleHandleW pGetModuleHandle;
	FpGetProcAddress pGetProcAddress;
	FpUpdatePerUserSystemParameters4 pUpdate4;
	FpUpdatePerUserSystemParameters8 pUpdate8;
	FpOpenProcess pOpenProcess;
	FpCloseHandle pCloseHandle;
};

class FixScancodeMap {
private:
	struct ScancodeMap {
		DWORD header1;
		DWORD header2;
		DWORD count;
		DWORD entry[1];
	};

	struct WlInfo {
		HANDLE m_hProcess;
		LPVOID m_remoteMem;
		LPVOID m_remoteInfo;
		HANDLE m_hThread;
	};

private:
	static const DWORD s_fixEntryNum;
	static const DWORD s_fixEntry[];

private:
	HWND m_hwnd;
	UINT m_messageOnFail;
	int m_errorOnConstruct;
	DWORD m_winlogonPid;
	std::list<WlInfo> m_wlTrash;
	InjectInfo m_info;
	Registry m_regHKCU;
	Registry m_regHKLM;
	Registry *m_pReg;
	HANDLE m_hFixEvent;
	HANDLE m_hRestoreEvent;
	HANDLE m_hQuitEvent;
	HANDLE m_hThread;
	unsigned m_threadId;

private:
	int acquirePrivileges();
	DWORD getWinLogonPid();
	static bool clean(WlInfo wl);
	int injectThread(DWORD dwPID);
	int update();
	int fix();
	int restore();
	static unsigned int WINAPI threadLoop(void *i_this);

public:
	FixScancodeMap();
	~FixScancodeMap();

	int init(HWND i_hwnd, UINT i_messageOnFail);
	int escape(bool i_escape);

	/// signal the thread to quit (non-blocking)
	void signalQuit();
	/// detach and return the thread handle (sets internal handle to NULL)
	HANDLE detachThread();
};
