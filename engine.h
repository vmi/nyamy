//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// engine.h


#ifndef _ENGINE_H
#  define _ENGINE_H

#  include "multithread.h"
#  include "setting.h"
#  include "adhoc_keyseq.h"
#  include "msgstream.h"
#  include "hook.h"
#  include <atomic>
#  include <deque>
#  include <functional>
#  include <memory>
#  include <set>
#  include <queue>
#  include <mutex>
#  include <variant>


enum {
	///
	WM_APP_engineNotify = WM_APP + 110,
};

/// An item in the Engine input queue: a raw keyboard event, an ad-hoc key
/// sequence, or a Setting to activate
using InputEvent = std::variant<KEYBOARD_INPUT_DATA, AdHocKeySeq,
                                std::shared_ptr<Setting> >;


///
enum EngineNotify {
	EngineNotify_shellExecute,			///
	EngineNotify_loadSetting,			///
	EngineNotify_showDlg,				///
	EngineNotify_helpMessage,			///
	EngineNotify_setForegroundWindow,		///
	EngineNotify_clearLog,			///
};


///
class Engine
{
private:
	enum {
		MAX_GENERATE_KEYBOARD_EVENTS_RECURSION_COUNT = 64, ///
		MAX_KEYMAP_PREFIX_HISTORY = 64, ///
	};

	using KeymapPtrList = Keymaps::KeymapPtrList;	///

	/// focus of a thread
	class FocusOfThread
	{
	public:
		DWORD m_threadId;				/// thread id
		HWND m_hwndFocus;				/** window that has focus on
                                                    the thread */
		wstringi m_className;			/// class name of hwndFocus
		wstringi m_titleName;			/// title name of hwndFocus
		bool m_isConsole;				/// is hwndFocus console ?
		KeymapPtrList m_keymaps;			/// keymaps

	public:
		///
		FocusOfThread() : m_threadId(0), m_hwndFocus(NULL), m_isConsole(false) { }
	};
	using FocusOfThreads = std::map<DWORD /*ThreadId*/, FocusOfThread>;	///

	using ThreadIds = std::list<DWORD /*ThreadId*/>;	///

	/// current status in generateKeyboardEvents
	class Current
	{
	public:
		const Keymap *m_keymap;			/// current keymap
		ModifiedKey m_mkey;		/// current processing key that user inputed
		/// index in currentFocusOfThread-&gt;keymaps
		Keymaps::KeymapPtrList::iterator m_i;
		/// non-null for ExecKeySeq: bypasses keymap lookup in generateKeyboardEvents
		const KeySeq *m_adhocKeySeq = nullptr;

	public:
		///
		bool isPressed() const {
			return m_mkey.m_modifier.isOn(Modifier::Type_Down);
		}
	};

	friend class FunctionParam;

	/// part of keySeq
	enum Part {
		Part_all,					///
		Part_up,					///
		Part_down,					///
	};

	///
	class EmacsEditKillLine
	{
		std::wstring m_buf;	/// previous kill-line contents

	public:
		bool m_doForceReset;	///

	private:
		///
		HGLOBAL makeNewKillLineBuf(const wchar_t *i_data, int *i_retval);

	public:
		///
		void reset() {
			m_buf.resize(0);
		}
		/** EmacsEditKillLineFunc.
		clear the contents of the clopboard
		at that time, confirm if it is the result of the previous kill-line
		*/
		void func();
		/// EmacsEditKillLinePred
		int pred();
	};

	/// window positon for &amp;WindowHMaximize, &amp;WindowVMaximize
	class WindowPosition
	{
	public:
		///
		enum Mode {
			Mode_normal,				///
			Mode_H,					///
			Mode_V,					///
			Mode_HV,					///
		};

	public:
		HWND m_hwnd;				///
		RECT m_rc;					///
		Mode m_mode;				///

