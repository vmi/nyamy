//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// mayu.cpp


#define APSTUDIO_INVOKED

#include "misc.h"
#include "compiler_specific_func.h"
#include "dlginvestigate.h"
#include "dlglog.h"
#include "dlgsetting.h"
#include "dlgversion.h"
#include "engine.h"
#include "errormessage.h"
#include "focus.h"
#include "function.h"
#include "hook.h"
#include "mayu.h"
#include "mayuipc.h"
#include "mayurc.h"
#include "msgstream.h"
#include "multithread.h"
#include "inifile.h"
#include "nyamy_paths.h"
#include "setting.h"
#include "scripter_manager.h"
#include "target.h"
#include "windowstool.h"
#include "vk2tchar.h"
#include <process.h>
#include <time.h>
#include <commctrl.h>
#include <wtsapi32.h>
#include <aclapi.h>
#include <cwchar>


///
#define ID_MENUITEM_reloadBegin _APS_NEXT_COMMAND_VALUE


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Mayu


///
class Mayu
{
	HWND m_hwndTaskTray;				/// tasktray window
	HWND m_hwndLog;				/// log dialog
	HWND m_hwndInvestigate;			/// investigate dialog
	HWND m_hwndVersion;				/// version dialog

	UINT m_WM_TaskbarRestart;			/** window message sent when
                                                    taskber restarts */
	UINT m_WM_MayuIPC;				/** IPC message sent from
						    other applications */
	NOTIFYICONDATA m_ni;				/// taskbar icon data
	HICON m_tasktrayIcon[2];			/// taskbar icon
	int m_tasktrayIconRetries;			/// attempts left at adding it
	bool m_canUseTasktrayBaloon;			///

	womsgstream m_log;				/** log stream (output to log
						    dialog's edit) */
#ifdef LOG_TO_FILE
	std::wofstream m_logFile;
#endif // LOG_TO_FILE

	HMENU m_hMenuTaskTray;			/// tasktray menu
	HANDLE m_hMutexYamyd;
	STARTUPINFO m_si;
	PROCESS_INFORMATION m_pi;
	HANDLE m_mutex;
	HANDLE m_hNotifyMailslot;			/// mailslot to receive notify
	HANDLE m_hNotifyThread;			/// thread reading the mailslot
	bool volatile m_notifyReaderStop;		/// asks that thread to finish
	BYTE m_notifyBuf[NOTIFY_MESSAGE_SIZE];
	static const DWORD SESSION_LOCKED = 1<<0;
	static const DWORD SESSION_DISCONNECTED = 1<<1;
	static const DWORD SESSION_END_QUERIED = 1<<2;
	DWORD m_sessionState;

	bool m_isSettingDialogOpened;			/// is setting dialog opened ?

	Engine m_engine;				/// engine
	std::unique_ptr<ScripterManager> m_scripter;	/// scripter subprocess manager

	bool m_usingSN;		   /// using WTSRegisterSessionNotification() ?
	time_t m_startTime;				/// mayu started at ...

	enum {
		WM_APP_taskTrayNotify = WM_APP + 101,	///
		WM_APP_msgStreamNotify = WM_APP + 102,	///
		WM_APP_scripterSettingReady = ScripterManager::WM_ScripterSettingReady,	///< scripter generated Setting
		ID_TaskTrayIcon = 1,			///
		ID_TaskTrayIconRetryTimer = 2,		///< timer retrying the icon
	};

private:
	static unsigned int WINAPI notifyReaderProc(void *i_this) {
		reinterpret_cast<Mayu *>(i_this)->notifyReader();
		_endthreadex(0);
		return 0;
	}

	/** Read notifications from the hooks and dispatch them.

	    This has a thread of its own rather than being a completion routine on
	    the UI thread, because an APC is delivered only while its thread sits
	    in an alertable wait.  A tracked popup menu, a modal dialog or any long
	    call out of the UI thread stops delivery - and &Sync parks the engine
	    thread until its notification comes back through here, so it would then
	    always run into its timeout.  Focus and lock state notifications were
	    equally stuck behind such a UI thread.

	    notifyHandler() can consequently run here and, through the WM_COPYDATA
	    fallback hook.cpp uses when it cannot open the mailslot, on the UI
	    thread at the same time.  What it touches is either its own argument or
	    guarded: the engine behind its mutex, the log behind its own. */
	void notifyReader() {
		while (!m_notifyReaderStop) {
			memset(m_notifyBuf, 0, sizeof(m_notifyBuf));

			DWORD len = 0;
			if (!ReadFile(m_hNotifyMailslot, m_notifyBuf, sizeof(m_notifyBuf),
						  &len, NULL)) {
				DWORD err = GetLastError();
				// Anything but our own cancellation ends notifications for the
				// rest of the session - as it did before, but silently.  Say so
				// rather than leave a nyamy that quietly stops following focus.
				if (!m_notifyReaderStop && err != ERROR_OPERATION_ABORTED) {
					Acquire a(&m_log, LogLevel::Error);
					m_log << L"internal error: cannot read notifications (0x"
					<< std::hex << err << std::dec
					<< L"); focus and lock state will no longer follow."
					<< std::endl;
				}
				break;
			}
			if (m_notifyReaderStop)
				break;

			// anyone in the session can write here, so do not trust the length
			if (len < sizeof(Notify))
				continue;

			COPYDATASTRUCT cd = {
				.dwData = static_cast<ULONG_PTR>(
					reinterpret_cast<Notify *>(m_notifyBuf)->m_type),
				.cbData = len,
				.lpData = m_notifyBuf,
			};
			notifyHandler(&cd);
		}
	}

	/// start reading notifications
	void startNotifyReader() {
		m_notifyReaderStop = false;
		CHECK_TRUE( m_hNotifyThread = (HANDLE)_beginthreadex(
			NULL, 0, notifyReaderProc, this, 0, NULL) );
	}

	/// stop reading notifications.  m_hNotifyThread is NULL afterwards if,
	/// and only if, the thread is confirmed gone
	void stopNotifyReader() {
		if (!m_hNotifyThread)
			return;
		m_notifyReaderStop = true;
		CancelIoEx(m_hNotifyMailslot, NULL);	// break the blocking ReadFile
		if (WaitForSingleObject(m_hNotifyThread, 2000) == WAIT_OBJECT_0) {
			CloseHandle(m_hNotifyThread);
			m_hNotifyThread = NULL;
		}
		// Otherwise it is still inside ReadFile or notifyHandler and would use
		// this object after it is torn down.  Leave the thread running and the
		// mailslot open rather than pull either away from it; we are exiting.
		// Not expected: CancelIoEx ends the read, and the only thing
		// notifyHandler waits on is the engine mutex.
	}

	/// register class for tasktray
	ATOM Register_tasktray() {
		WNDCLASS wc = {
			.style = 0,
			.lpfnWndProc = tasktray_wndProc,
			.cbClsExtra = 0,
			.cbWndExtra = sizeof(Mayu*),
			.hInstance = g_hInst,
			.hIcon = NULL,
			.hCursor = NULL,
			.hbrBackground = NULL,
			.lpszMenuName = NULL,
			.lpszClassName = L"mayuTasktray",
		};
		return RegisterClass(&wc);
	}

