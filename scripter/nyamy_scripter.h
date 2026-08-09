//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// nyamy_scripter.h - Public C API for nyamy-scripter DLL
//
// Language-neutral interface used from FFI / mruby.
// All strings are NUL-terminated UTF-8 (no BOM).

#ifndef _NYAMY_SCRIPTER_H
#define _NYAMY_SCRIPTER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef _NYAMY_SCRIPTER_IMPL
#  define NYS_API __declspec(dllexport)
#else
#  define NYS_API __declspec(dllimport)
#endif

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus
#if 0 // Dummy code to prevent the IDE from indenting inside extern "C"
}
#endif

//=============================================================================
// Opaque types
//=============================================================================

/// Wrapper around std::vector<NYsFuncArg> (opaque; see nys_types.h)
typedef struct NYsFuncArgs  NYsFuncArgs;

/// Wrapper around std::vector<std::string> (UTF-8)
typedef struct NYsStrs      NYsStrs;


//=============================================================================
// NYsType -- type tag for FuncArg elements
//=============================================================================

typedef enum NYsType {
	NYsType_Error        = -1,
	NYsType_String       = 0,
	NYsType_Number       = 1,
	NYsType_Regexp       = 2,
	NYsType_KeySeqIdx    = 3,
	NYsType_ModifierSpec = 4,
	NYsType_TokenSeq     = 5,
} NYsType;


//=============================================================================
// NYsFuncArgs / NYsStrs -- memory management
//=============================================================================

/// Create a writable NYsFuncArgs / NYsStrs.
/// Valid only from within a scripter callback (on_load_setting or on_exec_user_func).
/// Lifetime is managed by the callback session; do not free manually.
NYS_API NYsFuncArgs* nys_func_args_new(void);
NYS_API NYsStrs*     nys_strs_new(void);

NYS_API int nys_func_args_length(const NYsFuncArgs* fas);
NYS_API int nys_strs_length(const NYsStrs* ss);

/// Read one element.
/// NYsType_String / NYsType_Regexp:
///   *p_value = const char* (UTF-8, lifetime == NYsFuncArgs*), *p_length = byte count (excl. NUL)
/// NYsType_Number:      *p_value = int32_t
/// NYsType_KeySeqIdx:   *p_value = uint32_t
/// NYsType_ModifierSpec: *p_value = modifiers bitmask (uint64_t),
///                      *p_length = dontcares bitmask (uint64_t)
/// NYsType_TokenSeq:    *p_value = const NYsStrs*, *p_length = element count
NYS_API NYsType nys_func_args_get(const NYsFuncArgs* fas, int idx,
	int64_t* p_value, int64_t* p_length);
NYS_API bool   nys_strs_get(const NYsStrs* ss, int idx,
	const char** p_value, size_t* p_length);

/// Append one element.  value/length semantics are the same as nys_func_args_get.
NYS_API bool nys_func_args_push(NYsFuncArgs* fas, NYsType type,
	int64_t value, int64_t length);
NYS_API bool nys_strs_push(NYsStrs* ss, const char* value, size_t length);


//=============================================================================
// Callbacks
//=============================================================================

/// Called when a new setting load is requested.
/// exeCtx: caller-supplied context pointer passed to nys_start().
/// Return true on success (DLL sends the queued commands to the Engine).
typedef bool (*nys_on_load_setting)(void* exeCtx);

/// Called when the Engine fires &ExecUserFunc().
/// exeCtx: caller-supplied context pointer passed to nys_start().
/// args: arguments sent by the Engine with the ExecUserFunc command.
typedef void (*nys_on_exec_user_func)(void*             /* exeCtx */,
                                     const char*       /* user_func_name */,
                                     const NYsFuncArgs* /* args */);

/// Callback table for nys_start().
/// on_quit is optional and may be NULL.
typedef struct NYsCallbacks {
	bool (*on_load_setting)(void* exeCtx);
	void (*on_quit)(void* exeCtx);
} NYsCallbacks;

//=============================================================================
// Main event loop
//=============================================================================

/// Start the scripter event loop.
/// callbacks: callback table (must not be NULL; on_load_setting must not be NULL).
/// exeCtx:   caller-supplied context pointer passed to every callback.
/// Returns 0 when the Engine sends Quit, non-zero on error.
///
/// Runs the callbacks on the calling thread and reads the ctrl stream on a
/// thread of its own, so that Quit is observed even while a callback is
/// running.  Whatever was queued before Quit still runs first.
NYS_API int nys_start(const NYsCallbacks* callbacks, void* exeCtx);

/// Milliseconds nys_start() waits, after Quit (or ctrl-pipe EOF) is observed,
/// for the calling thread to finish what it is running.  On timeout the process
/// is terminated outright: a script stuck in a loop or a blocking call cannot
/// be unwound, and a scripter that outlives the nyamy that spawned it would run
/// on with nobody left to stop it.  A scripter launched by nyamy should pass
/// kScripterQuitTimeoutMillisec (ctrl_stream.h), which nyamy's own grace period
/// is set to exceed.
/// 0 (the default) waits indefinitely, which is what an in-process host wants.
/// Call before nys_start().
NYS_API void nys_set_quit_timeout(uint32_t millisec);