	public:
		///
		WindowPosition(HWND i_hwnd, const RECT &i_rc, Mode i_mode)
				: m_hwnd(i_hwnd), m_rc(i_rc), m_mode(i_mode) { }
	};
	using WindowPositions = std::list<WindowPosition>;

	using WindowsWithAlpha = std::list<HWND>; /// windows for &amp;WindowSetAlpha

	enum InterruptThreadReason {
		InterruptThreadReason_Terminate,
		InterruptThreadReason_Pause,
		InterruptThreadReason_Resume,
	};

	///
	class InputHandler {
	public:
		using INSTALL_HOOK = int (*)(INPUT_DETOUR i_keyboardDetour, Engine *i_engine, bool i_install);

		static unsigned int WINAPI run(void *i_this);

		InputHandler(INSTALL_HOOK i_installHook, INPUT_DETOUR i_inputDetour);

		~InputHandler();

		void run();

		int start(Engine *i_engine);

		int stop();

		/// post WM_QUIT to the thread (non-blocking)
		void postQuit();
		/// return the thread handle
		HANDLE hThread() const { return m_hThread; }
		/// close and null the thread handle
		void closeThread();

	private:
		unsigned m_threadId;
		HANDLE m_hThread;
		HANDLE m_hEvent;
		INSTALL_HOOK m_installHook;
		INPUT_DETOUR m_inputDetour;
		Engine *m_engine;
	};

private:
	std::recursive_mutex m_mutex;			/// mutex

	// setting
	HWND m_hwndAssocWindow;			/** associated window (we post
                                                    message to it) */
	std::atomic<std::shared_ptr<Setting>> m_setting;  ///< current setting

	// engine thread state
	HANDLE m_threadHandle;
	unsigned m_threadId;
	std::unique_ptr<std::deque<InputEvent>> m_inputQueue;
	HANDLE m_queueMutex;
	MSLLHOOKSTRUCT m_msllHookCurrent;
	bool m_buttonPressed;
	bool m_dragging;
	InputHandler m_keyboardHandler;
	InputHandler m_mouseHandler;
	HANDLE m_readEvent;				/** reading from mayu device
                                                    has been completed */
	OVERLAPPED m_ol;				/** for async read/write of
						    mayu device */
	HANDLE m_hookPipe;				/// named pipe for &SetImeString
	bool volatile m_isLogMode;			/// is logging mode ?
	bool volatile m_isEnabled;			/// is enabled  ?
	bool volatile m_isSynchronizing;		/// is synchronizing ?
	HANDLE m_eSync;				/// event for synchronization
	int m_generateKeyboardEventsRecursionGuard;	/** guard against too many
                                                    recursion */

	// current key state
	Modifier m_currentLock;			/// current lock key's state
	int m_currentKeyPressCount;			/** how many keys are pressed
                                                    phisically ? */
	int m_currentKeyPressCountOnWin32;		/** how many keys are pressed
                                                    on win32 ? */
	std::atomic<bool> m_resyncForceRequested;	/** force resyncKeyStates
                                                    on next handler wakeup */
	Key *m_lastGeneratedKey;			/// last generated key
	Key *m_lastPressedKey[2];			/// last pressed key
	ModifiedKey m_oneShotKey;			/// one shot key
	unsigned int m_oneShotRepeatableRepeatCount; /// repeat count of one shot key
	bool m_isPrefix;				/// is prefix ?
	bool m_doesIgnoreModifierForPrefix;		/** does ignore modifier key
                                                    when prefixed ? */
	bool m_doesEditNextModifier;			/** does edit next user input
                                                    key's modifier ? */
	Modifier m_modifierForNextKey;		/** modifier for next key if
                                                    above is true */

