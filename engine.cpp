//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// engine.cpp


#include "misc.h"

#include "engine.h"
#include "errormessage.h"
#include "hook.h"
#include "mayurc.h"
#include "windowstool.h"

#include <iomanip>

#include <process.h>


/** Thread whose focus m_focusOfThreads holds for this foreground window.

    The hooks report a focus under the thread that observed it
    (NotifySetFocus::m_threadId is the reporting thread), and an application is
    free to own its top level window on one thread and the control that takes
    the keyboard on another - WinUI 3 does exactly that, and Windows 11's
    Notepad puts every tab on a thread of its own.  Those threads share an input
    queue, so GetGUIThreadInfo() answers for any of them; asking the foreground
    thread therefore names the window that actually has the focus, and the
    thread owning that window is the one that reported it.

    Keying off the foreground thread instead looks up an entry that no longer
    describes where the keyboard is going: moving the focus to a window owned by
    another thread of the same application notifies only that other thread, and
    leaves the foreground thread's entry behind, pointing at whatever it saw
    last.  Switching Notepad tabs left the keymaps on the tab that was open
    before.
*/
static DWORD focusThreadOf(HWND i_hwndFore)
{
	DWORD threadId = GetWindowThreadProcessId(i_hwndFore, NULL);
	GUITHREADINFO gti;
	memset(&gti, 0, sizeof(gti));
	gti.cbSize = sizeof(gti);
	// hwndFocus is NULL whenever the queue has no focus window at all - during
	// an activation change, or while a menu has taken the input - and then the
	// foreground thread is still the best answer available.
	if (GetGUIThreadInfo(threadId, &gti) && gti.hwndFocus)
		threadId = GetWindowThreadProcessId(gti.hwndFocus, NULL);
	return threadId;
}


// check focus window
void Engine::checkFocusWindow()
{
	int count = 0;

restart:
	count ++;

	HWND hwndFore = GetForegroundWindow();
	DWORD threadId = focusThreadOf(hwndFore);

	if (hwndFore) {
		{
			Lock lock(this);
			if (m_currentFocusOfThread &&
					m_currentFocusOfThread->m_threadId == threadId &&
					m_currentFocusOfThread->m_hwndFocus == m_hwndFocus)
				return;

			m_emacsEditKillLine.reset();

			// erase dead thread
			if (!m_detachedThreadIds.empty()) {
				for (ThreadIds::iterator i = m_detachedThreadIds.begin();
						i != m_detachedThreadIds.end(); i ++) {
					FocusOfThreads::iterator j = m_focusOfThreads.find((*i));
					if (j != m_focusOfThreads.end()) {
						FocusOfThread *fot = &((*j).second);
						Acquire a(&m_log, LogLevel::Debug);
						m_log << L"RemoveThread" << std::endl;
						m_log << L"  HWND:     " << std::hex << static_cast<DWORD>(reinterpret_cast<uintptr_t>(fot->m_hwndFocus))
						<< std::dec << std::endl;
						m_log << L"  THREADID: " << fot->m_threadId << std::endl;
						m_log << L"  CLASS:    " << fot->m_className << std::endl;
						m_log << L"  TITLE:   \"" << fot->m_titleName << L"\"" << std::endl;
						m_log << std::endl;
						m_focusOfThreads.erase(j);
					}
				}
				m_detachedThreadIds.erase
				(m_detachedThreadIds.begin(), m_detachedThreadIds.end());
			}

			FocusOfThreads::iterator i = m_focusOfThreads.find(threadId);
			if (i != m_focusOfThreads.end()) {
				m_currentFocusOfThread = &((*i).second);
				if (!m_currentFocusOfThread->m_isConsole || 2 <= count) {
					if (m_currentFocusOfThread->m_keymaps.empty())
						setCurrentKeymap(NULL);
					else
						setCurrentKeymap(*m_currentFocusOfThread->m_keymaps.begin());
					m_hwndFocus = m_currentFocusOfThread->m_hwndFocus;
					checkShow(m_hwndFocus);

					Acquire a(&m_log, LogLevel::Debug);
					m_log << L"FocusChanged" << std::endl;
					m_log << L"  HWND:     "
					<< std::hex << static_cast<DWORD>(reinterpret_cast<uintptr_t>(m_currentFocusOfThread->m_hwndFocus))
					<< std::dec << std::endl;
					m_log << L"  THREADID: "
					<< m_currentFocusOfThread->m_threadId << std::endl;
					m_log << L"  CLASS:    "
					<< m_currentFocusOfThread->m_className << std::endl;
					m_log << L"  TITLE:   \""
					<< m_currentFocusOfThread->m_titleName << L"\"" << std::endl;
					m_log << std::endl;
					return;
				}
			}
		}

		wchar_t className[GANA_MAX_ATOM_LENGTH];
		if (GetClassName(hwndFore, className, NUMBER_OF(className))) {
			if (_wcsicmp(className, L"ConsoleWindowClass") == 0) {
				wchar_t titleName[1024];
				if (GetWindowText(hwndFore, titleName, NUMBER_OF(titleName)) == 0)
					titleName[0] = L'\0';
				// This path reads the title itself instead of going through the
				// hook, so it has to do the hook's escaping too - the stored
				// title is what window matchers see.
				std::wstring title = escapeControlChars(titleName);
				setFocus(hwndFore, threadId, className, title.c_str(), true);
				Acquire a(&m_log, LogLevel::Debug);
				m_log << L"HWND:     " << std::hex << static_cast<DWORD>(reinterpret_cast<uintptr_t>(hwndFore))
				<< std::dec << std::endl;
				m_log << L"THREADID: " << threadId << std::endl;
				m_log << L"CLASS:    " << className << std::endl;
				m_log << L"TITLE:   \"" << title << L"\"" << std::endl << std::endl;
				goto restart;
			}
		}
	}

	Lock lock(this);
	if (m_globalFocus.m_keymaps.empty()) {
		Acquire a(&m_log, LogLevel::Debug);
		m_log << L"NO GLOBAL FOCUS" << std::endl;
		m_currentFocusOfThread = NULL;
		setCurrentKeymap(NULL);
	} else {
		if (m_currentFocusOfThread != &m_globalFocus) {
			Acquire a(&m_log, LogLevel::Debug);
			m_log << L"GLOBAL FOCUS" << std::endl;
			m_currentFocusOfThread = &m_globalFocus;
			setCurrentKeymap(m_globalFocus.m_keymaps.front());
		}
	}
	m_hwndFocus = NULL;
}



// is modifier pressed ?
bool Engine::isPressed(Modifier::Type i_mt)
{
	const Keymap::ModAssignments &ma = m_currentKeymap->getModAssignments(i_mt);
	for (Keymap::ModAssignments::const_iterator i = ma.begin();
			i != ma.end(); ++ i)
		if ((*i).m_key->m_isPressed)
			return true;
	return false;
}


// fix modifier key (if fixed, return true)
bool Engine::fixModifierKey(ModifiedKey *io_mkey, Keymap::AssignMode *o_am)
{
	// for all modifier ...
	for (int i = Modifier::Type_begin; i != Modifier::Type_end; ++ i) {
		// get modifier assignments (list of modifier keys)
		const Keymap::ModAssignments &ma =
			m_currentKeymap->getModAssignments(static_cast<Modifier::Type>(i));

		for (Keymap::ModAssignments::const_iterator
				j = ma.begin(); j != ma.end(); ++ j)
			if (io_mkey->m_key == (*j).m_key) { // is io_mkey a modifier ?
				logNote(LogLevel::Debug, L"Modifier Key");
				// set dontcare for this modifier
				io_mkey->m_modifier.dontcare(static_cast<Modifier::Type>(i));
				*o_am = (*j).m_assignMode;
				return true;
			}
	}
	*o_am = Keymap::AM_notModifier;
	return false;
}


// write 2 spaces per nesting level
void Engine::logIndent(int i_level)
{
	static const wchar_t spaces[] = L"                                ";
	int n = i_level * 2;
	if (n < 0)
		n = 0;
	if (NUMBER_OF(spaces) - 1 < static_cast<size_t>(n))
		n = NUMBER_OF(spaces) - 1;
	m_log.write(spaces, n);
}


// write "{indent}{mark}{note}{scan codes}  {key}"
//
// Everything is padded with spaces rather than tabs: the log control's tab
// stops depend on the font the user picked, so tabs put the columns in a
// different place for everyone.
void Engine::writeKeyLine(const Key *i_key, const ModifiedKey &i_mkey,
						  const wchar_t *i_mark, const wchar_t *i_note)
{
	logIndent(m_logIndent);
	m_log << i_mark;
	if (i_note)
		m_log << i_note;

	// scan codes; the "E0-" / "E1-" slot is blank-filled when absent so that
	// the hex codes stay in one column
	for (size_t i = 0; i < i_key->getScanCodesSize(); ++ i) {
		const ScanCode &sc = i_key->getScanCodes()[i];
		if (sc.m_flags & ScanCode::E0)
			m_log << L"E0-";
		else if (sc.m_flags & ScanCode::E1)
			m_log << L"E1-";
		else
			m_log << L"   ";
		m_log << L"0x" << std::hex << std::setw(2) << std::setfill(L'0')
		<< static_cast<int>(sc.m_scan) << std::dec << L" ";
	}

	if (i_mkey.m_key)		// otherwise the key matches no physical key
		m_log << L" " << i_mkey;
	m_log << std::endl;
}


// write a "*" note line at the current nesting level
void Engine::logNote(LogLevel i_level, const wchar_t *i_text)
{
	if (!m_log.wouldLog(i_level))
		return;
	Acquire a(&m_log, i_level);
	logIndent(m_logIndent);
	m_log << L"*   " << i_text << std::endl;
}