	/// notify handler
	BOOL notifyHandler(COPYDATASTRUCT *cd) {
		switch (cd->dwData) {
		case Notify::Type_setFocus:
		case Notify::Type_name: {
			NotifySetFocus *n = (NotifySetFocus *)cd->lpData;
			n->m_className[NUMBER_OF(n->m_className) - 1] = L'\0';
			n->m_titleName[NUMBER_OF(n->m_titleName) - 1] = L'\0';

			if (n->m_type == Notify::Type_setFocus)
				m_engine.setFocus(n->getHwnd(), n->m_threadId,
								  n->m_className, n->m_titleName, false);

			{
				Acquire a(&m_log, LogLevel::Debug);
				m_log << L"HWND:     " << std::hex
				<< n->_m_hwnd /* always 32bit width when log outout */
				<< std::dec << std::endl;
				m_log << L"THREADID: " << static_cast<int>(n->m_threadId)
				<< std::endl;
			}
			Acquire a(&m_log, (n->m_type == Notify::Type_name) ? LogLevel::Info : LogLevel::Debug);
			m_log << L"CLASS:    " << n->m_className << std::endl;
			m_log << L"TITLE:   \"" << n->m_titleName << L"\"" << std::endl;

			bool isMDI = true;
			HWND hwnd = getToplevelWindow(n->getHwnd(), &isMDI);
			// zeroed because the queries below fail for a NULL window - a
			// notification can carry one - and used to print whatever the
			// stack happened to hold
			RECT rc = {};
			if (isMDI) {
				if (getChildWindowRect(hwnd, &rc))
					m_log << L"MDI Window Position/Size: ("
					<< rc.left << L", " << rc.top << L") / ("
					<< rcWidth(&rc) << L"x" << rcHeight(&rc) << L")"
					<< std::endl;
				hwnd = getToplevelWindow(n->getHwnd(), NULL);
			}

			if (GetWindowRect(hwnd, &rc))
				m_log << L"Toplevel Window Position/Size: ("
				<< rc.left << L", " << rc.top << L") / ("
				<< rcWidth(&rc) << L"x" << rcHeight(&rc) << L")"
				<< std::endl;
			else
				m_log << L"Toplevel Window Position/Size: (unavailable)"
				<< std::endl;

			SystemParametersInfo(SPI_GETWORKAREA, 0, (void *)&rc, FALSE);
			m_log << L"Desktop Window Position/Size: ("
			<< rc.left << L", " << rc.top << L") / ("
			<< rcWidth(&rc) << L"x" << rcHeight(&rc) << L")"
			<< std::endl;

			m_log << std::endl;
			break;
		}

		case Notify::Type_lockState: {
			NotifyLockState *n = (NotifyLockState *)cd->lpData;
			m_engine.setLockState(n->m_isNumLockToggled,
								  n->m_isCapsLockToggled,
								  n->m_isScrollLockToggled,
								  n->m_isKanaLockToggled,
								  n->m_isImeLockToggled,
								  n->m_isImeCompToggled);
#if 0
			Acquire a(&m_log, LogLevel::Info);
			if (n->m_isKanaLockToggled) {
				m_log << L"Notify::Type_lockState Kana on  : ";
			} else {
				m_log << L"Notify::Type_lockState Kana off : ";
			}
			m_log << n->m_debugParam << ", "
			<< g_hookData->m_correctKanaLockHandling << std::endl;
#endif
			break;
		}

		case Notify::Type_sync: {
			m_engine.syncNotify();
			break;
		}

		case Notify::Type_threadAttach: {
			NotifyThreadAttach *n = (NotifyThreadAttach *)cd->lpData;
			m_engine.threadAttachNotify(n->m_threadId);
			break;
		}

		case Notify::Type_threadDetach: {
			NotifyThreadDetach *n = (NotifyThreadDetach *)cd->lpData;
			m_engine.threadDetachNotify(n->m_threadId);
			break;
		}

		case Notify::Type_command64: {
			NotifyCommand64 *n = (NotifyCommand64 *)cd->lpData;
			m_engine.commandNotify(n->getHwnd(), n->m_message,
								   n->m_wParam, n->m_lParam);
			break;
		}

		case Notify::Type_command32: {
			NotifyCommand32 *n = (NotifyCommand32 *)cd->lpData;
			m_engine.commandNotify(n->getHwnd(), n->m_message,
								   n->m_wParam, n->m_lParam);
			break;
		}

		case Notify::Type_show: {
			NotifyShow *n = (NotifyShow *)cd->lpData;
			switch (n->m_show) {
			case NotifyShow::Show_Maximized:
				m_engine.setShow(true, false, n->m_isMDI);
				break;
			case NotifyShow::Show_Minimized:
				m_engine.setShow(false, true, n->m_isMDI);
				break;
			case NotifyShow::Show_Normal:
			default:
				m_engine.setShow(false, false, n->m_isMDI);
				break;
			}
			break;
		}

		case Notify::Type_log: {
			Acquire a(&m_log, LogLevel::Debug);
			NotifyLog *n = (NotifyLog *)cd->lpData;
			m_log << L"hook log: " << n->m_msg << std::endl;
			break;
		}
		}
		return true;
	}