	/** current keymaps.
	    <dl>
	    <dt>when &amp;OtherWindowClass
	    <dd>currentKeymap becoms currentKeymaps[++ Current::i]
	    <dt>when &amp;KeymapParent
	    <dd>currentKeymap becoms currentKeyamp-&gt;parentKeymap
	    <dt>other
	    <dd>currentKeyamp becoms *Current::i
	    </dl>
	*/
	const Keymap * volatile m_currentKeymap;	/// current keymap
	FocusOfThreads /*volatile*/ m_focusOfThreads;	///
	FocusOfThread * volatile m_currentFocusOfThread; ///
	FocusOfThread m_globalFocus;			///
	HWND m_hwndFocus;				/// current focus window
	ThreadIds m_attachedThreadIds;	///
	ThreadIds m_detachedThreadIds;	///

	// for functions
	KeymapPtrList m_keymapPrefixHistory;		/// for &amp;KeymapPrevPrefix
	EmacsEditKillLine m_emacsEditKillLine;	/// for &amp;EmacsEditKillLine
	const ActionFunction *m_afShellExecute;	/// for &amp;ShellExecute

	WindowPositions m_windowPositions;		///
	WindowsWithAlpha m_windowsWithAlpha;		///

	std::wstring m_helpMessage;			/// for &amp;HelpMessage
	std::wstring m_helpTitle;				/// for &amp;HelpMessage
	int m_variable;				/// for &amp;Variable,
	///  &amp;Repeat

public:
	womsgstream &m_log;				/** log stream (output to log
                                                    dialog's edit) */

public:
	/// keyboard handler thread
	static unsigned int WINAPI keyboardDetour(Engine *i_this, WPARAM i_wParam, LPARAM i_lParam);
	/// mouse handler thread
	static unsigned int WINAPI mouseDetour(Engine *i_this, WPARAM i_wParam, LPARAM i_lParam);
private:
	///
	unsigned int keyboardDetour(KBDLLHOOKSTRUCT *i_kid);
	///
	unsigned int mouseDetour(WPARAM i_message, MSLLHOOKSTRUCT *i_mid);
	///
	unsigned int injectInput(const KEYBOARD_INPUT_DATA *i_kid, const KBDLLHOOKSTRUCT *i_kidRaw);

private:
	/// keyboard handler thread
	static unsigned int WINAPI keyboardHandler(void *i_this);
	///
	void keyboardHandler();

	/// activate a Setting.  Engine thread only, called at an event boundary.
	void applySetting(std::shared_ptr<Setting> i_setting);

	/// check focus window
	void checkFocusWindow();
	/// is modifier pressed ?
	bool isPressed(Modifier::Type i_mt);
	/// fix modifier key
	bool fixModifierKey(ModifiedKey *io_mkey, Keymap::AssignMode *o_am);

	/// output to log
	void outputToLog(const Key *i_key, const ModifiedKey &i_mkey,
					 int i_debugLevel);

	/// genete modifier events
	void generateModifierEvents(const Modifier &i_mod);

	/// genete event
	void generateEvents(Current i_c, const Keymap *i_keymap, Key *i_event);

	/// generate keyboard event
	void generateKeyEvent(Key *i_key, bool i_doPress, bool i_isByAssign);
	///
	void generateActionEvents(const Current &i_c, const Action *i_a,
							  bool i_doPress);
	///
	void generateKeySeqEvents(const Current &i_c, const KeySeq *i_keySeq,
							  Part i_part);
	///
	void generateKeyboardEvents(const Current &i_c);
	///
	void beginGeneratingKeyboardEvents(const Current &i_c, bool i_isModifier);

	/// pop all pressed key on win32
	void keyboardResetOnWin32();

	/// release modifiers and reset counters when no key is pressed
	void resetModifiersIfIdle();

	/// drop pressed-key marks that no longer match the OS key state
	void resyncKeyStates(bool i_force);

	/// get current modifiers
	Modifier getCurrentModifiers(Key *i_key, bool i_isPressed);

	/// describe bindings
	void describeBindings();

	/// update m_lastPressedKey
	void updateLastPressedKey(Key *i_key);

	/// set current keymap
	void setCurrentKeymap(const Keymap *i_keymap,
						  bool i_doesAddToHistory = false);

