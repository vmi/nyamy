//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// engine_test_main.cpp
//
// Unit tests for Engine's setting activation and for the notices around it.


#include "engine_test.h"
#include "function.h"


int g_engineTestFailures = 0;
static int s_engineTestCount = 0;


void engineTestFail(const char *i_file, int i_line, const char *i_what)
{
	printf("FAIL: %s(%d): %s\n", i_file, i_line, i_what);
	++ g_engineTestFailures;
}


void engineTestCounted()
{
	++ s_engineTestCount;
}


int engineTestCount()
{
	return s_engineTestCount;
}


// Mirrors what CmdProcessor::beginSetting() puts in place: a keymap of
// Type_windowOr with empty patterns, which matches every window.
std::shared_ptr<Setting> makeGlobalSetting()
{
	std::shared_ptr<Setting> setting = std::make_shared<Setting>();
	ActionFunction af(createFunctionData(L"OtherWindowClass"));
	KeySeq *defaultKeySeq = setting->m_keySeqs.add(KeySeq(L"").add(af));
	setting->m_keymaps.add(Keymap(Keymap::Type_windowOr, L"Global", L"", L"",
								  defaultKeySeq, NULL));
	return setting;
}


// Type_keymap never matches a window (Keymap::doesSameWindow), so searching
// for the global focus in this one comes back empty.  The loader cannot
// produce it, which is why the case is only reachable from here - and why the
// engine used to read front() off the empty list rather than handle it.
std::shared_ptr<Setting> makeUnmatchableSetting()
{
	std::shared_ptr<Setting> setting = std::make_shared<Setting>();
	ActionFunction af(createFunctionData(L"OtherWindowClass"));
	KeySeq *defaultKeySeq = setting->m_keySeqs.add(KeySeq(L"").add(af));
	setting->m_keymaps.add(Keymap(Keymap::Type_keymap, L"Lonely", L"", L"",
								  defaultKeySeq, NULL));
	return setting;
}


std::wstring takeLog(womsgstream *io_log)
{
	std::wstring text;
	io_log->takeString(&text);
	return text;
}


bool logHas(const std::wstring &i_log, const wchar_t *i_text)
{
	return i_log.find(i_text) != std::wstring::npos;
}


int main()
{
	runSettingActivationTests();
	runAdHocKeySeqTests();
	runStickyNoticeTests();

	if (g_engineTestFailures == 0) {
		printf("\nALL PASSED (%d checks)\n", engineTestCount());
		return 0;
	}
	printf("\nFAILED (%d of %d checks)\n", g_engineTestFailures,
		   engineTestCount());
	return 1;
}