	/// window procedure for tasktray
	static LRESULT CALLBACK
	tasktray_wndProc(HWND i_hwnd, UINT i_message,
					 WPARAM i_wParam, LPARAM i_lParam) {
		Mayu *This = reinterpret_cast<Mayu *>(GetWindowLongPtr(i_hwnd, 0));

		if (!This)
			switch (i_message) {
			case WM_CREATE:
				This = reinterpret_cast<Mayu *>(
						   reinterpret_cast<CREATESTRUCT *>(i_lParam)->lpCreateParams);
				SetWindowLongPtr(i_hwnd, 0, (LONG_PTR)This);
				return 0;
			}
		else
			switch (i_message) {
			case WM_COPYDATA: {
				COPYDATASTRUCT *cd;
				cd = reinterpret_cast<COPYDATASTRUCT *>(i_lParam);
				return This->notifyHandler(cd);
			}
			case WM_QUERYENDSESSION:
				This->m_sessionState |= Mayu::SESSION_END_QUERIED;
				This->m_engine.prepairQuit();
				PostQuitMessage(0);
				return TRUE;

#ifndef WM_WTSSESSION_CHANGE			// WinUser.h
#  define WM_WTSSESSION_CHANGE            0x02B1
#endif
			case WM_WTSSESSION_CHANGE: {
				const char *m = "";
				switch (i_wParam) {
#ifndef WTS_CONSOLE_CONNECT			// WinUser.h
#  define WTS_CONSOLE_CONNECT                0x1
#  define WTS_CONSOLE_DISCONNECT             0x2
#  define WTS_REMOTE_CONNECT                 0x3
#  define WTS_REMOTE_DISCONNECT              0x4
#  define WTS_SESSION_LOGON                  0x5
#  define WTS_SESSION_LOGOFF                 0x6
#  define WTS_SESSION_LOCK                   0x7
#  define WTS_SESSION_UNLOCK                 0x8
#endif
				case WTS_CONSOLE_CONNECT:
					This->m_sessionState &= ~Mayu::SESSION_DISCONNECTED;
					m = "WTS_CONSOLE_CONNECT";
					break;
				case WTS_CONSOLE_DISCONNECT:
					This->m_sessionState |= Mayu::SESSION_DISCONNECTED;
					m = "WTS_CONSOLE_DISCONNECT";
					break;
				case WTS_REMOTE_CONNECT:
					This->m_sessionState &= ~Mayu::SESSION_DISCONNECTED;
					m = "WTS_REMOTE_CONNECT";
					break;
				case WTS_REMOTE_DISCONNECT:
					This->m_sessionState |= Mayu::SESSION_DISCONNECTED;
					m = "WTS_REMOTE_DISCONNECT";
					break;
				case WTS_SESSION_LOGON:
					m = "WTS_SESSION_LOGON";
					break;
				case WTS_SESSION_LOGOFF:
					m = "WTS_SESSION_LOGOFF";
					break;
				case WTS_SESSION_LOCK:
					This->m_sessionState |= Mayu::SESSION_LOCKED;
					m = "WTS_SESSION_LOCK";
					break;
				case WTS_SESSION_UNLOCK: {
					This->m_sessionState &= ~Mayu::SESSION_LOCKED;
					if (!This->m_sessionState) {
						if (This->m_engine.getIsEnabled()) {
							This->m_engine.unlocked();
						}
					}
					m = "WTS_SESSION_UNLOCK";
					break;
				}
					//case WTS_SESSION_REMOTE_CONTROL: m = "WTS_SESSION_REMOTE_CONTROL"; break;
				}
				This->m_log << L"WM_WTSESSION_CHANGE("
				<< i_wParam << ", " << i_lParam << "): "
				<< m << std::endl;
				return TRUE;
			}
			case WM_APP_msgStreamNotify: {
				womsgstream::StreamBuf *log =
					reinterpret_cast<womsgstream::StreamBuf *>(i_lParam);
				const std::wstring &str = log->acquireString();
#ifdef LOG_TO_FILE
				This->m_logFile << str << std::flush;
#endif // LOG_TO_FILE
				// Appending to the edit control is by far the most expensive
				// thing on the log path, and the cost grows with how much the
				// control already holds, so the buffer is kept modest.
				editInsertTextAtLast(GetDlgItem(This->m_hwndLog, IDC_EDIT_log),
									 str, kLogEditMaxChars);
				log->releaseString();
				return 0;
			}

			case WM_APP_taskTrayNotify: {
				if (i_wParam == ID_TaskTrayIcon)
					switch (i_lParam) {
					case WM_RBUTTONUP: {
						POINT p;
						CHECK_TRUE( GetCursorPos(&p) );
						SetForegroundWindow(i_hwnd);
						HMENU hMenuSub = GetSubMenu(This->m_hMenuTaskTray, 0);
						if (This->m_engine.getIsEnabled())
							CheckMenuItem(hMenuSub, ID_MENUITEM_disable,
										  MF_UNCHECKED | MF_BYCOMMAND);
						else
							CheckMenuItem(hMenuSub, ID_MENUITEM_disable,
										  MF_CHECKED | MF_BYCOMMAND);
						CHECK_TRUE( SetMenuDefaultItem(hMenuSub,
													   ID_MENUITEM_investigate, FALSE) );

						// create reload menu
						HMENU hMenuSubSub = GetSubMenu(hMenuSub, 1);
						IniFile ini;
						int mayuIndex;
						ini.read(L".mayuIndex", &mayuIndex, 0);
						while (DeleteMenu(hMenuSubSub, 0, MF_BYPOSITION))
							;
						wregex_stored getName(L"^([^;]*);");
						for (int index = 0; ; index ++) {
							wchar_t buf[100];
							std::swprintf(buf, NUMBER_OF(buf), L".mayu%d", index);
							wstringi dot_mayu;
							if (!ini.read(buf, &dot_mayu))
								break;
							std::wsmatch what;
							if (std::regex_search(dot_mayu, what, getName)) {
								MENUITEMINFO mii;
								std::memset(&mii, 0, sizeof(mii));
								mii.cbSize = sizeof(mii);
								mii.fMask = MIIM_ID | MIIM_STATE | MIIM_TYPE;
								mii.fType = MFT_STRING;
								mii.fState =
									MFS_ENABLED | ((mayuIndex == index) ? MFS_CHECKED : 0);
								mii.wID = ID_MENUITEM_reloadBegin + index;
								wstringi name(what.str(1));
								mii.dwTypeData = const_cast<wchar_t *>(name.c_str());
								mii.cch = static_cast<UINT>(name.size());

								InsertMenuItem(hMenuSubSub, index, TRUE, &mii);
							}
						}

						// show popup menu
						TrackPopupMenu(hMenuSub, TPM_LEFTALIGN,
									   p.x, p.y, 0, i_hwnd, NULL);
						// TrackPopupMenu may fail (ERROR_POPUP_ALREADY_ACTIVE)
						break;
					}

					case WM_LBUTTONDBLCLK:
						SendMessage(i_hwnd, WM_COMMAND,
									MAKELONG(ID_MENUITEM_investigate, 0), 0);
						break;
					}
				return 0;
			}

			case WM_APP_scripterSettingReady: {
				// The Setting travels through ScripterManager's single slot;
				// this notification carries no payload.  An empty slot is
				// normal (a later commit superseded this one, or the slot was
				// already taken), so simply ignore it.
				//
				// Hand the Setting to the engine thread rather than applying
				// it here: see Engine::scheduleSetting().
				if (std::shared_ptr<Setting> newSetting =
							ScripterManager::takePendingSetting())
					This->m_engine.scheduleSetting(std::move(newSetting));
				return 0;
			}

			case WM_COMMAND: {
				int notify_code = HIWORD(i_wParam);
				int id = LOWORD(i_wParam);
				if (notify_code == 0) // menu
					switch (id) {
					default:
						if (ID_MENUITEM_reloadBegin <= id) {
							IniFile ini;
							ini.write(L".mayuIndex", id - ID_MENUITEM_reloadBegin);
							This->load();
						}
						break;
					case ID_MENUITEM_reload:
						This->load();
						break;
					case ID_MENUITEM_investigate: {
						// The log window used to be parked directly below the
						// investigate dialog, taking that dialog's width.  The
						// investigate dialog is DS_CENTERMOUSE, so with the
						// pointer low on the screen the log window ended up
						// hanging off the bottom - and its own size and
						// position, which are now remembered across sessions,
						// were overwritten every time.
						ShowWindow(This->m_hwndLog, SW_SHOW);
						ShowWindow(This->m_hwndInvestigate, SW_SHOW);

						SetForegroundWindow(This->m_hwndLog);
						SetForegroundWindow(This->m_hwndInvestigate);
						break;
					}
					case ID_MENUITEM_setting:
						if (!This->m_isSettingDialogOpened) {
							This->m_isSettingDialogOpened = true;
							if (DialogBox(g_hInst, MAKEINTRESOURCE(IDD_DIALOG_setting),
										  NULL, dlgSetting_dlgProc))
								This->load();
							This->m_isSettingDialogOpened = false;
						}
						break;
					case ID_MENUITEM_log:
						ShowWindow(This->m_hwndLog, SW_SHOW);
						SetForegroundWindow(This->m_hwndLog);
						break;
					case ID_MENUITEM_check: {
						BOOL ret;
						BYTE keys[256];
						ret = GetKeyboardState(keys);
						if (ret == 0) {
							This->m_log << L"Check Keystate Failed(%d)"
							<< GetLastError() << std::endl;
						} else {
							This->m_log << L"Check Keystate: " << std::endl;
							for (int i = 0; i < 0xff; i++) {
								USHORT asyncKey;
								asyncKey = GetAsyncKeyState(i);
								This->m_log << std::hex;
								if (asyncKey & 0x8000) {
									This->m_log << L"  " << VK2WCHAR[i]
									<< L"(0x" << i << L"): pressed!"
									<< std::endl;
								}
								if (i == 0x14 || // VK_CAPTITAL
										i == 0x15 || // VK_KANA
										i == 0x19 || // VK_KANJI
										i == 0x90 || // VK_NUMLOCK
										i == 0x91    // VK_SCROLL
								   ) {
									if (keys[i] & 1) {
										This->m_log << L"  " << VK2WCHAR[i]
										<< L"(0x" << i << L"): locked!"
										<< std::endl;
									}
								}
								This->m_log << std::dec;
							}
							This->m_log << std::endl;
						}
						break;
					}
					case ID_MENUITEM_version:
						ShowWindow(This->m_hwndVersion, SW_SHOW);
						SetForegroundWindow(This->m_hwndVersion);
						break;
					case ID_MENUITEM_help: {
						wchar_t buf[GANA_MAX_PATH];
						CHECK_TRUE( GetModuleFileName(g_hInst, buf, NUMBER_OF(buf)) );
						wstringi helpFilename = pathRemoveFileSpec(buf);
						helpFilename += L"\\";
						helpFilename += loadString(IDS_helpFilename);
						ShellExecute(NULL, L"open", helpFilename.c_str(),
									 NULL, NULL, SW_SHOWNORMAL);
						break;
					}
					case ID_MENUITEM_disable:
						This->m_engine.enable(!This->m_engine.getIsEnabled());
						This->showTasktrayIcon();
						break;
					case ID_MENUITEM_quit:
						This->m_engine.prepairQuit();
						PostQuitMessage(0);
						break;
					}
				return 0;
			}

			case WM_APP_engineNotify: {
				switch (i_wParam) {
				case EngineNotify_shellExecute:
					This->m_engine.shellExecute();
					break;
				case EngineNotify_loadSetting:
					This->load();
					break;
				case EngineNotify_helpMessage:
					This->showHelpMessage(false);
					if (i_lParam)
						This->showHelpMessage(true);
					break;
				case EngineNotify_showDlg: {
					// show investigate/log window
					int sw = (i_lParam & ~MayuDialogType_mask);
					HWND hwnd = NULL;
					switch (static_cast<MayuDialogType>(
								i_lParam & MayuDialogType_mask)) {
					case MayuDialogType_investigate:
						hwnd = This->m_hwndInvestigate;
						break;
					case MayuDialogType_log:
						hwnd = This->m_hwndLog;
						break;
					}
					if (hwnd) {
						ShowWindow(hwnd, sw);
						switch (sw) {
						case SW_SHOWNORMAL:
						case SW_SHOWMAXIMIZED:
						case SW_SHOW:
						case SW_RESTORE:
						case SW_SHOWDEFAULT:
							SetForegroundWindow(hwnd);
							break;
						}
					}
					break;
				}
				case EngineNotify_setForegroundWindow:
					// FIXME: completely useless. why ?
					setForegroundWindow(reinterpret_cast<HWND>(i_lParam));
					{
						Acquire a(&This->m_log, LogLevel::Debug);
						This->m_log << L"setForegroundWindow(0x"
						<< std::hex << i_lParam << std::dec << L")"
						<< std::endl;
					}
					break;
				case EngineNotify_clearLog:
					SendMessage(This->m_hwndLog, WM_COMMAND,
								MAKELONG(IDC_BUTTON_clearLog, 0), 0);
					break;
				default:
					break;
				}
				return 0;
			}

			case WM_APP_dlglogNotify: {
				switch (i_wParam) {
				case DlgLogNotify_logCleared:
					This->showBanner(true);
					break;
				case DlgLogNotify_thresholdChanged:
					This->setLogThreshold(
						logLevelFromByte(static_cast<uint8_t>(i_lParam)));
					break;
				default:
					break;
				}
				return 0;
			}

			case WM_TIMER:
				if (i_wParam == ID_TaskTrayIconRetryTimer) {
					This->onTasktrayIconRetryTimer();
					return 0;
				}
				break;

			case WM_DESTROY:
				if (This->m_usingSN) {
					wtsUnRegisterSessionNotification(i_hwnd);
					This->m_usingSN = false;
				}
				return 0;

			default:
				if (i_message == This->m_WM_TaskbarRestart) {
					if (This->showTasktrayIcon(true)) {
						Acquire a(&This->m_log, LogLevel::Info);
						This->m_log << L"Tasktray icon is updated." << std::endl;
					} else {
						Acquire a(&This->m_log, LogLevel::Debug);
						This->m_log << L"Tasktray icon is not ready yet; retrying."
						<< std::endl;
					}
					return 0;
				} else if (i_message == This->m_WM_MayuIPC) {
					switch (static_cast<MayuIPCCommand>(i_wParam)) {
					case MayuIPCCommand_Enable:
						This->m_engine.enable(!!i_lParam);
						This->showTasktrayIcon();
						if (i_lParam) {
							Acquire a(&This->m_log, LogLevel::Debug);
							This->m_log << L"Enabled by another application."
							<< std::endl;
						} else {
							Acquire a(&This->m_log, LogLevel::Debug);
							This->m_log << L"Disabled by another application."
							<< std::endl;
						}
						break;
					}
				}
			}
		return DefWindowProc(i_hwnd, i_message, i_wParam, i_lParam);
	}