	/// ExecUserFunc callback type (called from engine thread when a user function is invoked)
	using ExecUserFuncCallback = std::function<void(const wstringi &,
	                                                const std::vector<FuncArg> &,
	                                                const TriggerInfo &)>;

	/// reconstruct a Current from a TriggerInfo
	Current reconstructCurrentFromContext(const TriggerInfo &ctx,
	                                      const std::shared_ptr<Setting> &s);

	ExecUserFuncCallback m_execUserFuncCallback;  ///< callback for ExecUserFunc
private:
	// BEGINING OF FUNCTION DEFINITION
	/// send a default key to Windows
	void funcDefault(FunctionParam *i_param);
	/// use a corresponding key of a parent keymap
	void funcKeymapParent(FunctionParam *i_param);
	/// use a corresponding key of a current window
	void funcKeymapWindow(FunctionParam *i_param);
	/// use a corresponding key of the previous prefixed keymap
	void funcKeymapPrevPrefix(FunctionParam *i_param, int i_previous);
	/// use a corresponding key of an other window class, or use a default key
	void funcOtherWindowClass(FunctionParam *i_param);
	/// prefix key
	void funcPrefix(FunctionParam *i_param, const Keymap *i_keymap,
					BooleanType i_doesIgnoreModifiers = BooleanType_true);
	/// other keymap's key
	void funcKeymap(FunctionParam *i_param, const Keymap *i_keymap);
	/// sync
	void funcSync(FunctionParam *i_param);
	/// toggle lock
	void funcToggle(FunctionParam *i_param, ModifierLockType i_lock,
					ToggleType i_toggle = ToggleType_toggle);
	/// edit next user input key's modifier
	void funcEditNextModifier(FunctionParam *i_param,
							  const Modifier &i_modifier);
	/// variable
	void funcVariable(FunctionParam *i_param, int i_mag, int i_inc);
	/// repeat N times
	void funcRepeat(FunctionParam *i_param, const KeySeq *i_keySeq,
					int i_max = 10);
	/// undefined (bell)
	void funcUndefined(FunctionParam *i_param);
	/// ignore
	void funcIgnore(FunctionParam *i_param);
	/// post message
	void funcPostMessage(FunctionParam *i_param, ToWindowType i_window,
						 UINT i_message, WPARAM i_wParam, LPARAM i_lParam);
	/// ShellExecute
	void funcShellExecute(FunctionParam *i_param, const StrExprArg &i_operation,
						  const StrExprArg &i_file, const StrExprArg &i_parameters,
						  const StrExprArg &i_directory,
						  ShowCommandType i_showCommand);
	/// SetForegroundWindow
	void funcSetForegroundWindow(FunctionParam *i_param,
								 const wregex_stored &i_windowClassName,
								 LogicalOperatorType i_logicalOp
								 = LogicalOperatorType_and,
								 const wregex_stored &i_windowTitleName
								 = wregex_stored(L".*"));
	/// load setting
	void funcLoadSetting(FunctionParam *i_param,
						 const StrExprArg &i_name = StrExprArg());
	/// virtual key
	void funcVK(FunctionParam *i_param, VKey i_vkey);
	/// wait
	void funcWait(FunctionParam *i_param, int i_milliSecond);
	/// investigate WM_COMMAND, WM_SYSCOMMAND
	void funcInvestigateCommand(FunctionParam *i_param);
	/// show mayu dialog box
	void funcMayuDialog(FunctionParam *i_param, MayuDialogType i_dialog,
						ShowCommandType i_showCommand);
	/// describe bindings
	void funcDescribeBindings(FunctionParam *i_param);
	/// show help message
	void funcHelpMessage(FunctionParam *i_param,
						 const StrExprArg &i_title = StrExprArg(),
						 const StrExprArg &i_message = StrExprArg());
	/// show variable
	void funcHelpVariable(FunctionParam *i_param, const StrExprArg &i_title);
	/// raise window
	void funcWindowRaise(FunctionParam *i_param,
						 TargetWindowType i_twt = TargetWindowType_overlapped);
	/// lower window
	void funcWindowLower(FunctionParam *i_param,
						 TargetWindowType i_twt = TargetWindowType_overlapped);
	/// minimize window
	void funcWindowMinimize(FunctionParam *i_param, TargetWindowType i_twt
							= TargetWindowType_overlapped);
	/// maximize window
	void funcWindowMaximize(FunctionParam *i_param, TargetWindowType i_twt
							= TargetWindowType_overlapped);
	/// maximize window horizontally
	void funcWindowHMaximize(FunctionParam *i_param, TargetWindowType i_twt
							 = TargetWindowType_overlapped);
	/// maximize window virtically
	void funcWindowVMaximize(FunctionParam *i_param, TargetWindowType i_twt
							 = TargetWindowType_overlapped);
	/// maximize window virtically or horizontally
	void funcWindowHVMaximize(FunctionParam *i_param, BooleanType i_isHorizontal,
							  TargetWindowType i_twt
							  = TargetWindowType_overlapped);
	/// move window
	void funcWindowMove(FunctionParam *i_param, int i_dx, int i_dy,
						TargetWindowType i_twt
						= TargetWindowType_overlapped);
	/// move window to ...
	void funcWindowMoveTo(FunctionParam *i_param, GravityType i_gravityType,
						  int i_dx, int i_dy, TargetWindowType i_twt
						  = TargetWindowType_overlapped);
	/// move window visibly
	void funcWindowMoveVisibly(FunctionParam *i_param,
							   TargetWindowType i_twt
							   = TargetWindowType_overlapped);
	/// move window to other monitor
	void funcWindowMonitorTo(FunctionParam *i_param,
							 WindowMonitorFromType i_fromType, int i_monitor,
							 BooleanType i_adjustPos = BooleanType_true,
							 BooleanType i_adjustSize = BooleanType_false);
	/// move window to other monitor
	void funcWindowMonitor(FunctionParam *i_param, int i_monitor,
						   BooleanType i_adjustPos = BooleanType_true,
						   BooleanType i_adjustSize = BooleanType_false);
	///
	void funcWindowClingToLeft(FunctionParam *i_param,
							   TargetWindowType i_twt
							   = TargetWindowType_overlapped);
	///
	void funcWindowClingToRight(FunctionParam *i_param,
								TargetWindowType i_twt
								= TargetWindowType_overlapped);
	///
	void funcWindowClingToTop(FunctionParam *i_param,
							  TargetWindowType i_twt
							  = TargetWindowType_overlapped);
	///
	void funcWindowClingToBottom(FunctionParam *i_param,
								 TargetWindowType i_twt
								 = TargetWindowType_overlapped);
	/// close window
	void funcWindowClose(FunctionParam *i_param,
						 TargetWindowType i_twt = TargetWindowType_overlapped);
	/// toggle top-most flag of the window
	void funcWindowToggleTopMost(FunctionParam *i_param);
	/// identify the window
	void funcWindowIdentify(FunctionParam *i_param);
	/// set alpha blending parameter to the window
	void funcWindowSetAlpha(FunctionParam *i_param, int i_alpha);
	/// redraw the window
	void funcWindowRedraw(FunctionParam *i_param);
	/// resize window to
	void funcWindowResizeTo(FunctionParam *i_param, int i_width, int i_height,
							TargetWindowType i_twt
							= TargetWindowType_overlapped);
	/// move the mouse cursor
	void funcMouseMove(FunctionParam *i_param, int i_dx, int i_dy);
	/// send a mouse-wheel-message to Windows
	void funcMouseWheel(FunctionParam *i_param, int i_delta);
	/// convert the contents of the Clipboard to upper case or lower case
	void funcClipboardChangeCase(FunctionParam *i_param,
								 BooleanType i_doesConvertToUpperCase);
	/// convert the contents of the Clipboard to upper case
	void funcClipboardUpcaseWord(FunctionParam *i_param);
	/// convert the contents of the Clipboard to lower case
	void funcClipboardDowncaseWord(FunctionParam *i_param);
	/// set the contents of the Clipboard to the string
	void funcClipboardCopy(FunctionParam *i_param, const StrExprArg &i_text);
	///
	void funcEmacsEditKillLinePred(FunctionParam *i_param,
								   const KeySeq *i_keySeq1,
								   const KeySeq *i_keySeq2);
	///
	void funcEmacsEditKillLineFunc(FunctionParam *i_param);
	/// clear log
	void funcLogClear(FunctionParam *i_param);
	/// recenter
	void funcRecenter(FunctionParam *i_param);
	/// Direct SSTP
	void funcDirectSSTP(FunctionParam *i_param,
						const wregex_stored &i_name,
						const StrExprArg &i_protocol,
						const std::list<wstringq> &i_headers);
	/// PlugIn
	void funcPlugIn(FunctionParam *i_param,
					const StrExprArg &i_dllName,
					const StrExprArg &i_funcName = StrExprArg(),
					const StrExprArg &i_funcParam = StrExprArg(),
					BooleanType i_doesCreateThread = BooleanType_false);
	/// set IME open status
	void funcSetImeStatus(FunctionParam *i_param, ToggleType i_toggle = ToggleType_toggle);
	/// set string to IME
	void funcSetImeString(FunctionParam *i_param, const StrExprArg &i_data);
	/// enter to mouse event hook mode
	void funcMouseHook(FunctionParam *i_param, MouseHookType i_hookType, int i_hookParam);
	/// cancel prefix
	void funcCancelPrefix(FunctionParam *i_param);
	/// exec user function
	void funcExecUserFunc(FunctionParam *i_param, const wstringq &i_name,
		const std::vector<FuncArg> &i_args);

