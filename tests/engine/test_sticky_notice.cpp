//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// test_sticky_notice.cpp
//
// Engine::StickyNotice and reportSticky().  These conditions hold until the
// focus or the setting changes, so reporting them per key event fills the log
// ring and throws away the lines that say how the state was reached - which is
// exactly what is wanted when it is investigated.


#include "engine_test.h"


namespace
{

using StickyNotice = EngineTestAccess::StickyNotice;


///
void aNoticeReportsOnlyItsFirstOccurrence()
{
	StickyNotice notice;
	ENGINE_CHECK(!notice.isActive(), "starts inactive");
	ENGINE_CHECK(notice.shouldReport(), "the first occurrence is reported");
	ENGINE_CHECK(notice.isActive(), "and is remembered");
	ENGINE_CHECK(!notice.shouldReport(), "the second is not");
	ENGINE_CHECK(!notice.shouldReport(), "nor the third");
}


///
void aNoticeCountsWhatItSuppressed()
{
	StickyNotice notice;
	size_t suppressed = 12345;

	ENGINE_CHECK(!notice.clear(&suppressed), "clearing an inactive notice");
	ENGINE_CHECK(suppressed == 12345, "leaves the count alone");

	notice.shouldReport();
	notice.shouldReport();
	notice.shouldReport();
	ENGINE_CHECK(notice.clear(&suppressed), "clearing an active notice");
	ENGINE_CHECK(suppressed == 2, "counts the occurrences it kept quiet about");
	ENGINE_CHECK(!notice.isActive(), "and ends the condition");

	ENGINE_CHECK(notice.shouldReport(), "the next occurrence is reported again");
	ENGINE_CHECK(notice.clear(&suppressed), "and clears again");
	ENGINE_CHECK(suppressed == 0, "with nothing suppressed this time");
}


///
void reportStickyWritesOnceAndOnRecovery()
{
	womsgstream log(WM_APP);
	Engine engine(log);
	StickyNotice notice;
	takeLog(&log);

	EngineTestAccess::reportSticky(&engine, &notice, true, LogLevel::Error,
								   L"internal error: m_currentKeymap == NULL");
	ENGINE_CHECK(logHas(takeLog(&log), L"m_currentKeymap == NULL"),
				 "the condition is reported when it starts");

	for (int i = 0; i < 100; ++ i)
		EngineTestAccess::reportSticky(&engine, &notice, true, LogLevel::Error,
									   L"internal error: m_currentKeymap == NULL");
	ENGINE_CHECK(takeLog(&log).empty(),
				 "a hundred more key events add nothing to the log");

	EngineTestAccess::reportSticky(&engine, &notice, false, LogLevel::Error,
								   L"internal error: m_currentKeymap == NULL");
	std::wstring text = takeLog(&log);
	ENGINE_CHECK(logHas(text, L"recovered:"), "recovery is reported");
	ENGINE_CHECK(logHas(text, L"(suppressed 100 more)"),
				 "with how many occurrences went unsaid");
}


/// Nothing is written for a condition that never started.
void reportStickyIsQuietWhenNothingHappened()
{
	womsgstream log(WM_APP);
	Engine engine(log);
	StickyNotice notice;
	takeLog(&log);

	for (int i = 0; i < 10; ++ i)
		EngineTestAccess::reportSticky(&engine, &notice, false, LogLevel::Error,
									   L"internal error: m_currentKeymap == NULL");
	ENGINE_CHECK(takeLog(&log).empty(), "a healthy engine says nothing");
}


/// Recovery without suppressed occurrences leaves the count off the line.
void recoveryWithoutSuppressionOmitsTheCount()
{
	womsgstream log(WM_APP);
	Engine engine(log);
	StickyNotice notice;

	EngineTestAccess::reportSticky(&engine, &notice, true, LogLevel::Error,
								   L"internal error: m_currentKeymap == NULL");
	takeLog(&log);
	EngineTestAccess::reportSticky(&engine, &notice, false, LogLevel::Error,
								   L"internal error: m_currentKeymap == NULL");

	std::wstring text = takeLog(&log);
	ENGINE_CHECK(logHas(text, L"recovered:"), "recovery is reported");
	ENGINE_CHECK(!logHas(text, L"suppressed"),
				 "with no count when there is none to give");
}


/** A condition that was never wrong ends without a word.  "no setting yet" is
    the case: passing keys on untouched is the right thing to do until a
    setting exists, and calling the end of it a recovery would read as though
    something had gone wrong. */
void aConditionThatWasNotWrongEndsQuietly()
{
	womsgstream log(WM_APP);
	Engine engine(log);
	StickyNotice notice;

	EngineTestAccess::reportSticky(&engine, &notice, true, LogLevel::Info,
								   L"no setting yet; keys are not remapped",
								   false);
	ENGINE_CHECK(logHas(takeLog(&log), L"no setting yet"),
				 "the condition is still reported when it starts");

	EngineTestAccess::reportSticky(&engine, &notice, true, LogLevel::Info,
								   L"no setting yet; keys are not remapped",
								   false);
	EngineTestAccess::reportSticky(&engine, &notice, false, LogLevel::Info,
								   L"no setting yet; keys are not remapped",
								   false);
	ENGINE_CHECK(takeLog(&log).empty(), "but its end is not news");
	ENGINE_CHECK(!notice.isActive(), "and it is cleared all the same");
}

} // namespace


void runStickyNoticeTests()
{
	aNoticeReportsOnlyItsFirstOccurrence();
	aNoticeCountsWhatItSuppressed();
	reportStickyWritesOnceAndOnRecovery();
	reportStickyIsQuietWhenNothingHappened();
	recoveryWithoutSuppressionOmitsTheCount();
	aConditionThatWasNotWrongEndsQuietly();
}