	// Read the active config profile from nyamy.ini.
	// Returns false if no valid entry exists.
	static bool readIniConfig(wstringi *o_name, wstringi *o_path, Symbols *o_symbols)
	{
		IniFile ini;
		int index = 0;
		ini.read(L".mayuIndex", &index, 0);
		wchar_t key[32];
		_snwprintf(key, NUMBER_OF(key), L".mayu%d", index);
		wstringi entry;
		if (!ini.read(key, &entry))
			return false;
		wregex_stored re(L"^([^;]*);([^;]*);(.*)$");
		std::wsmatch m;
		if (!std::regex_match(static_cast<const std::wstring&>(entry), m, re))
			return false;
		if (o_name) *o_name = m.str(1);
		if (o_path) *o_path = m.str(2);
		if (o_symbols) {
			wstringi symPart = m.str(3);
			wregex_stored reSym(L"-D([^;]*)(.*)$");
			std::wsmatch ms;
			while (std::regex_search(static_cast<const std::wstring&>(symPart), ms, reSym)) {
				o_symbols->insert(ms.str(1));
				symPart = ms.str(2);
			}
		}
		return true;
	}

	/// load setting
	void load() {
		// command-line symbols
		Symbols initialSymbols;
		for (int i = 1; i < __argc; ++ i) {
			if (__wargv[i][0] == L'-' && __wargv[i][1] == L'D')
				initialSymbols.insert(__wargv[i] + 2);
		}

		// ini config: name, path, ini symbols merged into initialSymbols
		wstringi configName, configPath;
		{
			Symbols iniSyms;
			if (readIniConfig(&configName, &configPath, &iniSyms)) {
				for (const auto &s : iniSyms)
					initialSymbols.insert(s);
			}
		}

		if (!m_scripter) {
			m_scripter = std::make_unique<ScripterManager>(&m_log, &m_log, m_hwndTaskTray);
			m_scripter->setExecKeySeqCallback([this](AdHocKeySeq item) {
				m_engine.scheduleAdHocKeySeq(std::move(item));
			});
			m_engine.setExecUserFuncCallback(
				[this](const wstringi &name, const std::vector<FuncArg> &args,
				       const TriggerInfo &ctx) {
					if (m_scripter) m_scripter->execUserFunc(name, args, ctx);
				});
		}

		// Start (or restart) scripter asynchronously; result notified via WM_APP_scripterSettingReady.
		// The threshold goes with the Start: the log dialog restores its
		// "detail" state from the ini before the first load, so the scripter
		// can be launched into detail mode from the very first line it writes.
		m_scripter->start(configName, configPath, initialSymbols,
						  m_log.getThreshold());
	}