	// END OF FUNCTION DEFINITION
#  define FUNCTION_FRIEND
#  include "functions.h"
#  undef FUNCTION_FRIEND

public:
	///
	Engine(womsgstream &i_log);
	///
	~Engine();

	/// start/stop keyboard handler thread
	void start();
	///
	void stop();

	/// parallel shutdown: stop InputHandlers in parallel, signal engine to exit,
	/// and return the engine thread handle for external WaitForMultipleObjects.
	/// Must be called before cleanupAfterStop().
	HANDLE signalStop();
	/// release engine resources after the engine thread handle has been waited on.
	void cleanupAfterStop(HANDLE hEngineThread);

	/// do some procedure before quit which must be done synchronously
	/// (i.e. not on WM_QUIT)
	bool prepairQuit();

	/// logging mode
	void enableLogMode(bool i_isLogMode = true) {
		m_isLogMode = i_isLogMode;
	}
	///
	void disableLogMode() {
		m_isLogMode = false;
	}

	/// enable/disable engine
	void enable(bool i_isEnabled = true) {
		m_isEnabled = i_isEnabled;
	}
	///
	void disable() {
		m_isEnabled = false;
	}
	///
	bool getIsEnabled() const {
		return m_isEnabled;
	}

	/// request a forced key state resync (called on session unlock)
	void unlocked();

