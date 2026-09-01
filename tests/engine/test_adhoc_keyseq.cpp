//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// test_adhoc_keyseq.cpp
//
// Engine::canRunAdHocKeySeq(): the guard in front of &ExecKeySeq and of the
// key sequences user functions send back.  Unlike a key event this path has no
// pass-through to fall back on - with no current keymap the generators would
// dereference NULL - so what it refuses matters more than what it allows.


#include "engine_test.h"


namespace
{

///
AdHocKeySeq makeItem(const std::shared_ptr<Setting> &i_origin)
{
	AdHocKeySeq item(new AdHocItem());
	item->keySeq = std::make_unique<KeySeq>(L"");
	item->origin = i_origin;
	return item;
}


///
void anItemRunsWhenThereIsAKeymap()
{
	womsgstream log(WM_APP);
	Engine engine(log);
	std::shared_ptr<Setting> setting = makeGlobalSetting();
	EngineTestAccess::applySetting(&engine, setting);

	const wchar_t *reason = NULL;
	ENGINE_CHECK(EngineTestAccess::canRunAdHocKeySeq(&engine, makeItem(setting),
													 setting, &reason),
				 "an item runs against the setting it was built for");
	ENGINE_CHECK(reason == NULL, "and there is nothing to report");
}


/** The state this issue is about: a thread the engine knows about, but with no
    keymap to its name.  The item has to be dropped - reconstructing a Current
    from here hands out a NULL keymap, and beginGeneratingKeyboardEvents()
    reads front() off the empty list on top of that. */
void anItemIsDroppedWhenTheThreadHasNoKeymap()
{
	womsgstream log(WM_APP);
	Engine engine(log);
	std::shared_ptr<Setting> setting = makeGlobalSetting();
	EngineTestAccess::applySetting(&engine, setting);

	engine.setFocus(reinterpret_cast<HWND>(0x1234), 4242, L"Foo", L"Bar", false);
	EngineTestAccess::clearKeymaps(&engine, 4242);
	EngineTestAccess::makeCurrent(&engine, 4242);
	ENGINE_CHECK(EngineTestAccess::hasCurrentFocus(&engine),
				 "the thread is the current focus");
	ENGINE_CHECK(EngineTestAccess::currentKeymap(&engine) == NULL,
				 "but there is no current keymap");

	const wchar_t *reason = NULL;
	ENGINE_CHECK(!EngineTestAccess::canRunAdHocKeySeq(&engine, makeItem(setting),
													  setting, &reason),
				 "the item is dropped rather than run without a keymap");
	ENGINE_CHECK(reason != NULL && logHas(reason, L"no keymap"),
				 "and the reason is worth a line in the log");
}


/// An item materialized against a Setting that a reload has replaced holds
/// Key* into it; running it would dereference them.
void anItemFromAnOlderSettingIsDropped()
{
	womsgstream log(WM_APP);
	Engine engine(log);
	std::shared_ptr<Setting> older = makeGlobalSetting();
	std::shared_ptr<Setting> current = makeGlobalSetting();
	EngineTestAccess::applySetting(&engine, current);

	const wchar_t *reason = NULL;
	ENGINE_CHECK(!EngineTestAccess::canRunAdHocKeySeq(&engine, makeItem(older),
													  current, &reason),
				 "an item from a retired Setting is dropped");
	ENGINE_CHECK(reason != NULL && logHas(reason, L"setting reloaded"),
				 "and says which way it went");
}


/// Before the first Setting there is nothing to run against, and that is
/// ordinary enough to keep quiet about.
void anItemIsDroppedSilentlyWithoutASetting()
{
	womsgstream log(WM_APP);
	Engine engine(log);

	const wchar_t *reason = NULL;
	ENGINE_CHECK(!EngineTestAccess::canRunAdHocKeySeq(
					 &engine, makeItem(std::shared_ptr<Setting>()),
					 std::shared_ptr<Setting>(), &reason),
				 "no Setting, nothing to run");
	ENGINE_CHECK(reason == NULL, "and nothing to say about it");
}


/// An item without a key sequence is nothing to run either.
void anEmptyItemIsDroppedSilently()
{
	womsgstream log(WM_APP);
	Engine engine(log);
	std::shared_ptr<Setting> setting = makeGlobalSetting();
	EngineTestAccess::applySetting(&engine, setting);

	AdHocKeySeq item(new AdHocItem());
	item->origin = setting;

	const wchar_t *reason = NULL;
	ENGINE_CHECK(!EngineTestAccess::canRunAdHocKeySeq(&engine, item, setting,
													  &reason),
				 "an item with no key sequence is dropped");
	ENGINE_CHECK(reason == NULL, "silently");
}

} // namespace


void runAdHocKeySeqTests()
{
	anItemRunsWhenThereIsAKeymap();
	anItemIsDroppedWhenTheThreadHasNoKeymap();
	anItemFromAnOlderSettingIsDropped();
	anItemIsDroppedSilentlyWithoutASetting();
	anEmptyItemIsDroppedSilently();
}