	/// the log dialog toggled "detail"
	void setLogThreshold(LogLevel i_level) {
		m_log.setThreshold(i_level);
		if (m_scripter)
			m_scripter->setLogLevel(i_level);
	}

	// show message (a baloon from the task tray icon)
	void showHelpMessage(bool i_doesShow = true) {
		if (m_canUseTasktrayBaloon) {
			if (i_doesShow) {
				std::wstring helpMessage, helpTitle;
				m_engine.getHelpMessages(&helpMessage, &helpTitle);
				wcslcpy(m_ni.szInfo, helpMessage.c_str(), NUMBER_OF(m_ni.szInfo));
				wcslcpy(m_ni.szInfoTitle, helpTitle.c_str(),
						NUMBER_OF(m_ni.szInfoTitle));
				m_ni.dwInfoFlags = NIIF_INFO;
			} else
				m_ni.szInfo[0] = m_ni.szInfoTitle[0] = L'\0';
			CHECK_TRUE( Shell_NotifyIcon(NIM_MODIFY, &m_ni) );
		}
	}

	// one attempt at putting the icon into the task tray.
	// http://support.microsoft.com/kb/418138/JA/ is why NIM_MODIFY is worth a
	// try of its own when NIM_ADD fails
	bool tryAddTasktrayIcon() {
		if (Shell_NotifyIcon(NIM_ADD, &m_ni))
			return true;
		return !!Shell_NotifyIcon(NIM_MODIFY, &m_ni);
	}

	// change the task tray icon
	bool showTasktrayIcon(bool i_doesAdd = false) {
		m_ni.hIcon  = m_tasktrayIcon[m_engine.getIsEnabled() ? 1 : 0];
		m_ni.szInfo[0] = m_ni.szInfoTitle[0] = L'\0';
		if (i_doesAdd) {
			// The shell refuses the icon until its tray is up, which at logon is
			// still seconds away, so the attempt has to be repeated.  Retry from
			// a timer rather than sleeping here: the shell keeps its
			// "app starting" cursor up until this thread reaches its message
			// loop, so sleeping through the retry budget put a busy cursor on
			// screen for as long as Windows allows one (~5 seconds).
			m_tasktrayIconRetries = 60;
			if (tryAddTasktrayIcon()) {
				KillTimer(m_hwndTaskTray, ID_TaskTrayIconRetryTimer);
				return true;
			}
			SetTimer(m_hwndTaskTray, ID_TaskTrayIconRetryTimer, 1000, NULL);
			return false;
		} else {
			return !!Shell_NotifyIcon(NIM_MODIFY, &m_ni);
		}
	}

	/// ID_TaskTrayIconRetryTimer expired
	void onTasktrayIconRetryTimer() {
		if (tryAddTasktrayIcon()) {
			KillTimer(m_hwndTaskTray, ID_TaskTrayIconRetryTimer);
			Acquire a(&m_log, LogLevel::Debug);
			m_log << L"Tasktray icon is added." << std::endl;
			return;
		}
		if (-- m_tasktrayIconRetries <= 0) {
			KillTimer(m_hwndTaskTray, ID_TaskTrayIconRetryTimer);
			Acquire a(&m_log, LogLevel::Warn);
			m_log << L"gave up adding the tasktray icon." << std::endl;
		}
	}

	void showBanner(bool i_isCleared) {
		// Every log line already carries hh:mm:ss.SSS, and a bare [YYYY-MM-DD]
		// line is emitted whenever the date changes, so the banner only needs
		// the wall clock time - and no ruled lines to separate it.
		wchar_t starttimebuf[64];
		wcsftime(starttimebuf, NUMBER_OF(starttimebuf), L"%H:%M:%S",
				 localtime(&m_startTime));

		Acquire a(&m_log, LogLevel::Info);
		m_log << loadString(IDS_mayu) << L" " WIDEN(VERSION);
#ifndef NDEBUG
		m_log << L" (DEBUG)";
#endif
		m_log << L" (UNICODE)";
		m_log << std::endl;
		m_log << L"  built by "
		<< WIDEN(LOGNAME) << L"@" << toLower(WIDEN(COMPUTERNAME))
		<< L" (" << WIDEN(__DATE__) <<  L" "
		<< WIDEN(__TIME__) << L", "
		<< getCompilerVersionString() << L")" << std::endl;
		wchar_t modulebuf[1024];
		CHECK_TRUE( GetModuleFileName(g_hInst, modulebuf,
									  NUMBER_OF(modulebuf)) );
		m_log << L"  started at " << starttimebuf << std::endl;
		m_log << L"  " << modulebuf << std::endl;
		m_log << (i_isCleared ? L"log was cleared." : L"log begins.")
		<< std::endl;
	}

	int errorDialogWithCode(UINT ids, int code, UINT style = MB_OK | MB_ICONSTOP)
	{
		wchar_t title[1024];
		wchar_t text[1024];

		std::swprintf(title, NUMBER_OF(title), loadString(IDS_mayu).c_str());
		std::swprintf(text, NUMBER_OF(text), loadString(ids).c_str(), code);
 		return MessageBox((HWND)NULL, text, title, style);
	}

