//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// yamyd.cpp

#include "mayu.h"
#include "hook.h"

static LPTOP_LEVEL_EXCEPTION_FILTER s_prevExceptionFilter = NULL;

static LONG WINAPI crashExceptionFilter(EXCEPTION_POINTERS *i_ep)
{
	emergencyUnhookAll();
	if (s_prevExceptionFilter)
		return s_prevExceptionFilter(i_ep);
	return EXCEPTION_CONTINUE_SEARCH;
}


/// main
int WINAPI wWinMain(HINSTANCE /* i_hInstance */, HINSTANCE /* i_hPrevInstance */,
					 LPWSTR /* i_lpszCmdLine */, int /* i_nCmdShow */)
{
	// crash-safe hook cleanup
	s_prevExceptionFilter = SetUnhandledExceptionFilter(crashExceptionFilter);

	HANDLE mutex = OpenMutex(SYNCHRONIZE, FALSE, MUTEX_YAMYD_BLOCKER);
	if (mutex != NULL) {
		CHECK_FALSE( installMessageHook(0) );

		// wait for master process exit
		WaitForSingleObject(mutex, INFINITE);
		CHECK_FALSE( uninstallMessageHook() );
		ReleaseMutex(mutex);
	}

	return 0;
}