// output one key to m_log
void Engine::outputToLog(const Key *i_key, const ModifiedKey &i_mkey,
						 LogLevel i_level, const wchar_t *i_mark,
						 const wchar_t *i_note)
{
	// the ostream formatting below runs even when the result is discarded,
	// and it is the most expensive thing on the key input path
	if (!m_log.wouldLog(i_level))
		return;
	Acquire a(&m_log, i_level);
	writeKeyLine(i_key, i_mkey, i_mark, i_note);
}


// output a physical key event; opens a new block in the log
void Engine::outputInputToLog(const Key *i_key, const ModifiedKey &i_mkey,
							  LogLevel i_level)
{
	m_logIndent = 0;
	if (m_log.wouldLog(i_level)) {
		Acquire a(&m_log, i_level);
		m_log << std::endl;	// blank line between one keystroke and the next
		writeKeyLine(i_key, i_mkey, L"IN ", NULL);
	}
	m_logIndent = 1;
}


// describe bindings
void Engine::describeBindings()
{
	Acquire a(&m_log, LogLevel::Info);

	Keymap::DescribeParam dp;
	for (KeymapPtrList::iterator i = m_currentFocusOfThread->m_keymaps.begin();
			i != m_currentFocusOfThread->m_keymaps.end(); ++ i)
		(*i)->describe(m_log, &dp);
	m_log << std::endl;
}


// update m_lastPressedKey
void Engine::updateLastPressedKey(Key *i_key)
{
	m_lastPressedKey[1] = m_lastPressedKey[0];
	m_lastPressedKey[0] = i_key;
}

// set current keymap
void Engine::setCurrentKeymap(const Keymap *i_keymap, bool i_doesAddToHistory)
{
	if (i_doesAddToHistory) {
		m_keymapPrefixHistory.push_back(const_cast<Keymap *>(m_currentKeymap));
		if (MAX_KEYMAP_PREFIX_HISTORY < m_keymapPrefixHistory.size())
			m_keymapPrefixHistory.pop_front();
	} else
		m_keymapPrefixHistory.clear();
	m_currentKeymap = i_keymap;
}


// get current modifiers
Modifier Engine::getCurrentModifiers(Key *i_key, bool i_isPressed)
{
	Modifier cmods;
	cmods.add(m_currentLock);

	cmods.press(Modifier::Type_Shift  , isPressed(Modifier::Type_Shift  ));
	cmods.press(Modifier::Type_Alt    , isPressed(Modifier::Type_Alt    ));
	cmods.press(Modifier::Type_Control, isPressed(Modifier::Type_Control));
	cmods.press(Modifier::Type_Windows, isPressed(Modifier::Type_Windows));
	cmods.press(Modifier::Type_Up     , !i_isPressed);
	cmods.press(Modifier::Type_Down   , i_isPressed);

	cmods.press(Modifier::Type_Repeat , false);
	if (m_lastPressedKey[0] == i_key) {
		if (i_isPressed)
			cmods.press(Modifier::Type_Repeat, true);
		else
			if (m_lastPressedKey[1] == i_key)
				cmods.press(Modifier::Type_Repeat, true);
	}

	for (int i = Modifier::Type_Mod0; i <= Modifier::Type_Mod9; ++ i)
		cmods.press(static_cast<Modifier::Type>(i),
					isPressed(static_cast<Modifier::Type>(i)));

	return cmods;
}


// generate keyboard event for a key
void Engine::generateKeyEvent(Key *i_key, bool i_doPress, bool i_isByAssign)
{
	auto s = m_setting.load(std::memory_order_relaxed);
	// check if key is event
	bool isEvent = false;
	for (Key **e = Event::events; *e; ++ e)
		if (*e == i_key) {
			isEvent = true;
			break;
		}

	bool isAlreadyReleased = false;

	if (!isEvent) {
		if (i_doPress && !i_key->m_isPressedOnWin32)
			++ m_currentKeyPressCountOnWin32;
		else if (!i_doPress) {
			if (i_key->m_isPressedOnWin32)
				-- m_currentKeyPressCountOnWin32;
			else
				isAlreadyReleased = true;
		}
		i_key->m_isPressedOnWin32 = i_doPress;

		if (i_isByAssign)
			i_key->m_isPressedByAssign = i_doPress;

		Key *sync = s->m_keyboard.getSyncKey();

		if (!isAlreadyReleased || i_key == sync) {
			KEYBOARD_INPUT_DATA kid = { 0, 0, 0, 0, 0 };
			const ScanCode *sc = i_key->getScanCodes();
			for (size_t i = 0; i < i_key->getScanCodesSize(); ++ i) {
				kid.MakeCode = sc[i].m_scan;
				kid.Flags = sc[i].m_flags;
				if (!i_doPress)
					kid.Flags |= KEYBOARD_INPUT_DATA::BREAK;
				injectInput(&kid, NULL);
			}

			m_lastGeneratedKey = i_doPress ? i_key : NULL;
		}
	}

	// the "Gen Modifiers" header is written here rather than by
	// generateModifierEvents(), so that an empty block prints nothing at all
	if (m_isGeneratingModifiers && !m_modifierHeaderWritten) {
		m_modifierHeaderWritten = true;
		logNote(LogLevel::Debug, L"Gen Modifiers");
		++ m_logIndent;
	}

	ModifiedKey mkey(i_key);
	mkey.m_modifier.on(Modifier::Type_Up, !i_doPress);
	mkey.m_modifier.on(Modifier::Type_Down, i_doPress);
	outputToLog(i_key, mkey, LogLevel::Debug, L"OUT",
				isAlreadyReleased ? L"(already released) " : NULL);
}


// genete event
void Engine::generateEvents(Current i_c, const Keymap *i_keymap, Key *i_event)
{
	// generate
	i_c.m_keymap = i_keymap;
	i_c.m_mkey.m_key = i_event;
	if (const Keymap::KeyAssignment *keyAssign =
				i_c.m_keymap->searchAssignment(i_c.m_mkey)) {
		if (m_log.wouldLog(LogLevel::Debug)) {
			Acquire a(&m_log, LogLevel::Debug);
			logIndent(m_logIndent);
			m_log << L"*   event " << i_event->getName() << std::endl;
		}
		generateKeySeqEvents(i_c, keyAssign->m_keySeq, Part_all);
	}
}


// genete modifier events
void Engine::generateModifierEvents(const Modifier &i_mod)
{
	auto s = m_setting.load(std::memory_order_relaxed);
	// The header and the indent are produced by generateKeyEvent(), the first
	// time it actually generates something.  Most blocks generate nothing.
	bool wasGenerating = m_isGeneratingModifiers;
	bool hadHeader = m_modifierHeaderWritten;
	m_isGeneratingModifiers = true;
	m_modifierHeaderWritten = false;

	for (int i = Modifier::Type_begin; i < Modifier::Type_BASIC; ++ i) {
		Keyboard::Mods &mods =
			s->m_keyboard.getModifiers(static_cast<Modifier::Type>(i));

		if (i_mod.isDontcare(static_cast<Modifier::Type>(i)))
			// no need to process
			;
		else if (i_mod.isPressed(static_cast<Modifier::Type>(i)))
			// we have to press this modifier
		{
			bool noneIsPressed = true;
			bool noneIsPressedByAssign = true;
			for (Keyboard::Mods::iterator i2 = mods.begin(); i2 != mods.end(); ++ i2) {
				if ((*i2)->m_isPressedOnWin32)
					noneIsPressed = false;
				if ((*i2)->m_isPressedByAssign)
					noneIsPressedByAssign = false;
			}
			if (noneIsPressed) {
				if (noneIsPressedByAssign)
					generateKeyEvent(mods.front(), true, false);
				else
					for (Keyboard::Mods::iterator
							it = mods.begin(); it != mods.end(); ++ it)
						if ((*it)->m_isPressedByAssign)
							generateKeyEvent((*it), true, false);
			}
		}

		else
			// we have to release this modifier
		{
			// avoid such sequences as  "Alt U-ALt" or "Windows U-Windows"
			if (i == Modifier::Type_Alt || i == Modifier::Type_Windows) {
				for (Keyboard::Mods::iterator j = mods.begin(); j != mods.end(); ++ j)
					if ((*j) == m_lastGeneratedKey) {
						Keyboard::Mods *mods2 =
							&s->m_keyboard.getModifiers(Modifier::Type_Shift);
						if (mods2->size() == 0)
							mods2 = &s->m_keyboard.getModifiers(
									   Modifier::Type_Control);
						if (0 < mods2->size()) {
							generateKeyEvent(mods2->front(), true, false);
							generateKeyEvent(mods2->front(), false, false);
						}
						break;
					}
			}

			for (Keyboard::Mods::iterator j = mods.begin(); j != mods.end(); ++ j) {
				if ((*j)->m_isPressedOnWin32)
					generateKeyEvent((*j), false, false);
			}
		}
	}

	if (m_modifierHeaderWritten)
		-- m_logIndent;
	m_isGeneratingModifiers = wasGenerating;
	m_modifierHeaderWritten = hadHeader;
}


