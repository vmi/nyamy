//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// hook_stub.cpp
// Unit tests build the engine consumer side (engine.cpp / function.cpp) but
// must never install OS hooks. These no-op stubs satisfy the hook DLL imports
// so the test executable links without the nyamy hook DLL.

#define _HOOK_CPP          // suppress the dllimport declarations in hook.h
#include "misc.h"
#include "hook.h"

// In nyamy this points into the shared section of the hook DLL.  A real
// instance rather than a null pointer, because applySetting() writes
// m_correctKanaLockHandling into it: nothing reads it back here.
static HookData s_hookData = {};

DllExport HookData *g_hookData = &s_hookData;

DllExport int installKeyboardHook(INPUT_DETOUR /*i_keyboardDetour*/,
                                  Engine * /*i_engine*/, bool /*i_install*/)
{
	return 0;
}

DllExport int installMouseHook(INPUT_DETOUR /*i_mouseDetour*/,
                               Engine * /*i_engine*/, bool /*i_install*/)
{
	return 0;
}
