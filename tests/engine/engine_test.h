//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// engine_test.h
//
// Minimal checking for the engine-side unit tests (nyamy-engine-tests.vcxproj).
//
// These drive Engine's own entry points rather than going in through
// checkFocusWindow(): that one starts from GetForegroundWindow(), which a test
// cannot choose, and reaches seven more window queries from there.  What is
// left is free of Win32 state and is where the behaviour being checked lives.
// EngineTestAccess is the friend that opens the door; see engine.h.


#ifndef _ENGINE_TEST_H
#  define _ENGINE_TEST_H

#  include "engine.h"
#  include "setting.h"
#  include <cstdio>
#  include <memory>
#  include <string>


/// failures so far, across every test file
extern int g_engineTestFailures;

/// report a failure with its source location
void engineTestFail(const char *i_file, int i_line, const char *i_what);

/// record that a check was made, whether or not it passed
void engineTestCounted();

/// how many checks have run
int engineTestCount();

///
#  define ENGINE_CHECK(cond, what)					\
	do {								\
		engineTestCounted();					\
		if (!(cond))						\
			engineTestFail(__FILE__, __LINE__, (what));	\
	} while (0)


/// A Setting with one keymap that matches every window, the way the loader
/// opens a definition block.
std::shared_ptr<Setting> makeGlobalSetting();

/// A Setting whose only keymap matches no window at all, so that searching for
/// the global focus comes back empty.
std::shared_ptr<Setting> makeUnmatchableSetting();

/// everything written to the log since the last call
std::wstring takeLog(womsgstream *io_log);

///
bool logHas(const std::wstring &i_log, const wchar_t *i_text);


/** Reach into Engine.  Declared a friend by engine.h, which is what makes the
    members below legal; it changes nothing about what is generated for the
    shipped build. */
class EngineTestAccess
{
public:
	using StickyNotice = Engine::StickyNotice;

	/// activate a Setting, as the engine thread does at an event boundary
	static void applySetting(Engine *io_engine, std::shared_ptr<Setting> i_setting) {
		io_engine->applySetting(std::move(i_setting));
	}

	/// fill the keymaps of a registered thread if it has none
	static bool ensureKeymaps(Engine *io_engine, DWORD i_threadId,
							  const std::shared_ptr<Setting> &i_setting) {
		Engine::FocusOfThread *fot = findFocus(io_engine, i_threadId);
		return fot ? io_engine->ensureKeymaps(fot, i_setting) : false;
	}

	///
	static bool canRunAdHocKeySeq(Engine *io_engine, const AdHocKeySeq &i_item,
								  const std::shared_ptr<Setting> &i_setting,
								  const wchar_t **o_reason) {
		return io_engine->canRunAdHocKeySeq(i_item, i_setting, o_reason);
	}

	///
	static void reportSticky(Engine *io_engine, StickyNotice *io_notice,
							 bool i_isActive, LogLevel i_level,
							 const wchar_t *i_message,
							 bool i_doesReportRecovery = true) {
		io_engine->reportSticky(io_notice, i_isActive, i_level, i_message,
								i_doesReportRecovery);
	}

	/// how many keymaps a registered thread has; 0 when it is not registered
	static size_t keymapCount(Engine *io_engine, DWORD i_threadId) {
		Engine::FocusOfThread *fot = findFocus(io_engine, i_threadId);
		return fot ? fot->m_keymaps.size() : 0;
	}

	///
	static size_t focusThreadCount(Engine *io_engine) {
		return io_engine->m_focusOfThreads.size();
	}

	///
	static size_t globalKeymapCount(Engine *io_engine) {
		return io_engine->m_globalFocus.m_keymaps.size();
	}

	///
	static const Keymap *currentKeymap(Engine *io_engine) {
		return const_cast<const Keymap *>(io_engine->m_currentKeymap);
	}

	///
	static bool hasCurrentFocus(Engine *io_engine) {
		return io_engine->m_currentFocusOfThread != NULL;
	}

	/// Make a registered thread the current focus, as checkFocusWindow() would
	/// on the next event.
	static void makeCurrent(Engine *io_engine, DWORD i_threadId) {
		Engine::FocusOfThread *fot = findFocus(io_engine, i_threadId);
		io_engine->m_currentFocusOfThread = fot;
		io_engine->m_currentKeymap =
			(fot && !fot->m_keymaps.empty()) ? fot->m_keymaps.front() : NULL;
	}

	/// Put a registered thread back into the state this issue is about: known
	/// to the engine, but without a keymap to its name.
	static void clearKeymaps(Engine *io_engine, DWORD i_threadId) {
		if (Engine::FocusOfThread *fot = findFocus(io_engine, i_threadId))
			fot->m_keymaps.clear();
	}

private:
	static Engine::FocusOfThread *findFocus(Engine *io_engine, DWORD i_threadId) {
		Engine::FocusOfThreads::iterator i =
			io_engine->m_focusOfThreads.find(i_threadId);
		return (i == io_engine->m_focusOfThreads.end()) ? NULL : &(*i).second;
	}
};


// one per test file
void runSettingActivationTests();
void runAdHocKeySeqTests();
void runStickyNoticeTests();


#endif // !_ENGINE_TEST_H
