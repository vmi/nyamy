//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// core_test_main.cpp
//
// Unit tests for the parts of nyamy that have been made free of Windows and
// of the rest of nyamy, so that they can be checked without a window.


#include "core_test.h"


int g_coreTestFailures = 0;
static int s_coreTestCount = 0;


void coreTestFail(const char *i_file, int i_line, const char *i_what)
{
	printf("FAIL: %s(%d): %s\n", i_file, i_line, i_what);
	++ g_coreTestFailures;
}


void coreTestCounted()
{
	++ s_coreTestCount;
}


int coreTestCount()
{
	return s_coreTestCount;
}


int main()
{
	runLogBufferTests();
	runLogViewTests();

	if (g_coreTestFailures == 0) {
		printf("\nALL PASSED (%d checks)\n", coreTestCount());
		return 0;
	}
	printf("\nFAILED (%d of %d checks)\n", g_coreTestFailures,
		   coreTestCount());
	return 1;
}