// generate keyboard events for action
void Engine::generateActionEvents(const Current &i_c, const Action *i_a,
								  bool i_doPress)
{
	switch (i_a->getType()) {
		// key
	case Action::Type_key: {
		const ModifiedKey &mkey
		= reinterpret_cast<ActionKey *>(
			  const_cast<Action *>(i_a))->m_modifiedKey;

		// release
		if (!i_doPress &&
				(mkey.m_modifier.isOn(Modifier::Type_Up) ||
				 mkey.m_modifier.isDontcare(Modifier::Type_Up)))
			generateKeyEvent(mkey.m_key, false, true);

		// press
		else if (i_doPress &&
				 (mkey.m_modifier.isOn(Modifier::Type_Down) ||
				  mkey.m_modifier.isDontcare(Modifier::Type_Down))) {
			Modifier modifier = mkey.m_modifier;
			modifier.add(i_c.m_mkey.m_modifier);
			generateModifierEvents(modifier);
			generateKeyEvent(mkey.m_key, true, true);
		}
		break;
	}

	// keyseq
	case Action::Type_keySeq: {
		const ActionKeySeq *aks = reinterpret_cast<const ActionKeySeq *>(i_a);
		generateKeySeqEvents(i_c, aks->m_keySeq,
							 i_doPress ? Part_down : Part_up);
		break;
	}

	// function
	case Action::Type_function: {
		const ActionFunction *af = reinterpret_cast<const ActionFunction *>(i_a);
		bool is_up = (!i_doPress &&
					  (af->m_modifier.isOn(Modifier::Type_Up) ||
					   af->m_modifier.isDontcare(Modifier::Type_Up)));
		bool is_down = (i_doPress &&
						(af->m_modifier.isOn(Modifier::Type_Down) ||
						 af->m_modifier.isDontcare(Modifier::Type_Down)));

		if (!is_down && !is_up)
			break;

		if (m_log.wouldLog(LogLevel::Debug)) {
			Acquire a(&m_log, LogLevel::Debug);
			logIndent(m_logIndent);
			m_log << L"FN  " << af->m_functionData.get();
		}

		FunctionParam param;
		param.m_isPressed = i_doPress;
		param.m_hwnd = m_currentFocusOfThread->m_hwndFocus;
		param.m_c = i_c;
		param.m_doesNeedEndl = true;
		param.m_af = af;

		param.m_c.m_mkey.m_modifier.on(Modifier::Type_Up, !i_doPress);
		param.m_c.m_mkey.m_modifier.on(Modifier::Type_Down, i_doPress);

		af->m_functionData->exec(this, &param);

		if (param.m_doesNeedEndl) {
			Acquire a(&m_log, LogLevel::Debug);
			m_log << std::endl;
		}
		break;
	}
	}
}


// generate keyboard events for keySeq
void Engine::generateKeySeqEvents(const Current &i_c, const KeySeq *i_keySeq,
								  Part i_part)
{
	const KeySeq::Actions &actions = i_keySeq->getActions();
	if (actions.empty())
		return;
	if (i_part == Part_up)
		generateActionEvents(i_c, actions[actions.size() - 1].get(), false);
	else {
		size_t i;
		for (i = 0 ; i < actions.size() - 1; ++ i) {
			if (m_isAborting)
				return;
			generateActionEvents(i_c, actions[i].get(), true);
			generateActionEvents(i_c, actions[i].get(), false);
		}
		generateActionEvents(i_c, actions[i].get(), true);
		if (i_part == Part_all)
			generateActionEvents(i_c, actions[i].get(), false);
	}
}


// generate keyboard events for current key
void Engine::generateKeyboardEvents(const Current &i_c)
{
	// a &Sync or &Wait in this key sequence was cut short by shutdown; the
	// hooks are already uninstalled, so the rest of the sequence would only
	// inject keys nobody is listening for
	if (m_isAborting)
		return;

	if (++ m_generateKeyboardEventsRecursionGuard ==
			MAX_GENERATE_KEYBOARD_EVENTS_RECURSION_COUNT) {
		Acquire a(&m_log, LogLevel::Error);
		m_log << L"too deep keymap recursion.  there may be a loop."
		<< std::endl;
		-- m_generateKeyboardEventsRecursionGuard;
		return;
	}

	if (i_c.m_adhocKeySeq) {
		generateKeySeqEvents(i_c, i_c.m_adhocKeySeq, Part_all);
	} else {
		const Keymap::KeyAssignment *keyAssign
		= i_c.m_keymap->searchAssignment(i_c.m_mkey);
		if (!keyAssign) {
			const KeySeq *keySeq = i_c.m_keymap->getDefaultKeySeq();
			ASSERT( keySeq );
			generateKeySeqEvents(i_c, keySeq, i_c.isPressed() ? Part_down : Part_up);
		} else {
			if (keyAssign->m_modifiedKey.m_modifier.isOn(Modifier::Type_Up) ||
					keyAssign->m_modifiedKey.m_modifier.isOn(Modifier::Type_Down))
				generateKeySeqEvents(i_c, keyAssign->m_keySeq, Part_all);
			else
				generateKeySeqEvents(i_c, keyAssign->m_keySeq,
									 i_c.isPressed() ? Part_down : Part_up);
		}
	}
	m_generateKeyboardEventsRecursionGuard --;
}


// generate keyboard events for current key
void Engine::beginGeneratingKeyboardEvents(
	const Current &i_c, bool i_isModifier)
{
	auto s = m_setting.load(std::memory_order_relaxed);
	//             (1)             (2)             (3)  (4)   (1)
	// up/down:    D-              U-              D-   U-    D-
	// keymap:     m_currentKeymap m_currentKeymap X    X     m_currentKeymap
	// memo:       &Prefix(X)      ...             ...  ...   ...
	// m_isPrefix: false           true            true false false

	Current cnew(i_c);

	bool isPhysicallyPressed
	= cnew.m_mkey.m_modifier.isPressed(Modifier::Type_Down);

	// substitute
	ModifiedKey mkey = s->m_keyboard.searchSubstitute(cnew.m_mkey);
	if (mkey.m_key) {
		cnew.m_mkey = mkey;
		if (isPhysicallyPressed) {
			cnew.m_mkey.m_modifier.off(Modifier::Type_Up);
			cnew.m_mkey.m_modifier.on(Modifier::Type_Down);
		} else {
			cnew.m_mkey.m_modifier.on(Modifier::Type_Up);
			cnew.m_mkey.m_modifier.off(Modifier::Type_Down);
		}
		for (int i = Modifier::Type_begin; i != Modifier::Type_end; ++ i) {
			Modifier::Type type = static_cast<Modifier::Type>(i);
			if (cnew.m_mkey.m_modifier.isDontcare(type) &&
					!i_c.m_mkey.m_modifier.isDontcare(type))
				cnew.m_mkey.m_modifier.press(
					type, i_c.m_mkey.m_modifier.isPressed(type));
		}

		logNote(LogLevel::Debug, L"substitute");
		outputToLog(mkey.m_key, cnew.m_mkey, LogLevel::Debug, L"IN ");
	}

	// for prefix key
	const Keymap *tmpKeymap = m_currentKeymap;
	if (i_isModifier || !m_isPrefix) ;
	else if (isPhysicallyPressed)			// when (3)
		m_isPrefix = false;
	else if (!isPhysicallyPressed)		// when (2)
		m_currentKeymap = m_currentFocusOfThread->m_keymaps.front();

	// for m_emacsEditKillLine function
	m_emacsEditKillLine.m_doForceReset = !i_isModifier;

	// generate key event !
	m_generateKeyboardEventsRecursionGuard = 0;
	if (isPhysicallyPressed)
		generateEvents(cnew, cnew.m_keymap, &Event::before_key_down);
	generateKeyboardEvents(cnew);
	if (!isPhysicallyPressed)
		generateEvents(cnew, cnew.m_keymap, &Event::after_key_up);

	// for m_emacsEditKillLine function
	if (m_emacsEditKillLine.m_doForceReset)
		m_emacsEditKillLine.reset();

	// for prefix key
	if (i_isModifier)
		;
	else if (!m_isPrefix)				// when (1), (4)
		m_currentKeymap = m_currentFocusOfThread->m_keymaps.front();
	else if (!isPhysicallyPressed)		// when (2)
		m_currentKeymap = tmpKeymap;
}


/** Convert a screen point for MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK.

    The normalized range covers the whole virtual desktop, so the conversion has
    to start from its origin: that is negative whenever a secondary monitor sits
    left of or above the primary one, and leaving it out sent the cursor to the
    wrong place on such an arrangement.  The range is also inclusive at both
    ends, hence the division by one less than the extent.
*/
static void toVirtualDesktopAbsolute(POINT i_pt, LONG *o_dx, LONG *o_dy)
{
	int width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
	int height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
	*o_dx = (1 < width)
			? MulDiv(i_pt.x - GetSystemMetrics(SM_XVIRTUALSCREEN),
					 65535, width - 1)
			: 0;
	*o_dy = (1 < height)
			? MulDiv(i_pt.y - GetSystemMetrics(SM_YVIRTUALSCREEN),
					 65535, height - 1)
			: 0;
}