	int enableToWriteByUser(HANDLE hdl)
	{
		WCHAR userName[GANA_MAX_ATOM_LENGTH];
		DWORD userNameSize = NUMBER_OF(userName);

		SID_NAME_USE sidType;
		PSID pSid = NULL;
		DWORD sidSize = 0;
		WCHAR *pDomain = NULL;
		DWORD domainSize = 0;

		PSECURITY_DESCRIPTOR pSd = nullptr;
		PACL pOrigDacl;
		ACL_SIZE_INFORMATION aclInfo = {};

		PACL pNewDacl = nullptr;
		DWORD newDaclSize;

		DWORD aceIndex;
		DWORD newAceIndex = 0;

		BOOL ret;
		int err = 0;

		ret = GetUserName(userName, &userNameSize);
		if (ret == FALSE) {
			err = YAMY_ERROR_ON_GET_USERNAME;
			goto exit;
		}

		// get buffer size for pSid (and pDomain)
		ret = LookupAccountName(NULL, userName, pSid, &sidSize, pDomain, &domainSize, &sidType);
		if (ret != FALSE || GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
			// above call should fail by ERROR_INSUFFICIENT_BUFFER
			err = YAMY_ERROR_ON_GET_LOGONUSERNAME;
			goto exit;
		}

		pSid = reinterpret_cast<PSID>(LocalAlloc(LPTR, sidSize));
		pDomain = reinterpret_cast<WCHAR*>(LocalAlloc(LPTR, domainSize * sizeof(WCHAR)));
		if (pSid == NULL || pDomain == NULL) {
			err = YAMY_ERROR_NO_MEMORY;
			goto exit;
		}

		// get SID (and Domain) for logoned user
		ret = LookupAccountName(NULL, userName, pSid, &sidSize, pDomain, &domainSize, &sidType);
		if (ret == FALSE) {
			// LookupAccountName() should success in this time
			err = YAMY_ERROR_ON_GET_LOGONUSERNAME;
			goto exit;
		}

		// get DACL for hdl
		ret = GetSecurityInfo(hdl, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, NULL, NULL, &pOrigDacl, NULL, &pSd);
		if (ret != ERROR_SUCCESS) {
			err = YAMY_ERROR_ON_GET_SECURITYINFO;
			goto exit;
		}

		// get size for original DACL
		ret = GetAclInformation(pOrigDacl, &aclInfo, sizeof(aclInfo), AclSizeInformation);
		if (ret == FALSE) {
			err = YAMY_ERROR_ON_GET_DACL;
			goto exit;
		}

		// compute size for new DACL
		newDaclSize = aclInfo.AclBytesInUse + sizeof(ACCESS_ALLOWED_ACE) + GetLengthSid(pSid) - sizeof(DWORD);

		// allocate memory for new DACL
		pNewDacl = reinterpret_cast<PACL>(LocalAlloc(LPTR, newDaclSize));
		if (pNewDacl == NULL) {
			err = YAMY_ERROR_NO_MEMORY;
			goto exit;
		}

		// initialize new DACL
		ret = InitializeAcl(pNewDacl, newDaclSize, ACL_REVISION);
		if (ret == FALSE) {
			err = YAMY_ERROR_ON_INITIALIZE_ACL;
			goto exit;
		}

		// copy original DACL to new DACL
		for (aceIndex = 0; aceIndex < aclInfo.AceCount; aceIndex++) {
			LPVOID pAce;

			ret = GetAce(pOrigDacl, aceIndex, &pAce);
			if (ret == FALSE) {
				err = YAMY_ERROR_ON_GET_ACE;
				goto exit;
			}

			if ((reinterpret_cast<ACCESS_ALLOWED_ACE*>(pAce))->Header.AceFlags & INHERITED_ACE) {
				break;
			}

			if (EqualSid(pSid, &(reinterpret_cast<ACCESS_ALLOWED_ACE*>(pAce))->SidStart) != FALSE) {
				continue;
			}

			ret = AddAce(pNewDacl, ACL_REVISION, MAXDWORD, pAce, (reinterpret_cast<PACE_HEADER>(pAce))->AceSize);
			if (ret == FALSE) {
				err = YAMY_ERROR_ON_ADD_ACE;
				goto exit;
			}

			newAceIndex++;
		}

		ret = AddAccessAllowedAce(pNewDacl, ACL_REVISION, GENERIC_ALL, pSid);
		if (ret == FALSE) {
			err = YAMY_ERROR_ON_ADD_ALLOWED_ACE;
			goto exit;
		}

		// copy the rest of inherited ACEs
		for (; aceIndex < aclInfo.AceCount; aceIndex++) {
			LPVOID pAce;

			ret = GetAce(pOrigDacl, aceIndex, &pAce);
			if (ret == FALSE) {
				err = YAMY_ERROR_ON_GET_ACE;
				goto exit;
			}

			ret = AddAce(pNewDacl, ACL_REVISION, MAXDWORD, pAce, (reinterpret_cast<PACE_HEADER>(pAce))->AceSize);
			if (ret == FALSE) {
				err = YAMY_ERROR_ON_ADD_ACE;
				goto exit;
			}
		}

		ret = SetSecurityInfo(hdl, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, NULL, NULL, pNewDacl, NULL);
		if (ret != ERROR_SUCCESS) {
			err = YAMY_ERROR_ON_SET_SECURITYINFO;
		}

exit:
		LocalFree(pSd);
		LocalFree(pSid);
		LocalFree(pDomain);
		LocalFree(pNewDacl);

		return err;
	}

public:
	///
	Mayu(HANDLE i_mutex)
			: m_hwndTaskTray(NULL),
			m_mutex(i_mutex),
			m_hwndLog(NULL),
			m_WM_TaskbarRestart(RegisterWindowMessage(L"TaskbarCreated")),
			m_WM_MayuIPC(RegisterWindowMessage(WM_MayuIPC_NAME)),
			m_tasktrayIconRetries(0),
			m_canUseTasktrayBaloon(
				PACKVERSION(5, 0) <= getDllVersion(L"shlwapi.dll")),
			m_log(WM_APP_msgStreamNotify),
			m_isSettingDialogOpened(false),
			m_sessionState(0),
			m_engine(m_log) {
		// addSessionId(): mailslot names live in \Device\Mailslot, which has
		// no per session split of its own, so without it a second logged on
		// user's nyamy would find the name taken.  Everything else shared
		// with the hooks is named this way already.
		m_hNotifyMailslot = CreateMailslot(addSessionId(NOTIFY_MAILSLOT_NAME).c_str(), 0, MAILSLOT_WAIT_FOREVER, (SECURITY_ATTRIBUTES *)NULL);
		ASSERT(m_hNotifyMailslot != INVALID_HANDLE_VALUE);
		int err;
		if (checkWindowsVersion(6, 0) != FALSE) { // enableToWriteByUser() is available only Vista or later
			err = enableToWriteByUser(m_hNotifyMailslot);
			if (err) {
				errorDialogWithCode(IDS_cannotPermitStandardUser, err);
			}
		}

		m_hNotifyThread = NULL;
		m_notifyReaderStop = false;
		time(&m_startTime);

		CHECK_TRUE( Register_focus() );
		CHECK_TRUE( Register_target() );
		CHECK_TRUE( Register_tasktray() );

		// create windows, dialogs
		wstringi title = loadString(IDS_mayu);
		m_hwndTaskTray = CreateWindow(L"mayuTasktray", title.c_str(),
									  WS_OVERLAPPEDWINDOW,
									  CW_USEDEFAULT, CW_USEDEFAULT,
									  CW_USEDEFAULT, CW_USEDEFAULT,
									  NULL, NULL, g_hInst, this);
		CHECK_TRUE( m_hwndTaskTray );

		// set window handle of tasktray to hooks
		CHECK_FALSE( installMessageHook(m_hwndTaskTray) );
		m_usingSN = wtsRegisterSessionNotification(m_hwndTaskTray,
					NOTIFY_FOR_THIS_SESSION);

		DlgLogData dld = {
			.m_log = &m_log,
			.m_hwndTaskTray = m_hwndTaskTray,
		};
		m_hwndLog =
			CreateDialogParam(g_hInst, MAKEINTRESOURCE(IDD_DIALOG_log), NULL,
							  dlgLog_dlgProc, (LPARAM)&dld);
		CHECK_TRUE( m_hwndLog );

		DlgInvestigateData did = {
			.m_engine = &m_engine,
			.m_hwndLog = m_hwndLog,
		};
		m_hwndInvestigate =
			CreateDialogParam(g_hInst, MAKEINTRESOURCE(IDD_DIALOG_investigate), NULL,
							  dlgInvestigate_dlgProc, (LPARAM)&did);
		CHECK_TRUE( m_hwndInvestigate );

		m_hwndVersion =
			CreateDialogParam(g_hInst, MAKEINTRESOURCE(IDD_DIALOG_version),
							  NULL, dlgVersion_dlgProc,
							  (LPARAM)L"");
		CHECK_TRUE( m_hwndVersion );

		// attach log
#ifdef LOG_TO_FILE
		std::wstring path;
		wchar_t exePath[GANA_MAX_PATH];
		wchar_t exeDrive[GANA_MAX_PATH];
		wchar_t exeDir[GANA_MAX_PATH];
		GetModuleFileName(NULL, exePath, GANA_MAX_PATH);
		_wsplitpath_s(exePath, exeDrive, GANA_MAX_PATH, exeDir, GANA_MAX_PATH, NULL, 0, NULL, 0);
		path = exeDrive;
		path += exeDir;
		path += L"mayu.log";
		m_logFile.open(path.c_str(), std::ios::app);
		m_logFile.imbue(std::locale("japanese"));
#endif // LOG_TO_FILE
		SendMessage(GetDlgItem(m_hwndLog, IDC_EDIT_log), EM_SETLIMITTEXT, 0, 0);
		m_log.attach(m_hwndTaskTray);

		// start keyboard handler thread
		m_engine.setAssociatedWndow(m_hwndTaskTray);
		m_engine.start();

		// show tasktray icon
		m_tasktrayIcon[0] = loadSmallIcon(IDI_ICON_mayu_disabled);
		m_tasktrayIcon[1] = loadSmallIcon(IDI_ICON_mayu);
		std::memset(&m_ni, 0, sizeof(m_ni));
		m_ni.uID    = ID_TaskTrayIcon;
		m_ni.hWnd   = m_hwndTaskTray;
		m_ni.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
		m_ni.hIcon  = m_tasktrayIcon[1];
		m_ni.uCallbackMessage = WM_APP_taskTrayNotify;
		std::wstring tip = loadString(IDS_mayu);
		wcslcpy(m_ni.szTip, tip.c_str(), NUMBER_OF(m_ni.szTip));
		if (m_canUseTasktrayBaloon) {
			m_ni.cbSize = NOTIFYICONDATA_V3_SIZE;
			m_ni.uFlags |= NIF_INFO;
		} else
			m_ni.cbSize = NOTIFYICONDATA_V1_SIZE;
		showTasktrayIcon(true);

		// create menu
		m_hMenuTaskTray = LoadMenu(g_hInst, MAKEINTRESOURCE(IDR_MENU_tasktray));
		ASSERT(m_hMenuTaskTray);

		// set initial lock state
		notifyLockState();

		ZeroMemory(&m_pi,sizeof(m_pi));
		ZeroMemory(&m_si,sizeof(m_si));
		m_si.cb=sizeof(m_si);
		// A process started by the shell carries startup feedback - the
		// "app starting" cursor - and a child created while that feedback is
		// still up inherits it.  nyamyd waits on a mutex and never pumps
		// messages, so the feedback would never be cleared and the busy cursor
		// stayed on screen for as long as Windows allows one (~5 seconds) on
		// every launch.  Measured: 5.2s without this flag, gone with it.
		m_si.dwFlags = STARTF_FORCEOFFFEEDBACK;

		// create mutex to block yamyd
		m_hMutexYamyd = CreateMutex((SECURITY_ATTRIBUTES *)NULL, TRUE, MUTEX_YAMYD_BLOCKER);

		std::wstring yamydPath;
		wchar_t exePath[GANA_MAX_PATH];
		wchar_t exeDrive[GANA_MAX_PATH];
		wchar_t exeDir[GANA_MAX_PATH];

		GetModuleFileName(NULL, exePath, GANA_MAX_PATH);
		_wsplitpath_s(exePath, exeDrive, GANA_MAX_PATH, exeDir, GANA_MAX_PATH, NULL, 0, NULL, 0);
		yamydPath = exeDrive;
		yamydPath += exeDir;
		yamydPath += L"nyamyd32";

		BOOL result = CreateProcess(yamydPath.c_str(), NULL, NULL, NULL, FALSE,
							   NORMAL_PRIORITY_CLASS, 0, NULL, &m_si, &m_pi);
		if (result == FALSE) {
			WCHAR buf[1024];
			WCHAR text[1024];
			WCHAR title2[1024];

			m_pi.hProcess = NULL;
			LoadString(GetModuleHandle(NULL), IDS_cannotInvoke,
					   text, sizeof(text)/sizeof(text[0]));
			LoadString(GetModuleHandle(NULL), IDS_mayu,
					   title2, sizeof(title2)/sizeof(title2[0]));
			std::swprintf(buf, NUMBER_OF(buf),
						text, L"nyamyd32", GetLastError());
	 		MessageBox((HWND)NULL, buf, title2, MB_OK | MB_ICONSTOP);
		} else {
			CloseHandle(m_pi.hThread);
		}

	}