/// Version check for FFI compatibility: major(16) | minor(8) | patch(8)
NYS_API uint32_t nys_version(void);


//=============================================================================
// Setting registration API  (valid only from within on_load_setting)
//=============================================================================

/// Modifier context of the actions passed to nys_reg_keyseq().
/// A key-sequence right side may only carry the basic modifiers and U- / D-;
/// a named sequence definition and a substitute target may also carry the
/// ASSIGN-class ones (R-, NL-, M0-, L0-, ...).
enum {
	NYS_MODCTX_KEYSEQ = 0,		///< right side of a key / event assignment
	NYS_MODCTX_ASSIGN = 1,		///< named keyseq definition, substitute target
};

/// Register a key sequence.
/// Returns a non-negative index on success, or the existing index if name is
/// already registered.  Returns -1 on failure.
/// name: sequence name (NULL for anonymous; empty string is not allowed).
/// actions: mayu-syntax action string (e.g. "A B C", "&BeginningOfLine", "$otherseq")
/// context: one of NYS_MODCTX_*
NYS_API int  nys_reg_keyseq(const char* name, const char* actions, int context);

/// Return the index of a previously registered named key sequence, or -1.
NYS_API int  nys_get_keyseq_idx(const char* name);

/// Equivalent to: def key <names...> = <scancodes...>
/// names:     NYsStrs of key name strings
/// scancodes: NYsStrs of scan-code strings (e.g. "0x1c", "E0-0x1c")
NYS_API bool nys_def_key(const NYsStrs* names, const NYsStrs* scancodes);

/// Equivalent to: def mod <modifier_name> = <key_names...>
NYS_API bool nys_def_mod(const char* modifier_name, const NYsStrs* key_names);

/// Equivalent to: def sync = <scan_codes...>
NYS_API bool nys_def_sync(const NYsStrs* scan_codes);

/// Equivalent to: def alias <alias_name> = <key_name>
NYS_API bool nys_def_alias(const char* alias_name, const char* key_name);

/// Equivalent to: def subst <lhs_mod_keys...> = <keyseq>
/// lhs_mod_keys: NYsStrs of modifier-key strings (e.g. "*-LButton")
NYS_API bool nys_def_subst(const NYsStrs* lhs_mod_keys, int rhs_keyseq_idx);

/// Equivalent to: def option <option_name> = <value>
NYS_API bool nys_def_option(const char* option_name, const char* value);

/// Equivalent to: keymap / keymap2 / window directive.
/// keyword:       "keymap", "keymap2", or "window"
/// window_class:  regex pattern string (NULL for non-window keymaps)
/// window_title:  regex pattern string (NULL if not used)
/// op:            "&&", "||", or NULL (default: "&&")
/// parent_name:   NULL if no parent
/// default_keyseq_idx: -1 means none
NYS_API bool nys_begin_keymap(const char* keyword, const char* name,
	const char* window_class, const char* window_title,
	const char* op, const char* parent_name,
	int default_keyseq_idx);

/// Equivalent to: key <lhs_mod_keys...> = <keyseq>  (inside a keymap block)
NYS_API bool nys_assign_key(const NYsStrs* lhs_mod_keys, int rhs_keyseq_idx);

/// Equivalent to: event <event_name> = <keyseq>  (inside a keymap block)
NYS_API bool nys_assign_event(const char* event_name, int rhs_keyseq_idx);

/// Equivalent to: mod [<prefixes...>] <modifier_name> <op> <keys...>
/// prefixes: NYsStrs of "assign_mode + modifier_name" strings
///           (e.g. "!Shift", "!!!Ctrl") -- NULL means no prefixes.
/// op:       "=", "+=", or "-="
/// keys:     NYsStrs of key name strings (assign-mode prefix allowed,
///           e.g. "LShift", "!LShift", "!!!CapsLock")
NYS_API bool nys_assign_mod(const NYsStrs* prefixes, const char* modifier_name,
	const char* op, const NYsStrs* keys);

/// Register a user-defined function handler.
/// When the Engine fires &ExecUserFunc(func_name), on_exec_user_func is called
/// with func_name, the args sent by the Engine, and the trigger context.
NYS_API bool nys_reg_user_func(const char* func_name, nys_on_exec_user_func on_exec_user_func);

/// Equivalent to: define <symbol>
/// Adds the symbol to the current symbol set so that subsequent nys_has_symbol()
/// queries see it and flushQueue() emits a DefSymbol for it.  Idempotent.
NYS_API bool nys_define_symbol(const char* name);

/// Return true if the named symbol is defined in the current symbol set.
/// (The set comes from the Start command plus any nys_define_symbol() calls.)
NYS_API bool nys_has_symbol(const char* name);

/// Clear all queued setting commands (call before rebuilding from scratch).
NYS_API bool nys_reset_setting(void);

