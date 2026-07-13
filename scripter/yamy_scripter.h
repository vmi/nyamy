//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// yamy_scripter.h - Public C API for yamy-scripter DLL
//
// Language-neutral interface used from FFI / mruby.
// All strings are NUL-terminated UTF-8 (no BOM).

#ifndef _YAMY_SCRIPTER_H
#define _YAMY_SCRIPTER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef _YAMY_SCRIPTER_IMPL
#  define YS_API __declspec(dllexport)
#else
#  define YS_API __declspec(dllimport)
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

/// Wrapper around std::vector<YsFuncArg> (opaque; see ys_types.h)
typedef struct YsFuncArgs  YsFuncArgs;

/// Wrapper around std::vector<std::string> (UTF-8)
typedef struct YsStrs      YsStrs;


//=============================================================================
// YsType -- type tag for FuncArg elements
//=============================================================================

typedef enum YsType {
	YsType_Error        = -1,
	YsType_String       = 0,
	YsType_Number       = 1,
	YsType_Regexp       = 2,
	YsType_KeySeqIdx    = 3,
	YsType_ModifierSpec = 4,
	YsType_TokenSeq     = 5,
} YsType;


//=============================================================================
// YsFuncArgs / YsStrs -- memory management
//=============================================================================

/// Create a writable YsFuncArgs / YsStrs.
/// Valid only from within a scripter callback (on_load_setting or on_exec_user_func).
/// Lifetime is managed by the callback session; do not free manually.
YS_API YsFuncArgs* ys_func_args_new(void);
YS_API YsStrs*     ys_strs_new(void);

YS_API int ys_func_args_length(const YsFuncArgs* fas);
YS_API int ys_strs_length(const YsStrs* ss);

/// Read one element.
/// YsType_String / YsType_Regexp:
///   *p_value = const char* (UTF-8, lifetime == YsFuncArgs*), *p_length = byte count (excl. NUL)
/// YsType_Number:      *p_value = int32_t
/// YsType_KeySeqIdx:   *p_value = uint32_t
/// YsType_ModifierSpec: *p_value = modifiers bitmask (uint64_t),
///                      *p_length = dontcares bitmask (uint64_t)
/// YsType_TokenSeq:    *p_value = const YsStrs*, *p_length = element count
YS_API YsType ys_func_args_get(const YsFuncArgs* fas, int idx,
	int64_t* p_value, int64_t* p_length);
YS_API bool   ys_strs_get(const YsStrs* ss, int idx,
	const char** p_value, size_t* p_length);

/// Append one element.  value/length semantics are the same as ys_func_args_get.
YS_API bool ys_func_args_push(YsFuncArgs* fas, YsType type,
	int64_t value, int64_t length);
YS_API bool ys_strs_push(YsStrs* ss, const char* value, size_t length);


//=============================================================================
// Callbacks
//=============================================================================

/// Called when a new setting load is requested.
/// exeCtx: caller-supplied context pointer passed to ys_start().
/// Return true on success (DLL sends the queued commands to the Engine).
typedef bool (*ys_on_load_setting)(void* exeCtx);

/// Called when the Engine fires &ExecUserFunc().
/// exeCtx: caller-supplied context pointer passed to ys_start().
/// args: arguments sent by the Engine with the ExecUserFunc command.
typedef void (*ys_on_exec_user_func)(void*             /* exeCtx */,
                                     const char*       /* user_func_name */,
                                     const YsFuncArgs* /* args */);

/// Callback table for ys_start().
/// on_quit is optional and may be NULL.
typedef struct YsCallbacks {
	bool (*on_load_setting)(void* exeCtx);
	void (*on_quit)(void* exeCtx);
} YsCallbacks;

//=============================================================================
// Main event loop
//=============================================================================

/// Start the scripter event loop.
/// callbacks: callback table (must not be NULL; on_load_setting must not be NULL).
/// exeCtx:   caller-supplied context pointer passed to every callback.
/// Returns 0 when the Engine sends Quit, non-zero on error.
YS_API int ys_start(const YsCallbacks* callbacks, void* exeCtx);

/// Version check for FFI compatibility: major(16) | minor(8) | patch(8)
YS_API uint32_t ys_version(void);


//=============================================================================
// Setting registration API  (valid only from within on_load_setting)
//=============================================================================

/// Register a key sequence.
/// Returns a non-negative index on success, or the existing index if name is
/// already registered.  Returns -1 on failure.
/// name: sequence name (NULL for anonymous; empty string is not allowed).
/// actions: mayu-syntax action string (e.g. "A B C", "&BeginningOfLine", "$otherseq")
YS_API int  ys_reg_keyseq(const char* name, const char* actions);

/// Return the index of a previously registered named key sequence, or -1.
YS_API int  ys_get_keyseq_idx(const char* name);

/// Equivalent to: def key <names...> = <scancodes...>
/// names:     YsStrs of key name strings
/// scancodes: YsStrs of scan-code strings (e.g. "0x1c", "E0-0x1c")
YS_API bool ys_def_key(const YsStrs* names, const YsStrs* scancodes);

/// Equivalent to: def mod <modifier_name> = <key_names...>
YS_API bool ys_def_mod(const char* modifier_name, const YsStrs* key_names);

/// Equivalent to: def sync = <scan_codes...>
YS_API bool ys_def_sync(const YsStrs* scan_codes);

/// Equivalent to: def alias <alias_name> = <key_name>
YS_API bool ys_def_alias(const char* alias_name, const char* key_name);

/// Equivalent to: def subst <lhs_mod_keys...> = <keyseq>
/// lhs_mod_keys: YsStrs of modifier-key strings (e.g. "*-LButton")
YS_API bool ys_def_subst(const YsStrs* lhs_mod_keys, int rhs_keyseq_idx);