	///
	~Mayu() {
		// stop notify from mayu.dll
		g_hookData->setHwndTaskTray(NULL);
		CHECK_FALSE( uninstallMessageHook() );
		// The hooks are gone, so nothing new will be written to the mailslot.
		// Stop reading it before anything is torn down, so no notification can
		// reach the engine while it is being shut down.
		stopNotifyReader();

		// --- Phase A: signal all shutdowns simultaneously ---
		ReleaseMutex(m_hMutexYamyd);       // yamyd exits when it loses the mutex
		if (m_scripter) {
			// launchScripter() runs on an async task and rewrites the pipe and
			// thread handles; let it finish before we read or close them.
			m_scripter->waitForPendingStart();
			m_scripter->sendQuit();            // scripter: send quit + close ctrl pipe
			m_scripter->stopReaders();         // reader threads: end their reads
		}

		// --- Phase B+C: stop InputHandlers in parallel, then signal engine ---
		// signalStop() waits for both InputHandlers (up to 3 s, in parallel)
		// and signals the engine thread to exit.  The input queue outlives it
		// and is released by cleanupAfterStop() below.
		HANDLE hEngineThread = m_engine.signalStop();

		// --- Phase D: wait for all remaining threads/processes in parallel ---
		HANDLE handles[6] = {};
		DWORD n = 0;
		if (m_pi.hProcess) handles[n++] = m_pi.hProcess;
		if (m_scripter)
			n += m_scripter->collectHandles(handles + n, 6 - n);
		handles[n++] = hEngineThread;
		if (n > 0)
			WaitForMultipleObjects(n, handles, TRUE, kScripterQuitGraceMillisec);

		// --- cleanup handles ---
		if (m_pi.hProcess) { CloseHandle(m_pi.hProcess); m_pi.hProcess = NULL; }
		CloseHandle(m_hMutexYamyd);

		// The reader threads keep using this object's log stream, tasktray
		// window and callbacks, and the engine's input queue, so nothing below
		// may run while they are alive.  Phase D already waited
		// kScripterQuitGraceMillisec, hence the 0 grace here.
		if (m_scripter) {
			m_scripter->forceStop(0);
			m_scripter->closeHandles();
			m_scripter.reset();
		}
		m_engine.cleanupAfterStop(hEngineThread);

		// closing it while the reader thread is still blocked on it would pull
		// the handle out from under that thread; stopNotifyReader() leaves
		// m_hNotifyThread set when it could not confirm the thread had gone
		if (!m_hNotifyThread)
			CloseHandle(m_hNotifyMailslot);
		ReleaseMutex(m_mutex);
		WaitForSingleObject(m_mutex, INFINITE);
		// first, detach log from edit control to avoid deadlock
		m_log.detach();
#ifdef LOG_TO_FILE
		m_logFile.close();
#endif // LOG_TO_FILE

		// destroy windows
		CHECK_TRUE( DestroyWindow(m_hwndVersion) );
		CHECK_TRUE( DestroyWindow(m_hwndInvestigate) );
		CHECK_TRUE( DestroyWindow(m_hwndLog) );
		CHECK_TRUE( DestroyWindow(m_hwndTaskTray) );

		// destroy menu
		DestroyMenu(m_hMenuTaskTray);

		// delete tasktray icon
		CHECK_TRUE( Shell_NotifyIcon(NIM_DELETE, &m_ni) );
		CHECK_TRUE( DestroyIcon(m_tasktrayIcon[1]) );
		CHECK_TRUE( DestroyIcon(m_tasktrayIcon[0]) );
	}

