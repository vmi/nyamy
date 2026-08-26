//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// core_test.h
//
// Minimal checking for the core-side unit tests (nyamy-tests.vcxproj).
//
// Deliberately small: what belongs in this project is code with no Windows
// and no nyamy dependencies, so its tests need no fixtures, no setup and no
// framework.  Anything that does need those belongs in
// nyamy-scripter-tests.vcxproj, which already drives the whole pipeline.


#ifndef _CORE_TEST_H
#  define _CORE_TEST_H

#  include <cstdio>


/// failures so far, across every test file
extern int g_coreTestFailures;

/// report a failure with its source location
void coreTestFail(const char *i_file, int i_line, const char *i_what);

/// record that a check was made, whether or not it passed
void coreTestCounted();

/// how many checks have run
int coreTestCount();

///
#  define CORE_CHECK(cond, what)					\
	do {								\
		coreTestCounted();					\
		if (!(cond))						\
			coreTestFail(__FILE__, __LINE__, (what));	\
	} while (0)

// one per test file
void runLogBufferTests();
void runLogViewTests();


#endif // !_CORE_TEST_H