	/// associated window
	void setAssociatedWndow(HWND i_hwnd) {
		m_hwndAssocWindow = i_hwnd;
	}

	/// associated window
	HWND getAssociatedWndow() const {
		return m_hwndAssocWindow;
	}

	/// schedule a Setting for activation in the engine thread
	void scheduleSetting(std::shared_ptr<Setting> i_setting);

	/// schedule an ad-hoc key sequence for processing in the engine thread
	void scheduleAdHocKeySeq(AdHocKeySeq item);

	void setExecUserFuncCallback(ExecUserFuncCallback callback);
	void callExecUserFuncCallback(const wstringi &name,
	                              const std::vector<FuncArg> &args,
	                              const TriggerInfo &ctx);

	/// focus
	bool setFocus(HWND i_hwndFocus, DWORD i_threadId,
				  const wstringi &i_className,
				  const wstringi &i_titleName, bool i_isConsole);

	/// lock state
	bool setLockState(bool i_isNumLockToggled, bool i_isCapsLockToggled,
					  bool i_isScrollLockToggled, bool i_isKanaLockToggled,
					  bool i_isImeLockToggled, bool i_isImeCompToggled);

	/// show
	void checkShow(HWND i_hwnd);
	bool setShow(bool i_isMaximized, bool i_isMinimized, bool i_isMDI);