/// Equivalent to: def option <option_name> = <value>
YS_API bool ys_def_option(const char* option_name, const char* value);

/// Equivalent to: keymap / keymap2 / window directive.
/// keyword:       "keymap", "keymap2", or "window"
/// window_class:  regex pattern string (NULL for non-window keymaps)
/// window_title:  regex pattern string (NULL if not used)
/// op:            "&&", "||", or NULL (default: "&&")
/// parent_name:   NULL if no parent
/// default_keyseq_idx: -1 means none
YS_API bool ys_begin_keymap(const char* keyword, const char* name,
	const char* window_class, const char* window_title,
	const char* op, const char* parent_name,
	int default_keyseq_idx);

/// Equivalent to: key <lhs_mod_keys...> = <keyseq>  (inside a keymap block)
YS_API bool ys_assign_key(const YsStrs* lhs_mod_keys, int rhs_keyseq_idx);

/// Equivalent to: event <event_name> = <keyseq>  (inside a keymap block)
YS_API bool ys_assign_event(const char* event_name, int rhs_keyseq_idx);

/// Equivalent to: mod [<prefixes...>] <modifier_name> <op> <keys...>
/// prefixes: YsStrs of "assign_mode + modifier_name" strings
///           (e.g. "!Shift", "!!!Ctrl") -- NULL means no prefixes.
/// op:       "=", "+=", or "-="
/// keys:     YsStrs of key name strings (assign-mode prefix allowed,
///           e.g. "LShift", "!LShift", "!!!CapsLock")
YS_API bool ys_assign_mod(const YsStrs* prefixes, const char* modifier_name,
	const char* op, const YsStrs* keys);

/// Register a user-defined function handler.
/// When the Engine fires &ExecUserFunc(func_name), on_exec_user_func is called
/// with func_name, the args sent by the Engine, and the trigger context.
YS_API bool ys_reg_user_func(const char* func_name, ys_on_exec_user_func on_exec_user_func);

/// Equivalent to: define <symbol>
/// Adds the symbol to the current symbol set so that subsequent ys_has_symbol()
/// queries see it and flushQueue() emits a DefSymbol for it.  Idempotent.
YS_API bool ys_define_symbol(const char* name);

/// Return true if the named symbol is defined in the current symbol set.
/// (The set comes from the Start command plus any ys_define_symbol() calls.)
YS_API bool ys_has_symbol(const char* name);

/// Clear all queued setting commands (call before rebuilding from scratch).
YS_API bool ys_reset_setting(void);

/// Compile the default .mayu file (as resolved by ConfigFiles) and queue it.
/// Intended usage: return ys_load_mayu(); from on_load_setting.
YS_API bool ys_load_mayu(void);

/// Compile the .mayu file at path and queue it.
/// path: UTF-8 file path (relative paths are resolved from the config directory).
YS_API bool ys_include_mayu(const char* path);


//=============================================================================
// Scan-code query API  (valid only from within on_load_setting)
//=============================================================================

/// Resolve a key name or scan-code string to its scan-code WORD value.
/// The WORD is (prefix << 8) | code, where prefix is 0x00 (plain), 0xE0, or 0xE1
/// -- the same encoding used by the registry Scancode Map.
/// str: a key name defined by a prior ys_def_key (case-insensitive, aliases
///      included), or a scan-code literal ("0x1c", "E0-0x1c", "E1-0x0f", "28").
///      Key names take priority; a name that is not defined is parsed as a literal.
/// Returns the WORD value (0..0xE1FF) on success, or -1 if unresolvable.
YS_API int ys_sc_resolve(const char* str);

/// Number of entries in the (cached) registry Scancode Map.
/// The map is read lazily on first access and cleared by a new setting load.
/// Returns 0 when no Scancode Map is configured or the value is malformed.
YS_API int ys_scancode_map_length(void);

/// Read one Scancode Map entry as WORD values (see ys_sc_resolve for the encoding).
/// from_word: original scan code, to_word: remapped scan code (0 means disabled).
/// Both out pointers may be NULL.  Returns false if idx is out of range.
YS_API bool ys_scancode_map_entry(int idx, unsigned* from_word, unsigned* to_word);


//=============================================================================
// Path resolution API  (valid from on_load_setting and on_exec_user_func)
//=============================================================================

/// Return the home directory list used for config file lookup (UTF-8 strings).
/// Includes %USERPROFILE%\.config\yamy, %LOCALAPPDATA%\Yamy\Config, and the
/// executable directory, in that search order.
/// Lifetime: managed by the callback session; do not free manually.
YS_API YsStrs* ys_get_home_directories(void);

/// Resolve a config file name to an absolute path without compiling.
/// name:     file name (NULL or "" resolves the default .mayu via registry then home dirs)
/// out_path: resolved absolute path (NULL to ignore) -- session lifetime
/// Returns false if the file cannot be found; sets ys_last_error().
YS_API bool ys_resolve_config_path(const char*  name,
                                    const char** out_path);


//=============================================================================
// Runtime API  (valid only from within on_exec_user_func)
//=============================================================================

/// Ask the Engine to execute an ad-hoc key sequence.
/// - Must NOT be called from on_load_setting.
/// - actions must NOT contain &ExecUserFunc (infinite-loop guard).
YS_API bool ys_exec_keyseq(const char* actions);


//=============================================================================
// Error reporting
//=============================================================================

/// Return the last error message as a UTF-8 NUL-terminated string,
/// or NULL if no error has occurred.
YS_API const char* ys_last_error(void);


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
YS_API bool parseScancodeMapBlob(const unsigned char* data, size_t len,
	std::vector<std::pair<uint16_t, uint16_t>>& out);
#endif // __cplusplus

#endif // _YAMY_SCRIPTER_H
