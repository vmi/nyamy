//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// mruby_binding.h
//
// Public interface for the mruby DSL binding (exe side).
// Pass mruby_on_load_setting / mruby_on_quit in a NYsCallbacks struct to
// nys_start(), with a pointer to an MRubyContext as exeCtx.
// mruby_on_exec_user_func is registered via nys_reg_user_func() inside
// mruby_on_load_setting and is not passed directly to nys_start().

#ifndef _MRUBY_BINDING_H
#  define _MRUBY_BINDING_H

#  include "nyamy_scripter.h"
#  include <mruby.h>

#  ifdef __cplusplus
extern "C" {
#    if 0
}
#    endif
#  endif


// Caller-supplied context carried through nys_start().
// mrb is set inside mruby_on_load_setting and closed in mruby_on_quit.
//
// scriptArgIndex is where <script> sits in argv, which is not a constant once
// options exist; ARGV is built from the arguments after it.  includeDirs are
// the -I directories, prepended to $LOAD_PATH at each load.  Both are owned by
// the caller and must outlive nys_start(), since every reload reads them again.
struct MRubyContext {
	int                argc;
	const char* const* argv;
	mrb_state*         mrb;   // initially nullptr; set by mruby_on_load_setting
	int                scriptArgIndex;   // argv index of <script>
	const char* const* includeDirs;      // NULL-terminated, or NULL for none
};


// Callbacks for NYsCallbacks.on_load_setting / NYsCallbacks.on_quit.
bool mruby_on_load_setting(void* exeCtx);
void mruby_on_quit(void* exeCtx);

// Handler registered via nys_reg_user_func() inside mruby_on_load_setting.
// Dispatches Engine-sent args to the registered Ruby block.
void mruby_on_exec_user_func(void*                exeCtx,
                              const char*          func_name,
                              const NYsFuncArgs*    args);


#  ifdef __cplusplus
}
#  endif
#endif // !_MRUBY_BINDING_H