/// Compile the default .mayu file (as resolved by ConfigFiles) and queue it.
/// Intended usage: return nys_load_mayu(); from on_load_setting.
NYS_API bool nys_load_mayu(void);

/// Compile the .mayu file at path and queue it.
/// path: UTF-8 file path (relative paths are resolved from the config directory).
NYS_API bool nys_include_mayu(const char* path);


//=============================================================================
// Scan-code query API  (valid only from within on_load_setting)
//=============================================================================

/// Resolve a key name or scan-code string to its scan-code WORD value.
/// The WORD is (prefix << 8) | code, where prefix is 0x00 (plain), 0xE0, or 0xE1
/// -- the same encoding used by the registry Scancode Map.
/// str: a key name defined by a prior nys_def_key (case-insensitive, aliases
///      included), or a scan-code literal ("0x1c", "E0-0x1c", "E1-0x0f", "28").
///      Key names take priority; a name that is not defined is parsed as a literal.
/// Returns the WORD value (0..0xE1FF) on success, or -1 if unresolvable.
NYS_API int nys_sc_resolve(const char* str);

/// Number of entries in the (cached) registry Scancode Map.
/// The map is read lazily on first access and cleared by a new setting load.
/// Returns 0 when no Scancode Map is configured or the value is malformed.
NYS_API int nys_scancode_map_length(void);

/// Read one Scancode Map entry as WORD values (see nys_sc_resolve for the encoding).
/// from_word: original scan code, to_word: remapped scan code (0 means disabled).
/// Both out pointers may be NULL.  Returns false if idx is out of range.
NYS_API bool nys_scancode_map_entry(int idx, unsigned* from_word, unsigned* to_word);

/// Return true if word (as resolved by nys_sc_resolve) is currently
/// registered by "def option nls-keys".
/// Reflects the most recent nys_def_option("nls-keys", ...) call, parsed the
/// same way as the downstream Setting build; a key defined after that call
/// is not seen until the option is set again.
NYS_API bool nys_is_nls_key_word(int word);


//=============================================================================
// Path API  (callable at any time)
//=============================================================================

/// NYamy's directory layout, as UTF-8 paths without a trailing separator.
/// The values come from the NYAMY_ROOT / NYAMY_HOME / NYAMY_CONFIG environment
/// variables that nyamy publishes before launching the scripter; running
/// without nyamy above falls back to the executable directory and
/// %LOCALAPPDATA%\NYamy.
/// Lifetime: valid for the life of the process; never NULL.
///
///   root    directory holding nyamy.exe (distributed .mayu / .rb files)
///   home    per-user data directory; "<home>\Lib" is on $LOAD_PATH
///   config  per-user configuration directory (nyamy.ini, .mayu, .mayu.rb)
///
/// Config files are searched in config, then root.
NYS_API const char* nys_paths_root(void);
NYS_API const char* nys_paths_home(void);
NYS_API const char* nys_paths_config(void);

/// Resolve a config file name to an absolute path without compiling.
/// name:     file name; an absolute path is only checked for readability, a
///           relative one is searched in the config search path.
///           NULL or "" resolves the path received from the Engine, or the
///           default ".mayu".
/// out_path: resolved absolute path (NULL to ignore) -- session lifetime
/// Returns false if the file cannot be found; sets nys_last_error().
/// Valid from on_load_setting and on_exec_user_func.
NYS_API bool nys_resolve_config_path(const char*  name,
                                    const char** out_path);


//=============================================================================
// Runtime API  (valid only from within on_exec_user_func)
//=============================================================================

/// Ask the Engine to execute an ad-hoc key sequence.
/// - Must NOT be called from on_load_setting.
/// - actions must NOT contain &ExecUserFunc (infinite-loop guard).
NYS_API bool nys_exec_keyseq(const char* actions);


//=============================================================================
// Error reporting
//=============================================================================

/// Return the last error message as a UTF-8 NUL-terminated string,
/// or NULL if no error has occurred.
NYS_API const char* nys_last_error(void);


#ifdef __cplusplus
} // extern "C"
#endif // __cplusplus


//=============================================================================
// C++-only helpers (exported for unit tests)
//=============================================================================

#ifdef __cplusplus
#include <vector>
#include <utility>

/// Parse a raw registry "Scancode Map" REG_BINARY blob into (from, to) WORD
/// pairs.  Layout: header1(4) + header2(4) + count(4) + count*DWORD entries,
/// where the last entry is a null terminator, so the number of mappings is
/// count-1.  Each entry DWORD packs HIWORD = original scan code, LOWORD =
/// remapped scan code (0 disables the key).  A WORD is (prefix<<8)|code with
/// the high byte 0x00 or 0xE0 (extended).
/// out receives (from=HIWORD, to=LOWORD) for every mapping entry.
/// Returns false (and leaves out empty) when the blob is malformed.
NYS_API bool parseScancodeMapBlob(const unsigned char* data, size_t len,
	std::vector<std::pair<uint16_t, uint16_t>>& out);
#endif // __cplusplus

#endif // _NYAMY_SCRIPTER_H
