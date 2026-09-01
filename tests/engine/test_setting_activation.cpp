//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// test_setting_activation.cpp
//
// Engine::applySetting() and the keymap lists it is responsible for.


#include "engine_test.h"


namespace
{

/// a window handle the tests can hand to setFocus(); never used as a window
HWND fakeHwnd(uintptr_t i_value)
{
	return reinterpret_cast<HWND>(i_value);
}


/** A thread that reported its focus before the first Setting existed is
    registered with an empty keymap list, and setFocus() does not come back to
    it while the window has not changed.  Activating a Setting has to fill it.

    This used to be skipped whenever m_currentFocusOfThread was still NULL -
    which is its state until the engine thread has seen its first event, so
    every startup where the scripter committed before the first keystroke left
    those threads without a keymap for the rest of the session. */
void keymapsAreFilledForThreadsRegisteredBeforeTheFirstSetting()
{
	womsgstream log(WM_APP);
	Engine engine(log);

	// the window between installMessageHook() and the scripter's first commit
	engine.setFocus(fakeHwnd(0x1234), 4242, L"Foo", L"Bar", false);
	ENGINE_CHECK(EngineTestAccess::focusThreadCount(&engine) == 1,
				 "the thread is registered");
	ENGINE_CHECK(EngineTestAccess::keymapCount(&engine, 4242) == 0,
				 "registered with no keymaps while there is no Setting");
	ENGINE_CHECK(!EngineTestAccess::hasCurrentFocus(&engine),
				 "no current focus before the first event");

	EngineTestAccess::applySetting(&engine, makeGlobalSetting());

	ENGINE_CHECK(EngineTestAccess::keymapCount(&engine, 4242) == 1,
				 "applySetting fills the keymaps of a registered thread");
}


/// A thread registered while a Setting is in force gets its keymaps there and
/// then, and activating the next Setting keeps them.
void keymapsSurviveAReload()
{
	womsgstream log(WM_APP);
	Engine engine(log);

	EngineTestAccess::applySetting(&engine, makeGlobalSetting());
	engine.setFocus(fakeHwnd(0x1234), 4242, L"Foo", L"Bar", false);
	ENGINE_CHECK(EngineTestAccess::keymapCount(&engine, 4242) == 1,
				 "setFocus fills the keymaps when a Setting is in force");

	EngineTestAccess::applySetting(&engine, makeGlobalSetting());
	ENGINE_CHECK(EngineTestAccess::keymapCount(&engine, 4242) == 1,
				 "the keymaps point into the new Setting after a reload");
}


/// The activation line says the engine did it, and carries enough to tell a
/// setting that lost its keyboard definition from one that did not.
void activationIsReported()
{
	womsgstream log(WM_APP);
	Engine engine(log);
	takeLog(&log);

	EngineTestAccess::applySetting(&engine, makeGlobalSetting());

	std::wstring text = takeLog(&log);
	ENGINE_CHECK(logHas(text, L"setting activated:"), "activation is reported");
	ENGINE_CHECK(logHas(text, L"1 keymaps"), "the keymap count is reported");
	ENGINE_CHECK(logHas(text, L"0 keys"), "the key count is reported");
	ENGINE_CHECK(!logHas(text, L"successfully loaded"),
				 "the wording no longer reads like the scripter's own");
}


/// In detail mode the state right after activation is reported too, including
/// how many threads had been left without a keymap.
void activationReportsTheThreadsItFilled()
{
	womsgstream log(WM_APP);
	log.setThreshold(LogLevel::Debug);
	Engine engine(log);

	engine.setFocus(fakeHwnd(0x1234), 4242, L"Foo", L"Bar", false);
	engine.setFocus(fakeHwnd(0x5678), 4243, L"Foo", L"Bar", false);
	takeLog(&log);

	EngineTestAccess::applySetting(&engine, makeGlobalSetting());

	std::wstring text = takeLog(&log);
	ENGINE_CHECK(logHas(text, L"global keymaps: 1"),
				 "the global keymap count is reported");
	ENGINE_CHECK(logHas(text, L"focus threads: 2"),
				 "the number of known threads is reported");
	ENGINE_CHECK(logHas(text, L"(2 had no keymap)"),
				 "the number of threads that had none is reported");
}


/** A Setting no window matches leaves the global focus with an empty keymap
    list.  The loader cannot produce one, but the engine used to read front()
    off that list anyway - handing back something that is not NULL and passes
    the guard in keyboardHandler(). */
void anEmptyGlobalFocusLeavesNoCurrentKeymap()
{
	womsgstream log(WM_APP);
	Engine engine(log);
	takeLog(&log);

	EngineTestAccess::applySetting(&engine, makeUnmatchableSetting());

	ENGINE_CHECK(EngineTestAccess::globalKeymapCount(&engine) == 0,
				 "no keymap matches the global focus");
	ENGINE_CHECK(EngineTestAccess::currentKeymap(&engine) == NULL,
				 "the current keymap is NULL rather than front() of nothing");
	ENGINE_CHECK(!EngineTestAccess::hasCurrentFocus(&engine),
				 "the current focus is cleared with it");
	ENGINE_CHECK(logHas(takeLog(&log), L"m_globalFocus.m_keymaps is empty"),
				 "and it is reported as the internal error it is");
}


/// ensureKeymaps() heals a list that was left empty, and keeps quiet about the
/// ones that are already filled.
void ensureKeymapsHealsAnEmptyList()
{
	womsgstream log(WM_APP);
	Engine engine(log);
	std::shared_ptr<Setting> setting = makeGlobalSetting();

	engine.setFocus(fakeHwnd(0x1234), 4242, L"Foo", L"Bar", false);
	ENGINE_CHECK(EngineTestAccess::keymapCount(&engine, 4242) == 0,
				 "the list starts out empty");
	takeLog(&log);

	ENGINE_CHECK(EngineTestAccess::ensureKeymaps(&engine, 4242, setting),
				 "an empty list is filled");
	ENGINE_CHECK(EngineTestAccess::keymapCount(&engine, 4242) == 1,
				 "and now has the keymap that matches");
	ENGINE_CHECK(logHas(takeLog(&log), L"recovered the keymaps of thread 4242"),
				 "healing is reported, not done behind the log's back");

	ENGINE_CHECK(!EngineTestAccess::ensureKeymaps(&engine, 4242, setting),
				 "a filled list is left alone");
	ENGINE_CHECK(takeLog(&log).empty(), "and says nothing");
}


/// Without a Setting there is nothing to fill a list from.
void ensureKeymapsDoesNothingWithoutASetting()
{
	womsgstream log(WM_APP);
	Engine engine(log);

	engine.setFocus(fakeHwnd(0x1234), 4242, L"Foo", L"Bar", false);
	ENGINE_CHECK(!EngineTestAccess::ensureKeymaps(&engine, 4242,
												 std::shared_ptr<Setting>()),
				 "no Setting, nothing to do");
	ENGINE_CHECK(EngineTestAccess::keymapCount(&engine, 4242) == 0,
				 "the list is still empty");
}

} // namespace


void runSettingActivationTests()
{
	keymapsAreFilledForThreadsRegisteredBeforeTheFirstSetting();
	keymapsSurviveAReload();
	activationIsReported();
	activationReportsTheThreadsItFilled();
	anEmptyGlobalFocusLeavesNoCurrentKeymap();
	ensureKeymapsHealsAnEmptyList();
	ensureKeymapsDoesNothingWithoutASetting();
}