	/// sync
	bool syncNotify();

	/// thread attach notify
	bool threadAttachNotify(DWORD i_threadId);

	/// thread detach notify
	bool threadDetachNotify(DWORD i_threadId);

	/// shell execute
	void shellExecute();

	/// get help message
	void getHelpMessages(std::wstring *o_helpMessage, std::wstring *o_helpTitle);

	/// command notify
	template <typename WPARAM_T, typename LPARAM_T>
	void commandNotify(HWND i_hwnd, UINT i_message, WPARAM_T i_wParam,
					   LPARAM_T i_lParam)
	{
		Acquire b(&m_log, 0);
		HWND hf = m_hwndFocus;
		if (!hf)
			return;

		if (GetWindowThreadProcessId(hf, NULL) ==
				GetWindowThreadProcessId(m_hwndAssocWindow, NULL))
			return;	// inhibit the investigation of MADO TSUKAI NO YUUTSU

		const wchar_t *target = NULL;
		int number_target = 0;

		if (i_hwnd == hf)
			target = L"ToItself";
		else if (i_hwnd == GetParent(hf))
			target = L"ToParentWindow";
		else {
			// Function::toMainWindow
			HWND h = hf;
			while (true) {
				HWND p = GetParent(h);
				if (!p)
					break;
				h = p;
			}
			if (i_hwnd == h)
				target = L"ToMainWindow";
			else {
				// Function::toOverlappedWindow
				HWND h2 = hf;
				while (h2) {
					LONG_PTR style = GetWindowLongPtr(h2, GWL_STYLE);
					if ((style & WS_CHILD) == 0)
						break;
					h2 = GetParent(h2);
				}
				if (i_hwnd == h2)
					target = L"ToOverlappedWindow";
				else {
					// number
					HWND h3 = hf;
					for (number_target = 0; h3; number_target ++, h3 = GetParent(h3))
						if (i_hwnd == h3)
							break;
					return;
				}
			}
		}

		m_log << L"&PostMessage(";
		if (target)
			m_log << target;
		else
			m_log << number_target;
		m_log << L", " << i_message
		<< L", 0x" << std::hex << i_wParam
		<< L", 0x" << i_lParam << L") # hwnd = "
		<< static_cast<DWORD>(reinterpret_cast<uintptr_t>(i_hwnd)) << L", "
		<< L"message = " << std::dec;
		if (i_message == WM_COMMAND)
			m_log << L"WM_COMMAND, ";
		else if (i_message == WM_SYSCOMMAND)
			m_log << L"WM_SYSCOMMAND, ";
		else
			m_log << i_message << L", ";
		m_log << L"wNotifyCode = " << HIWORD(i_wParam) << L", "
		<< L"wID = " << LOWORD(i_wParam) << L", "
		<< L"hwndCtrl = 0x" << std::hex << i_lParam << std::dec << std::endl;
	}

	/// get current window class name
	const wstringi &getCurrentWindowClassName() const {
		return m_currentFocusOfThread->m_className;
	}

	/// get current window title name
	const wstringi &getCurrentWindowTitleName() const {
		return m_currentFocusOfThread->m_titleName;
	}
};


///
class FunctionParam
{
public:
	bool m_isPressed;				/// is key pressed ?
	HWND m_hwnd;					///
	Engine::Current m_c;				/// new context
	bool m_doesNeedEndl;				/// need endl ?
	const ActionFunction *m_af;			///
};


#endif // !_ENGINE_H