unsigned int Engine::injectInput(const KEYBOARD_INPUT_DATA *i_kid, const KBDLLHOOKSTRUCT *i_kidRaw)
{
	if (i_kid->Flags & KEYBOARD_INPUT_DATA::E1) {
		INPUT kid[2];
		int count = 1;

		kid[0].type = INPUT_MOUSE;
		kid[0].mi.dx = 0;
		kid[0].mi.dy = 0;
		kid[0].mi.time = 0;
		kid[0].mi.mouseData = 0;
		kid[0].mi.dwExtraInfo = 0;
		switch (i_kid->MakeCode) {
		case 1:
			if (i_kid->Flags & KEYBOARD_INPUT_DATA::BREAK) {
				kid[0].mi.dwFlags = MOUSEEVENTF_LEFTUP;
			} else {
				kid[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
			}
			break;
		case 2:
			if (i_kid->Flags & KEYBOARD_INPUT_DATA::BREAK) {
				kid[0].mi.dwFlags = MOUSEEVENTF_RIGHTUP;
			} else {
				kid[0].mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;
			}
			break;
		case 3:
			if (i_kid->Flags & KEYBOARD_INPUT_DATA::BREAK) {
				kid[0].mi.dwFlags = MOUSEEVENTF_MIDDLEUP;
			} else {
				kid[0].mi.dwFlags = MOUSEEVENTF_MIDDLEDOWN;
			}
			break;
		case 4:
			if (i_kid->Flags & KEYBOARD_INPUT_DATA::BREAK) {
				return 1;
			} else {
				kid[0].mi.mouseData = WHEEL_DELTA;
				kid[0].mi.dwFlags = MOUSEEVENTF_WHEEL;
			}
			break;
		case 5:
			if (i_kid->Flags & KEYBOARD_INPUT_DATA::BREAK) {
				return 1;
			} else {
				kid[0].mi.mouseData = static_cast<DWORD>(- WHEEL_DELTA);
				kid[0].mi.dwFlags = MOUSEEVENTF_WHEEL;
			}
			break;
		case 6:
			kid[0].mi.mouseData = XBUTTON1;
			if (i_kid->Flags & KEYBOARD_INPUT_DATA::BREAK) {
				kid[0].mi.dwFlags = MOUSEEVENTF_XUP;
			} else {
				kid[0].mi.dwFlags = MOUSEEVENTF_XDOWN;
			}
			break;
		case 7:
			kid[0].mi.mouseData = XBUTTON2;
			if (i_kid->Flags & KEYBOARD_INPUT_DATA::BREAK) {
				kid[0].mi.dwFlags = MOUSEEVENTF_XUP;
			} else {
				kid[0].mi.dwFlags = MOUSEEVENTF_XDOWN;
			}
			break;
		case 8:
			if (i_kid->Flags & KEYBOARD_INPUT_DATA::BREAK) {
				return 1;
			} else {
				kid[0].mi.mouseData = WHEEL_DELTA;
				kid[0].mi.dwFlags = MOUSEEVENTF_HWHEEL;
			}
			break;
		case 9:
			if (i_kid->Flags & KEYBOARD_INPUT_DATA::BREAK) {
				return 1;
			} else {
				kid[0].mi.mouseData = static_cast<DWORD>(- WHEEL_DELTA);
				kid[0].mi.dwFlags = MOUSEEVENTF_HWHEEL;
			}
			break;
		default:
			return 1;
			break;
		}
		if (!(i_kid->Flags & KEYBOARD_INPUT_DATA::BREAK) &&
			i_kid->MakeCode != 4 && i_kid->MakeCode != 5 &&
			i_kid->MakeCode != 8 && i_kid->MakeCode != 9) {
			HWND hwnd;
			POINT pt;

			if (GetCursorPos(&pt) && (hwnd = WindowFromPoint(pt))) {
				wchar_t className[GANA_MAX_ATOM_LENGTH];
				if (GetClassName(hwnd, className, NUMBER_OF(className))) {
					if (_wcsicmp(className, L"ConsoleWindowClass") == 0) {
						SetForegroundWindow(hwnd);
					}
				}
			}
			if (m_dragging) {
				toVirtualDesktopAbsolute(m_msllHookCurrent.pt,
										 &kid[0].mi.dx, &kid[0].mi.dy);
				kid[0].mi.dwFlags |= MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;

				kid[1].type = INPUT_MOUSE;
				toVirtualDesktopAbsolute(pt, &kid[1].mi.dx, &kid[1].mi.dy);
				kid[1].mi.time = 0;
				kid[1].mi.mouseData = 0;
				kid[1].mi.dwExtraInfo = 0;
				kid[1].mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;

				count = 2;
			}
		}
		SendInput(count, &kid[0], sizeof(kid[0]));
	} else {
		INPUT kid;

		kid.type = INPUT_KEYBOARD;
		kid.ki.wVk = 0;
		kid.ki.wScan = i_kid->MakeCode;
		kid.ki.dwFlags = KEYEVENTF_SCANCODE;
		kid.ki.time = i_kidRaw ? i_kidRaw->time : 0;
		kid.ki.dwExtraInfo = i_kidRaw ? i_kidRaw->dwExtraInfo : 0;
		if (i_kid->Flags & KEYBOARD_INPUT_DATA::BREAK) {
			kid.ki.dwFlags |= KEYEVENTF_KEYUP;
		}
		if (i_kid->Flags & KEYBOARD_INPUT_DATA::E0) {
			kid.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
		}
		SendInput(1, &kid, sizeof(kid));
	}
	return 1;
}


// pop all pressed key on win32
void Engine::keyboardResetOnWin32()
{
	auto s = m_setting.load(std::memory_order_relaxed);
	for (Keyboard::KeyIterator
			i = s->m_keyboard.getKeyIterator();  *i; ++ i) {
		if ((*i)->m_isPressedOnWin32)
			generateKeyEvent((*i), false, true);
	}
}


// release modifiers and reset counters when no key is pressed.
// Modifier presses are generated inline per action, but their release
// is centralized here; every path that generates key events must reach
// this after processing (see keyboardHandler).
void Engine::resetModifiersIfIdle()
{
	if (m_currentKeyPressCount <= 0) {
		{
			Acquire a(&m_log, LogLevel::Debug);
			logIndent(m_logIndent);
			m_log << L"*   No key is pressed" << std::endl;
		}
		generateModifierEvents(Modifier());
		if (0 < m_currentKeyPressCountOnWin32)
			keyboardResetOnWin32();
		m_currentKeyPressCount = 0;
		m_currentKeyPressCountOnWin32 = 0;
		m_oneShotKey.m_key = NULL;
	}
}


// convert a single scan code to a virtual key code for GetAsyncKeyState.
// returns 0 if the scan code has no reliable VK mapping.
static USHORT scanCodeToVKey(const ScanCode &i_sc)
{
	if (i_sc.m_flags & ScanCode::E1)
		return 0;
	bool isE0 = !!(i_sc.m_flags & ScanCode::E0);
	// fixed table for modifier keys; MAPVK_VSC_TO_VK_EX support for
	// E0-prefixed scan codes varies between Windows versions
	switch (i_sc.m_scan) {
	case 0x2a:
		return isE0 ? 0 : VK_LSHIFT;	// E0 0x2a is a fake shift prefix
	case 0x36:
		return isE0 ? 0 : VK_RSHIFT;
	case 0x1d:
		return isE0 ? VK_RCONTROL : VK_LCONTROL;
	case 0x38:
		return isE0 ? VK_RMENU : VK_LMENU;
	case 0x5b:
		return isE0 ? VK_LWIN : 0;
	case 0x5c:
		return isE0 ? VK_RWIN : 0;
	}
	UINT scan = i_sc.m_scan | (isE0 ? 0xe000 : 0);
	return static_cast<USHORT>(MapVirtualKey(scan, MAPVK_VSC_TO_VK_EX));
}


// drop pressed-key marks that no longer match the OS key state.
// The low-level hook consumes physical events and re-injects them, so
// GetAsyncKeyState() mirrors what this engine has output: a key marked
// m_isPressedOnWin32 but reported up by the OS means its release
// bypassed the hook (secure desktop, elevated foreground window).
// Keys the engine did not echo (true modifiers, substituted keys) are
// unverifiable and dropped only when i_force is true (session unlock).
void Engine::resyncKeyStates(bool i_force)
{
	Lock lock(this);
	auto s = m_setting.load(std::memory_order_acquire);
	if (!s)
		return;
	// a stale pressed mark always keeps the count above zero
	if (m_currentKeyPressCount <= 0 && !i_force)
		return;

	bool dropped = false;
	for (Keyboard::KeyIterator i = s->m_keyboard.getKeyIterator(); *i; ++ i) {
		Key *key = *i;
		if (!key->m_isPressed)
			continue;
		USHORT vkey = 0;
		if (key->m_isPressedOnWin32 && key->getScanCodesSize() == 1)
			vkey = scanCodeToVKey(key->getScanCodes()[0]);
		bool stale = vkey
					 ? !(GetAsyncKeyState(vkey) & 0x8000)
					 : i_force;
		if (stale) {
			{
				Acquire a(&m_log, LogLevel::Debug);
				logIndent(m_logIndent);
				m_log << L"*   resync: drop stale key " << *key << std::endl;
			}
			key->m_isPressed = false;
			-- m_currentKeyPressCount;
			dropped = true;
		}
	}
	if (dropped)
		resetModifiersIfIdle();
}


// park the engine thread with m_mutex released.  See the header for why the
// mutex has to go and what stays safe while it is gone.
Engine::WaitResult Engine::waitWhileUnlocked(HANDLE i_event, DWORD i_timeout)
{
	HANDLE handles[2];
	DWORD count = 0;
	if (i_event)
		handles[count ++] = i_event;
	const DWORD shutdownIndex = count;
	handles[count ++] = m_eShutdown;

	DWORD r;
	{
		ScopedUnlock unlock(this);
		r = WaitForMultipleObjects(count, handles, FALSE, i_timeout);
	}

	if (r == WAIT_OBJECT_0 + shutdownIndex) {
		// stop unwinding into more key generation; the caller returns and
		// keyboardHandler() leaves its loop through the m_isStopping check
		m_isAborting = true;
		return WaitResult::Aborted;
	}
	if (i_event && r == WAIT_OBJECT_0)
		return WaitResult::Signaled;
	// WAIT_TIMEOUT, or a failed wait: keep the historical behaviour of
	// carrying on with the rest of the key sequence
	return WaitResult::Timeout;
}


unsigned int WINAPI Engine::keyboardDetour(Engine *i_this, WPARAM i_wParam, LPARAM i_lParam)
{
	return i_this->keyboardDetour(reinterpret_cast<KBDLLHOOKSTRUCT*>(i_lParam));
}

unsigned int Engine::keyboardDetour(KBDLLHOOKSTRUCT *i_kid)
{
#if 0
	Acquire a(&m_log, LogLevel::Debug);
	m_log << std::hex
	<< L"keyboardDetour: vkCode=" << i_kid->vkCode
	<< L" scanCode=" << i_kid->scanCode
	<< L" flags=" << i_kid->flags << std::endl;
#endif
	if ((i_kid->flags & LLKHF_INJECTED) || !m_isEnabled) {
		return 0;
	} else {
		Key key;
		KEYBOARD_INPUT_DATA kid;

		kid.UnitId = 0;
		kid.MakeCode = static_cast<USHORT>(i_kid->scanCode);
		kid.Flags = 0;
		if (i_kid->flags & LLKHF_UP) {
			kid.Flags |= KEYBOARD_INPUT_DATA::BREAK;
		}
		if (i_kid->flags & LLKHF_EXTENDED) {
			kid.Flags |= KEYBOARD_INPUT_DATA::E0;
		}
		kid.Reserved = 0;
		kid.ExtraInformation = 0;

		// An NLS key delivers a make but never a break: the keyboard layout
		// driver consumes the release before this hook runs (README-yamy.txt
		// 3.2), and it is unreachable through Raw Input or GetAsyncKeyState
		// too.  Left alone the key would stay pressed for the rest of the
		// session, which also stops resetModifiersIfIdle() from ever running
		// again.  Pair the make with a synthesized break, the way mouse wheel
		// events are paired in mouseDetour().
		auto s = m_setting.load(std::memory_order_acquire);
		bool needsBreak = s
						  && !(kid.Flags & KEYBOARD_INPUT_DATA::BREAK)
						  && s->isNlsKey(kid.MakeCode, kid.Flags);

		WaitForSingleObject(m_queueMutex, INFINITE);
		m_inputQueue->push_back(kid);
		if (needsBreak) {
			kid.Flags |= KEYBOARD_INPUT_DATA::BREAK;
			m_inputQueue->push_back(kid);
		}
		SetEvent(m_readEvent);
		ReleaseMutex(m_queueMutex);
		return 1;
	}
}

unsigned int WINAPI Engine::mouseDetour(Engine *i_this, WPARAM i_wParam, LPARAM i_lParam)
{
	return i_this->mouseDetour(i_wParam, reinterpret_cast<MSLLHOOKSTRUCT*>(i_lParam));
}

unsigned int Engine::mouseDetour(WPARAM i_message, MSLLHOOKSTRUCT *i_mid)
{
	auto s = m_setting.load(std::memory_order_acquire);
	if (i_mid->flags & LLMHF_INJECTED || !m_isEnabled || !s || !s->m_mouseEvent) {
		return 0;
	} else {
		KEYBOARD_INPUT_DATA kid;

		kid.UnitId = 0;
		kid.Flags = KEYBOARD_INPUT_DATA::E1;
		kid.Reserved = 0;
		kid.ExtraInformation = 0;
		switch (i_message) {
		case WM_LBUTTONUP:
			kid.Flags |= KEYBOARD_INPUT_DATA::BREAK;
			[[fallthrough]];
		case WM_LBUTTONDOWN:
			kid.MakeCode = 1;
			break;
		case WM_RBUTTONUP:
			kid.Flags |= KEYBOARD_INPUT_DATA::BREAK;
			[[fallthrough]];
		case WM_RBUTTONDOWN:
			kid.MakeCode = 2;
			break;
		case WM_MBUTTONUP:
			kid.Flags |= KEYBOARD_INPUT_DATA::BREAK;
			[[fallthrough]];
		case WM_MBUTTONDOWN:
			kid.MakeCode = 3;
			break;
		case WM_MOUSEWHEEL:
			if (i_mid->mouseData & (1<<31)) {
				kid.MakeCode = 5;
			} else {
				kid.MakeCode = 4;
			}
			break;
		case WM_XBUTTONUP:
			kid.Flags |= KEYBOARD_INPUT_DATA::BREAK;
			[[fallthrough]];
		case WM_XBUTTONDOWN:
			switch ((i_mid->mouseData >> 16) & 0xFFFFU) {
			case XBUTTON1:
				kid.MakeCode = 6;
				break;
			case XBUTTON2:
				kid.MakeCode = 7;
				break;
			default:
				return 0;
				break;
			}
			break;
		case WM_MOUSEHWHEEL:
			if (i_mid->mouseData & (1<<31)) {
				kid.MakeCode = 9;
			} else {
				kid.MakeCode = 8;
			}
			break;
		case WM_MOUSEMOVE: {
			LONG dx = i_mid->pt.x - g_hookData->m_mousePos.x;
			LONG dy = i_mid->pt.y - g_hookData->m_mousePos.y;
			HWND target = g_hookData->getHwndMouseHookTarget();

			LONG dr = 0;
			dr += (i_mid->pt.x - m_msllHookCurrent.pt.x) * (i_mid->pt.x - m_msllHookCurrent.pt.x);
			dr += (i_mid->pt.y - m_msllHookCurrent.pt.y) * (i_mid->pt.y - m_msllHookCurrent.pt.y);
			if (m_buttonPressed && !m_dragging && m_dragThresholdPx &&
				(m_dragThresholdPx * m_dragThresholdPx < dr)) {
				kid.MakeCode = 0;
				WaitForSingleObject(m_queueMutex, INFINITE);
				m_dragging = true;
				m_inputQueue->push_back(kid);
				SetEvent(m_readEvent);
				ReleaseMutex(m_queueMutex);
			}

			switch (g_hookData->m_mouseHookType) {
			case MouseHookType_Wheel:
				// For this type, g_hookData->m_mouseHookParam means
				// translate rate mouse move to wheel.
				mouse_event(MOUSEEVENTF_WHEEL, 0, 0,
							g_hookData->m_mouseHookParam * dy, 0);
				return 1;
				break;
			case MouseHookType_WindowMove: {
				RECT curRect;

				if (!GetWindowRect(target, &curRect))
					return 0;

				// g_hookData->m_mouseHookParam < 0 means
				// target window to move is MDI.
				if (g_hookData->m_mouseHookParam < 0) {
					HWND parent = GetParent(target);
					POINT p = {curRect.left, curRect.top};

					if (parent == NULL || !ScreenToClient(parent, &p))
						return 0;

					curRect.left = p.x;
					curRect.top = p.y;
				}

				SetWindowPos(target, NULL,
							 curRect.left + dx,
							 curRect.top + dy,
							 0, 0,
							 SWP_ASYNCWINDOWPOS | SWP_NOACTIVATE |
							 SWP_NOOWNERZORDER | SWP_NOSIZE | SWP_NOZORDER);
				g_hookData->m_mousePos = i_mid->pt;
				return 0;
				break;
			}
			case MouseHookType_None:
			default:
				return 0;
				break;
			}
		}
		case WM_LBUTTONDBLCLK:
		case WM_RBUTTONDBLCLK:
		case WM_MBUTTONDBLCLK:
		case WM_XBUTTONDBLCLK:
		default:
			return 0;
			break;
		}

		// Nothing slow belongs here.  This runs in the low level mouse hook,
		// which Windows skips for an event whose hook takes longer than
		// LowLevelHooksTimeout - and a skipped button release leaves the
		// engine believing the button is still down.  An earlier version
		// logged a coordinate comparison at this point, which meant taking the
		// log lock while the UI thread could be holding it, and produced
		// exactly that: stuck buttons and modifiers shortly after startup,
		// when the log is busiest.

		WaitForSingleObject(m_queueMutex, INFINITE);

		if (kid.Flags & KEYBOARD_INPUT_DATA::BREAK) {
			m_buttonPressed = false;
			if (m_dragging) {
				KEYBOARD_INPUT_DATA kid2;

				m_dragging = false;
				kid2.UnitId = 0;
				kid2.Flags = KEYBOARD_INPUT_DATA::E1 | KEYBOARD_INPUT_DATA::BREAK;
				kid2.Reserved = 0;
				kid2.ExtraInformation = 0;
				kid2.MakeCode = 0;
				m_inputQueue->push_back(kid2);
			}
		} else if (i_message != WM_MOUSEWHEEL && i_message != WM_MOUSEHWHEEL) {
			m_buttonPressed = true;
			m_msllHookCurrent = *i_mid;
			// the config states the threshold in 96 dpi pixels, so that the
			// same setting means the same apparent distance on every monitor
			m_dragThresholdPx =
				scaleFromLogical(static_cast<int>(s->m_dragThreshold),
								 dpiForPoint(i_mid->pt));
		}

		m_inputQueue->push_back(kid);

		if (i_message == WM_MOUSEWHEEL || i_message == WM_MOUSEHWHEEL) {
			kid.UnitId = 0;
			kid.Flags |= KEYBOARD_INPUT_DATA::BREAK;
			kid.Reserved = 0;
			kid.ExtraInformation = 0;
			m_inputQueue->push_back(kid);
		}

		SetEvent(m_readEvent);
		ReleaseMutex(m_queueMutex);

		return 1;
	}
}

// keyboard handler thread
unsigned int WINAPI Engine::keyboardHandler(void *i_this)
{
	reinterpret_cast<Engine *>(i_this)->keyboardHandler();
	_endthreadex(0);
	return 0;
}
void Engine::keyboardHandler()
{
	// loop
	Key key;
	while (1) {
		InputEvent event;

		WaitForSingleObject(m_queueMutex, INFINITE);
		while (SignalObjectAndWait(m_queueMutex, m_readEvent, INFINITE, true) == WAIT_OBJECT_0) {
			if (m_isStopping) {
				ReleaseMutex(m_queueMutex);
				return;
			}

			if (m_inputQueue->empty()) {
				// check the flag only after ResetEvent; the reverse order
				// would let a request between the check and ResetEvent be
				// deferred until the next key event
				ResetEvent(m_readEvent);
				if (m_resyncForceRequested.exchange(false)) {
					ReleaseMutex(m_queueMutex);
					resyncKeyStates(true);
					WaitForSingleObject(m_queueMutex, INFINITE);
				}
				continue;
			}

			event = std::move(m_inputQueue->front());
			m_inputQueue->pop_front();
			if (m_inputQueue->empty()) {
				ResetEvent(m_readEvent);
			}

			break;

		}
		ReleaseMutex(m_queueMutex);

		// a resync request may be consumed here instead of in the empty
		// branch above when key events arrive together with the request
		if (m_resyncForceRequested.exchange(false))
			resyncKeyStates(true);

		checkFocusWindow();

		Lock lock(this);

		// Activate a Setting handed over by the scripter.  Done here so the
		// keymap changes at an event boundary, never in the middle of the
		// events that were queued before it.  Unlike the branches below this
		// one runs even while the engine is disabled: enabling it later has to
		// pick up the newest setting.
		if (std::holds_alternative<std::shared_ptr<Setting> >(event)) {
			applySetting(std::move(std::get<std::shared_ptr<Setting> >(event)));
			continue;
		}

		auto s = m_setting.load(std::memory_order_acquire);

		// Handle AdHocKeySeq via the same entry point as normal keys
		if (std::holds_alternative<AdHocKeySeq>(event)) {
			if (s && m_isEnabled) {
				auto &item = std::get<AdHocKeySeq>(event);
				if (item && item->keySeq) {
					if (item->origin != s) {
						// materialized against a Setting that has been
						// replaced by a reload; its Key* would dangle
						Acquire a(&m_log, LogLevel::Debug);
						m_log << L"*   ad-hoc key sequence discarded "
						L"(setting reloaded)" << std::endl;
						continue;
					}
					if (!m_currentFocusOfThread) continue;
					Current i_c = reconstructCurrentFromContext(item->context, s);
					i_c.m_adhocKeySeq = item->keySeq.get();
					i_c.m_mkey.m_modifier.on(Modifier::Type_Down);
					beginGeneratingKeyboardEvents(i_c, false);
					// modifiers pressed while generating the sequence are
					// released only by this reset; without it they would
					// remain pressed on Win32 (stuck modifier)
					resetModifiersIfIdle();
				}
			}
			continue;
		}

		// KEYBOARD_INPUT_DATA processing
		KEYBOARD_INPUT_DATA &kid = std::get<KEYBOARD_INPUT_DATA>(event);

		if (!s ||						// m_setting has not been loaded
				!m_isEnabled) {	// disabled
			if (m_isLogMode) {
				Key key2;
				key2.addScanCode(ScanCode(kid.MakeCode, kid.Flags));
				outputInputToLog(&key2, ModifiedKey(), LogLevel::Info);
				if (kid.Flags & KEYBOARD_INPUT_DATA::E1) {
					// through mouse event even if log mode
					injectInput(&kid, NULL);
				}
			} else {
				injectInput(&kid, NULL);
			}
			updateLastPressedKey(NULL);
			continue;
		}

		if (!m_currentFocusOfThread ||
				!m_currentKeymap) {
			injectInput(&kid, NULL);
			Acquire a(&m_log, LogLevel::Error);
			if (!m_currentFocusOfThread)
				m_log << L"internal error: m_currentFocusOfThread == NULL"
				<< std::endl;
			if (!m_currentKeymap)
				m_log << L"internal error: m_currentKeymap == NULL"
				<< std::endl;
			updateLastPressedKey(NULL);
			continue;
		}

		// drop stale pressed marks before evaluating modifiers so the
		// first key after returning from a secure desktop or an elevated
		// foreground window is interpreted with the real modifier state
		resyncKeyStates(false);

		Current c;
		c.m_keymap = m_currentKeymap;
		c.m_i = m_currentFocusOfThread->m_keymaps.begin();

		// search key
		key.addScanCode(ScanCode(kid.MakeCode, kid.Flags));
		c.m_mkey = s->m_keyboard.searchKey(key);
		if (!c.m_mkey.m_key) {
			c.m_mkey.m_key = s->m_keyboard.searchPrefixKey(key);
			if (c.m_mkey.m_key)
				continue;
		}

		// press the key and update counter
		bool isPhysicallyPressed
		= !(key.getScanCodes()[0].m_flags & ScanCode::BREAK);
		if (c.m_mkey.m_key) {
			if (!c.m_mkey.m_key->m_isPressed && isPhysicallyPressed)
				++ m_currentKeyPressCount;
			else if (c.m_mkey.m_key->m_isPressed && !isPhysicallyPressed)
				-- m_currentKeyPressCount;
			c.m_mkey.m_key->m_isPressed = isPhysicallyPressed;
		}

		// create modifiers
		c.m_mkey.m_modifier = getCurrentModifiers(c.m_mkey.m_key,
							  isPhysicallyPressed);
		Keymap::AssignMode am;
		bool isModifier = fixModifierKey(&c.m_mkey, &am);
		if (m_isPrefix) {
			if (isModifier && m_doesIgnoreModifierForPrefix)
				am = Keymap::AM_true;
			if (m_doesEditNextModifier) {
				Modifier modifier = m_modifierForNextKey;
				modifier.add(c.m_mkey.m_modifier);
				c.m_mkey.m_modifier = modifier;
			}
		}

		if (m_isLogMode) {
			outputInputToLog(&key, c.m_mkey, LogLevel::Info);
			if (kid.Flags & KEYBOARD_INPUT_DATA::E1) {
				// through mouse event even if log mode
				injectInput(&kid, NULL);
			}
		} else if (am == Keymap::AM_true) {
			// true modifier doesn't generate scan code
			outputInputToLog(&key, c.m_mkey, LogLevel::Debug);
			logNote(LogLevel::Debug, L"true modifier");
		} else if (am == Keymap::AM_oneShot || am == Keymap::AM_oneShotRepeatable) {
			// oneShot modifier doesn't generate scan code
			outputInputToLog(&key, c.m_mkey, LogLevel::Debug);
			logNote(LogLevel::Debug,
					(am == Keymap::AM_oneShot) ? L"one shot modifier"
					: L"one shot repeatable modifier");
			if (isPhysicallyPressed) {
				if (am == Keymap::AM_oneShotRepeatable	// the key is repeating
						&& m_oneShotKey.m_key == c.m_mkey.m_key) {
					if (m_oneShotRepeatableRepeatCount <
							s->m_oneShotRepeatableDelay) {
						; // delay
					} else {
						Current cnew = c;
						beginGeneratingKeyboardEvents(cnew, false);
					}
					++ m_oneShotRepeatableRepeatCount;
				} else {
					m_oneShotKey = c.m_mkey;
					m_oneShotRepeatableRepeatCount = 0;
				}
			} else {
				if (m_oneShotKey.m_key) {
					Current cnew = c;
					cnew.m_mkey.m_modifier = m_oneShotKey.m_modifier;
					cnew.m_mkey.m_modifier.off(Modifier::Type_Up);
					cnew.m_mkey.m_modifier.on(Modifier::Type_Down);
					beginGeneratingKeyboardEvents(cnew, false);

					cnew = c;
					cnew.m_mkey.m_modifier = m_oneShotKey.m_modifier;
					cnew.m_mkey.m_modifier.on(Modifier::Type_Up);
					cnew.m_mkey.m_modifier.off(Modifier::Type_Down);
					beginGeneratingKeyboardEvents(cnew, false);
				}
				m_oneShotKey.m_key = NULL;
				m_oneShotRepeatableRepeatCount = 0;
			}
		} else if (c.m_mkey.m_key) {
			// normal key
			outputInputToLog(&key, c.m_mkey, LogLevel::Debug);
			if (isPhysicallyPressed)
				m_oneShotKey.m_key = NULL;
			beginGeneratingKeyboardEvents(c, isModifier);
		} else {
			// undefined key
			if (kid.Flags & KEYBOARD_INPUT_DATA::E1) {
				// through mouse event even if undefined for fail safe
				injectInput(&kid, NULL);
			}
		}

		// if counter is zero, reset modifiers and keys on win32
		resetModifiersIfIdle();

		key.initialize();
		updateLastPressedKey(isPhysicallyPressed ? c.m_mkey.m_key : NULL);
	}
}


Engine::Engine(womsgstream &i_log)
		: m_mutexDepth(0),
		m_hwndAssocWindow(NULL),
		m_setting(std::shared_ptr<Setting>{}),
		m_buttonPressed(false),
		m_dragging(false),
		m_keyboardHandler(installKeyboardHook, Engine::keyboardDetour),
		m_mouseHandler(installMouseHook, Engine::mouseDetour),
		m_isStopping(false),
		m_readEvent(NULL),
		m_queueMutex(NULL),
		m_isLogMode(false),
		m_logIndent(0),
		m_isGeneratingModifiers(false),
		m_modifierHeaderWritten(false),
		m_isEnabled(true),
		m_isSynchronizing(false),
		m_isAborting(false),
		m_eSync(NULL),
		m_eShutdown(NULL),
		m_generateKeyboardEventsRecursionGuard(0),
		m_currentKeyPressCount(0),
		m_currentKeyPressCountOnWin32(0),
		m_resyncForceRequested(false),
		m_lastGeneratedKey(NULL),
		m_oneShotRepeatableRepeatCount(0),
		m_isPrefix(false),
		m_currentKeymap(NULL),
		m_currentFocusOfThread(NULL),
		m_hwndFocus(NULL),
		m_variable(0),
		m_log(i_log) {
#pragma warning(suppress: 6387)
	BOOL (WINAPI *pChangeWindowMessageFilter)(UINT, DWORD) =
		reinterpret_cast<BOOL (WINAPI*)(UINT, DWORD)>(GetProcAddress(GetModuleHandle(L"user32.dll"), "ChangeWindowMessageFilter"));

	if(pChangeWindowMessageFilter != NULL) {
		pChangeWindowMessageFilter(WM_COPYDATA, MSGFLT_ADD);
	}

	for (size_t i = 0; i < NUMBER_OF(m_lastPressedKey); ++ i)
		m_lastPressedKey[i] = NULL;

	// set default lock state
	for (int i = 0; i < Modifier::Type_end; ++ i)
		m_currentLock.dontcare(static_cast<Modifier::Type>(i));
	for (int i = Modifier::Type_Lock0; i <= Modifier::Type_Lock9; ++ i)
		m_currentLock.release(static_cast<Modifier::Type>(i));

	// create event for sync
	CHECK_TRUE( m_eSync = CreateEvent(NULL, FALSE, FALSE, NULL) );
	// manual reset: once shutdown starts every wait has to fail, not just one
	CHECK_TRUE( m_eShutdown = CreateEvent(NULL, TRUE, FALSE, NULL) );
	// create named pipe for &SetImeString
	m_hookPipe = CreateNamedPipe(addSessionId(HOOK_PIPE_NAME).c_str(),
								 PIPE_ACCESS_OUTBOUND,
								 PIPE_TYPE_BYTE, 1,
								 0, 0, 0, NULL);
	StrExprArg::setEngine(this);

	m_msllHookCurrent.pt.x = 0;
	m_msllHookCurrent.pt.y = 0;
	m_msllHookCurrent.mouseData = 0;
	m_msllHookCurrent.flags = 0;
	m_msllHookCurrent.time = 0;
	m_msllHookCurrent.dwExtraInfo = 0;
	m_dragThresholdPx = 0;
}




// start keyboard handler thread
void Engine::start() {
	m_keyboardHandler.start(this);
	m_mouseHandler.start(this);

	m_inputQueue = std::make_unique<std::deque<InputEvent>>();
	m_isStopping = false;
	m_isAborting = false;
	CHECK_TRUE( ResetEvent(m_eShutdown) );
	CHECK_TRUE( m_queueMutex = CreateMutex(NULL, FALSE, NULL) );
	CHECK_TRUE( m_readEvent = CreateEvent(NULL, TRUE, FALSE, NULL) );
	m_ol.Offset = 0;
	m_ol.OffsetHigh = 0;
	m_ol.hEvent = m_readEvent;

	CHECK_TRUE( m_threadHandle = (HANDLE)_beginthreadex(NULL, 0, keyboardHandler, this, 0, &m_threadId) );
}


// stop keyboard handler thread
// Phase B+C: stop InputHandlers in parallel, signal engine exit.
// Returns the engine thread handle; caller must WaitForSingleObject/
// WaitForMultipleObjects on it and then call cleanupAfterStop().
HANDLE Engine::signalStop() {
	// Phase B: send WM_QUIT to both InputHandlers simultaneously
	m_mouseHandler.postQuit();
	m_keyboardHandler.postQuit();
	HANDLE iHandles[2] = { m_mouseHandler.hThread(), m_keyboardHandler.hThread() };
	WaitForMultipleObjects(2, iHandles, TRUE, 3000);
	m_mouseHandler.closeThread();
	m_keyboardHandler.closeThread();

	// Phase C: signal engine thread to exit (safe now that hooks are stopped).
	// The queue itself stays alive: the scripter's data thread can still be
	// running and pushing into it through scheduleAdHocKeySeq().  Destroying it
	// here is what used to make that a null dereference.  cleanupAfterStop()
	// destroys it instead, and the caller only gets there once the reader
	// threads are confirmed stopped.
	// Break a &Sync or &Wait the engine thread may be parked in.  Without
	// this it can sit there for up to 5 s - and during shutdown it always
	// does, because uninstallMessageHook() has already stopped the sync
	// notification from ever arriving - which is longer than the caller
	// waits before cleanupAfterStop() frees the input queue underneath it.
	CHECK_TRUE( SetEvent(m_eShutdown) );

	WaitForSingleObject(m_queueMutex, INFINITE);
	m_isStopping = true;
	SetEvent(m_readEvent);
	ReleaseMutex(m_queueMutex);

	// Return engine thread handle for external WaitForMultipleObjects
	HANDLE h = m_threadHandle;
	m_threadHandle = NULL;
	return h;
}

// Call after the engine thread handle returned by signalStop() has been waited
// on AND every producer of the input queue has stopped - which for the
// scripter's data thread means ScripterManager::forceStop() has returned.  Both
// the queue and m_queueMutex go away here, so a producer still running would
// dereference freed memory.
void Engine::cleanupAfterStop(HANDLE hEngineThread) {
	CloseHandle(hEngineThread);
	m_inputQueue.reset();
	CHECK_TRUE( CloseHandle(m_readEvent) );
	m_readEvent = NULL;
	CloseHandle(m_queueMutex);
	m_queueMutex = NULL;
	for (ThreadIds::iterator i = m_attachedThreadIds.begin();
		 i != m_attachedThreadIds.end(); i++) {
		 PostThreadMessage(*i, WM_NULL, 0, 0);
	}
}

void Engine::stop() {
	HANDLE h = signalStop();
	WaitForSingleObject(h, 2000);
	cleanupAfterStop(h);
}


bool Engine::prepairQuit() {
	return true;
}


Engine::~Engine() {
	CHECK_TRUE( CloseHandle(m_eSync) );
	CHECK_TRUE( CloseHandle(m_eShutdown) );

	// destroy named pipe for &SetImeString
	if (m_hookPipe && m_hookPipe != INVALID_HANDLE_VALUE) {
		DisconnectNamedPipe(m_hookPipe);
		CHECK_TRUE( CloseHandle(m_hookPipe) );
	}
}


// activate a Setting.  Engine thread only: called from keyboardHandler() at an
// event boundary, where m_isSynchronizing is guaranteed to be false, because
// &Sync and &Wait set and clear that flag within the processing of a single
// event.  So unlike setFocus()/setLockState()/setShow() this needs no
// synchronizing guard and cannot fail.
void Engine::applySetting(std::shared_ptr<Setting> newSetting) {
	Lock lock(this);

	Setting *raw = newSetting.get();

	auto old = m_setting.load(std::memory_order_relaxed);
	if (old) {
		for (Keyboard::KeyIterator i = old->m_keyboard.getKeyIterator();
				*i; ++ i) {
			Key *key = raw->m_keyboard.searchKey(*(*i));
			if (key) {
				key->m_isPressed = (*i)->m_isPressed;
				key->m_isPressedOnWin32 = (*i)->m_isPressedOnWin32;
				key->m_isPressedByAssign = (*i)->m_isPressedByAssign;
			}
		}
		if (m_lastGeneratedKey)
			m_lastGeneratedKey =
				raw->m_keyboard.searchKey(*m_lastGeneratedKey);
		for (size_t i = 0; i < NUMBER_OF(m_lastPressedKey); ++ i)
			if (m_lastPressedKey[i])
				m_lastPressedKey[i] =
					raw->m_keyboard.searchKey(*m_lastPressedKey[i]);
		if (m_oneShotKey.m_key)
			m_oneShotKey.m_key =
				raw->m_keyboard.searchKey(*m_oneShotKey.m_key);
	}

	m_setting.store(std::move(newSetting), std::memory_order_release);

	g_hookData->m_correctKanaLockHandling = raw->m_correctKanaLockHandling;
	if (m_currentFocusOfThread) {
		for (FocusOfThreads::iterator i = m_focusOfThreads.begin();
				i != m_focusOfThreads.end(); i ++) {
			FocusOfThread *fot = &(*i).second;
			raw->m_keymaps.searchWindow(&fot->m_keymaps,
										fot->m_className, fot->m_titleName);
		}
	}
	raw->m_keymaps.searchWindow(&m_globalFocus.m_keymaps, L"", L"");
	if (m_globalFocus.m_keymaps.empty()) {
		Acquire a(&m_log, LogLevel::Error);
		m_log << L"internal error: m_globalFocus.m_keymap is empty"
		<< std::endl;
	}
	m_currentFocusOfThread = &m_globalFocus;
	setCurrentKeymap(m_globalFocus.m_keymaps.front());
	m_hwndFocus = NULL;

	Acquire a(&m_log, LogLevel::Info);
	m_log << L"successfully loaded (scripter)." << std::endl;
}


void Engine::scheduleSetting(std::shared_ptr<Setting> i_setting) {
	// Called from the UI thread.  The Setting is activated by the engine
	// thread at an event boundary, which an outstanding &Sync or &Wait can
	// push back by as much as their timeout; the caller must not wait for
	// that, or the tasktray and every dialog freeze along with it.
	WaitForSingleObject(m_queueMutex, INFINITE);
	m_inputQueue->push_back(std::move(i_setting));
	SetEvent(m_readEvent);
	ReleaseMutex(m_queueMutex);
}


void Engine::scheduleAdHocKeySeq(AdHocKeySeq item) {
	WaitForSingleObject(m_queueMutex, INFINITE);
	m_inputQueue->push_back(std::move(item));
	SetEvent(m_readEvent);
	ReleaseMutex(m_queueMutex);
}


void Engine::callExecUserFuncCallback(const wstringi &name,
                                      const std::vector<FuncArg> &args,
                                      const TriggerInfo &ctx)
{
	if (m_execUserFuncCallback) m_execUserFuncCallback(name, args, ctx);
}


void Engine::setExecUserFuncCallback(ExecUserFuncCallback callback)
{
	m_execUserFuncCallback = std::move(callback);
}


Engine::Current Engine::reconstructCurrentFromContext(const TriggerInfo &ctx,
                                                      const std::shared_ptr<Setting> &s)
{
	Current c;
	c.m_keymap = m_currentKeymap;
	// m_i must point into m_currentFocusOfThread->m_keymaps (not a local copy),
	// because funcKeymapParent compares it against that list's end().
	c.m_i = m_currentFocusOfThread->m_keymaps.begin();
	if (ctx.scanCode != 0) {
		Key key;
		key.addScanCode(ScanCode(ctx.scanCode, ctx.extended ? ScanCode::E0 : 0));
		c.m_mkey = s->m_keyboard.searchKey(key);
	}
	if (!ctx.windowClass.empty() || !ctx.windowTitle.empty()) {
		KeymapPtrList keymaps;
		s->m_keymaps.searchWindow(&keymaps, ctx.windowClass, ctx.windowTitle);
		if (!keymaps.empty())
			c.m_keymap = keymaps.front();
		// NOTE: do NOT set c.m_i from local 'keymaps' -- it would be a
		// dangling iterator after the list is destroyed on return.
	}
	return c;
}

void Engine::unlocked()
{
	// store the flag before SetEvent so the handler thread, which
	// re-checks the flag after ResetEvent, cannot miss the request
	m_resyncForceRequested.store(true);
	SetEvent(m_readEvent);
}

void Engine::checkShow(HWND i_hwnd) {
	// update show style of window
	// this update should be done in hook DLL, but to
	// avoid update-loss for some applications(such as
	// cmd.exe), we update here.
	bool isMaximized = false;
	bool isMinimized = false;
	bool isMDIMaximized = false;
	bool isMDIMinimized = false;
	while (i_hwnd) {
		LONG_PTR exStyle = GetWindowLongPtr(i_hwnd, GWL_EXSTYLE);
		if (exStyle & WS_EX_MDICHILD) {
			WINDOWPLACEMENT placement;
			placement.length = sizeof(WINDOWPLACEMENT);
			if (GetWindowPlacement(i_hwnd, &placement)) {
				switch (placement.showCmd) {
				case SW_SHOWMAXIMIZED:
					isMDIMaximized = true;
					break;
				case SW_SHOWMINIMIZED:
					isMDIMinimized = true;
					break;
				case SW_SHOWNORMAL:
				default:
					break;
				}
			}
		}

		LONG_PTR style = GetWindowLongPtr(i_hwnd, GWL_STYLE);
		if ((style & WS_CHILD) == 0) {
			WINDOWPLACEMENT placement;
			placement.length = sizeof(WINDOWPLACEMENT);
			if (GetWindowPlacement(i_hwnd, &placement)) {
				switch (placement.showCmd) {
				case SW_SHOWMAXIMIZED:
					isMaximized = true;
					break;
				case SW_SHOWMINIMIZED:
					isMinimized = true;
					break;
				case SW_SHOWNORMAL:
				default:
					break;
				}
			}
		}
		i_hwnd = GetParent(i_hwnd);
	}
	setShow(isMDIMaximized, isMDIMinimized, true);
	setShow(isMaximized, isMinimized, false);
}


// focus
bool Engine::setFocus(HWND i_hwndFocus, DWORD i_threadId,
					  const wstringi &i_className, const wstringi &i_titleName,
					  bool i_isConsole) {
	Lock lock(this);
	if (m_isSynchronizing)
		return false;
	// A thread reporting no focus window teaches us nothing about it, so it
	// stays unregistered - and an unregistered foreground thread is what makes
	// checkFocusWindow() fall back to the global focus, which is otherwise hard
	// to account for from the log alone.  notifySetFocus() no longer sends
	// these, so one arriving means it came from somewhere else.
	if (i_hwndFocus == NULL) {
		Acquire a(&m_log, LogLevel::Debug);
		m_log << L"NoFocusWindow: THREADID: " << i_threadId << std::endl;
		return true;
	}

	// remove newly created thread's id from m_detachedThreadIds
	if (!m_detachedThreadIds.empty()) {
		ThreadIds::iterator i;
		bool retry;
		do {
			retry = false;
			for (i = m_detachedThreadIds.begin();
					i != m_detachedThreadIds.end(); ++ i)
				if (*i == i_threadId) {
					m_detachedThreadIds.erase(i);
					retry = true;
					break;
				}
		} while (retry);
	}

	FocusOfThread *fot;
	FocusOfThreads::iterator i = m_focusOfThreads.find(i_threadId);
	if (i != m_focusOfThreads.end()) {
		fot = &(*i).second;
		if (fot->m_hwndFocus == i_hwndFocus &&
				fot->m_isConsole == i_isConsole &&
				fot->m_className == i_className &&
				fot->m_titleName == i_titleName)
			return true;
	} else {
		i = m_focusOfThreads.insert(
				FocusOfThreads::value_type(i_threadId, FocusOfThread())).first;
		fot = &(*i).second;
		fot->m_threadId = i_threadId;
	}
	fot->m_hwndFocus = i_hwndFocus;
	fot->m_isConsole = i_isConsole;
	fot->m_className = i_className;
	fot->m_titleName = i_titleName;

	if (auto s = m_setting.load(std::memory_order_relaxed)) {
		s->m_keymaps.searchWindow(&fot->m_keymaps,
										  i_className, i_titleName);
		ASSERT(0 < fot->m_keymaps.size());
	} else
		fot->m_keymaps.clear();
	checkShow(i_hwndFocus);
	return true;
}


// lock state
bool Engine::setLockState(bool i_isNumLockToggled,
						  bool i_isCapsLockToggled,
						  bool i_isScrollLockToggled,
						  bool i_isKanaLockToggled,
						  bool i_isImeLockToggled,
						  bool i_isImeCompToggled) {
	Lock lock(this);
	if (m_isSynchronizing)
		return false;
	m_currentLock.on(Modifier::Type_NumLock, i_isNumLockToggled);
	m_currentLock.on(Modifier::Type_CapsLock, i_isCapsLockToggled);
	m_currentLock.on(Modifier::Type_ScrollLock, i_isScrollLockToggled);
	m_currentLock.on(Modifier::Type_KanaLock, i_isKanaLockToggled);
	m_currentLock.on(Modifier::Type_ImeLock, i_isImeLockToggled);
	m_currentLock.on(Modifier::Type_ImeComp, i_isImeCompToggled);
	return true;
}


// show
bool Engine::setShow(bool i_isMaximized, bool i_isMinimized,
					 bool i_isMDI) {
	Lock lock(this);
	if (m_isSynchronizing)
		return false;
	Acquire b(&m_log, LogLevel::Debug);
	Modifier::Type max, min;
	if (i_isMDI == true) {
		max = Modifier::Type_MdiMaximized;
		min = Modifier::Type_MdiMinimized;
	} else {
		max = Modifier::Type_Maximized;
		min = Modifier::Type_Minimized;
	}
	m_currentLock.on(max, i_isMaximized);
	m_currentLock.on(min, i_isMinimized);
	m_log << L"Set show to " << (i_isMaximized ? L"Maximized" :
									i_isMinimized ? L"Minimized" : L"Normal");
	if (i_isMDI == true) {
		m_log << L" (MDI)";
	}
	m_log << std::endl;
	return true;
}


// sync
bool Engine::syncNotify() {
	Lock lock(this);
	if (!m_isSynchronizing)
		return false;
	CHECK_TRUE( SetEvent(m_eSync) );
	return true;
}


// thread attach notify
bool Engine::threadAttachNotify(DWORD i_threadId) {
	Lock lock(this);
	m_attachedThreadIds.push_back(i_threadId);
	return true;
}


// thread detach notify
bool Engine::threadDetachNotify(DWORD i_threadId) {
	Lock lock(this);
	m_detachedThreadIds.push_back(i_threadId);
	m_attachedThreadIds.erase(remove(m_attachedThreadIds.begin(), m_attachedThreadIds.end(), i_threadId),
							  m_attachedThreadIds.end());
	return true;
}


// get help message
void Engine::getHelpMessages(std::wstring *o_helpMessage, std::wstring *o_helpTitle) {
	Lock lock(this);
	*o_helpMessage = m_helpMessage;
	*o_helpTitle = m_helpTitle;
}


unsigned int WINAPI Engine::InputHandler::run(void *i_this)
{
	reinterpret_cast<InputHandler*>(i_this)->run();
	_endthreadex(0);
	return 0;
}

Engine::InputHandler::InputHandler(INSTALL_HOOK i_installHook, INPUT_DETOUR i_inputDetour)
	: m_installHook(i_installHook), m_inputDetour(i_inputDetour)
{
	CHECK_TRUE(m_hEvent = CreateEvent(NULL, FALSE, FALSE, NULL));
	CHECK_TRUE(m_hThread = (HANDLE)_beginthreadex(NULL, 0, run, this, CREATE_SUSPENDED, &m_threadId));
}

Engine::InputHandler::~InputHandler()
{
	CloseHandle(m_hEvent);
}

void Engine::InputHandler::run()
{
	MSG msg;

	CHECK_FALSE(m_installHook(m_inputDetour, m_engine, true));
	PeekMessage(&msg, NULL, WM_USER, WM_USER, PM_NOREMOVE);
	SetEvent(m_hEvent);

	while (GetMessage(&msg, NULL, 0, 0)) {
		// nothing to do...
	}

	CHECK_FALSE(m_installHook(m_inputDetour, m_engine, false));

	return;
}

int Engine::InputHandler::start(Engine *i_engine)
{
	m_engine = i_engine;
	ResumeThread(m_hThread);
	WaitForSingleObject(m_hEvent, INFINITE);
	return 0;
}

int Engine::InputHandler::stop()
{
	postQuit();
	WaitForSingleObject(m_hThread, 3000);
	closeThread();
	return 0;
}

void Engine::InputHandler::postQuit()
{
	PostThreadMessage(m_threadId, WM_QUIT, 0, 0);
}

void Engine::InputHandler::closeThread()
{
	CloseHandle(m_hThread);
	m_hThread = NULL;
}