	/// message loop
	WPARAM messageLoop() {
		showBanner(false);
		load();

		startNotifyReader();
		while (1) {
			// notifications are read on their own thread; this one only has
			// messages to wait for
			switch (DWORD ret = MsgWaitForMultipleObjectsEx(0, NULL,
						  INFINITE, QS_ALLINPUT, MWMO_ALERTABLE | MWMO_INPUTAVAILABLE)) {
			case WAIT_OBJECT_0: {
				MSG msg;
				if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) != 0) {
					if (msg.message == WM_QUIT) {
						return msg.wParam;
					}
					if (IsDialogMessage(m_hwndLog, &msg))
						break;
					if (IsDialogMessage(m_hwndInvestigate, &msg))
						break;
					if (IsDialogMessage(m_hwndVersion, &msg))
						break;
					TranslateMessage(&msg);
					DispatchMessage(&msg);
					break;
				}
				break;
			}

			case WAIT_IO_COMPLETION:
				break;

			case 0x102:
			default:
				break;
			}
		}
	}
};


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Functions


/// main
static LPTOP_LEVEL_EXCEPTION_FILTER s_prevExceptionFilter = NULL;

static LONG WINAPI crashExceptionFilter(EXCEPTION_POINTERS *i_ep)
{
	emergencyUnhookAll();
	if (s_prevExceptionFilter)
		return s_prevExceptionFilter(i_ep);
	return EXCEPTION_CONTINUE_SEARCH;
}


int WINAPI wWinMain(_In_ HINSTANCE i_hInstance, _In_opt_ HINSTANCE /* i_hPrevInstance */,
	_In_ PWSTR /* i_lpszCmdLine */, _In_ int /* i_nCmdShow */)
{
	g_hInst = i_hInstance;

	// crash-safe hook cleanup
	s_prevExceptionFilter = SetUnhandledExceptionFilter(crashExceptionFilter);

	// resolve NYAMY_ROOT / NYAMY_HOME / NYAMY_CONFIG and publish them to the
	// environment before anything reads nyamy.ini or starts the scripter
	NYamyPaths::init();

	// set locale
	CHECK_TRUE(_wsetlocale(LC_ALL, L""));

	// common controls
	INITCOMMONCONTROLSEX icc = {
		.dwSize = sizeof(icc),
		.dwICC = ICC_LISTVIEW_CLASSES,
	};
	CHECK_TRUE( InitCommonControlsEx(&icc) );

	// is another mayu running ?
	// CreateMutex() hands back a valid handle for a mutex that already
	// exists, so an existing instance shows in GetLastError(), never in the
	// handle - testing the handle alone let every further instance start.
	// ERROR_ACCESS_DENIED means the mutex is there but out of our reach,
	// which in practice is an elevated instance seen from a normal one.
	HANDLE mutex = CreateMutex((SECURITY_ATTRIBUTES *)NULL, TRUE,
							   MUTEX_MAYU_EXCLUSIVE_RUNNING);
	DWORD mutexError = GetLastError();	// before anything overwrites it
	if (mutex == nullptr || mutexError == ERROR_ALREADY_EXISTS) {
		std::wstring title = loadString(IDS_mayu);
		std::wstring text;
		if (mutexError == ERROR_ALREADY_EXISTS ||
				mutexError == ERROR_ACCESS_DENIED) {
			// another mayu already running
			text = loadString(IDS_mayuAlreadyExists);
		}
		else {
			// failed to create mutex for unknown reason
			wchar_t buf[1024]{ L"" };
			std::swprintf(buf, NUMBER_OF(buf), loadString(IDS_unexpectedError).c_str(), mutexError);
			text = buf;
		}
		if (g_hookData) {
			UINT WM_TaskbarRestart = RegisterWindowMessage(L"TaskbarCreated");
			PostMessage(g_hookData->getHwndTaskTray(),
						WM_TaskbarRestart, 0, 0);
		}
		MessageBox((HWND)NULL, text.c_str(), title.c_str(), MB_OK | MB_ICONSTOP);
		// we do not own it: closing only drops our reference, the running
		// instance keeps the mutex alive
		if (mutex)
			CloseHandle(mutex);
		return 1;
	}

	try {
		Mayu(mutex).messageLoop();
	} catch (ErrorMessage &i_e) {
		std::wstring title = loadString(IDS_mayu);
		MessageBox((HWND)NULL, i_e.getMessage().c_str(), title.c_str(),
				   MB_OK | MB_ICONSTOP);
	}

	CHECK_TRUE( CloseHandle(mutex) );
	return 0;
}
