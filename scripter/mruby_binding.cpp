//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// mruby_binding.cpp
//
// mruby DSL binding for yamy-scripter.exe (mruby variant).
// Implements Yamy::DSL, Yamy::KeySeq, Yamy::KeyMap,
// Yamy::EventMap, Yamy::ModMap, Yamy::ModValue, and Yamy::Modifier.
//
// The .rb script is evaluated via instance_eval on a Yamy::DSL object
// each time on_load_setting is called.

#include "mruby_binding.h"
#include "yamy_scripter.h"

#include <mruby.h>
#include <mruby/array.h>
#include <mruby/class.h>
#include <mruby/data.h>
#include <mruby/error.h>
#include <mruby/hash.h>
#include <mruby/string.h>
#include <mruby/variable.h>
#include <mruby/compile.h>

#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <windows.h>


//=============================================================================
// Globals
//=============================================================================

// func_name -> mrb proc (GC-protected as a module constant)
static mrb_value g_funcTable;   // mruby Hash: String => Proc


//=============================================================================
// Utility helpers
//=============================================================================

static std::string toStdStr(mrb_state *mrb, mrb_value v)
{
	if (!mrb_string_p(v))
		v = mrb_funcall(mrb, v, "to_s", 0);
	return std::string(RSTRING_PTR(v), RSTRING_LEN(v));
}

// Raise RuntimeError with the last C API error, or fallback if none.
static void raiseApiError(mrb_state *mrb, const char *fallback)
{
	const char *msg = ys_last_error();
	mrb_raise(mrb, E_RUNTIME_ERROR, msg ? msg : fallback);
}

static std::wstring utf8ToWide(const char *s, size_t len)
{
	if (len == 0)
		return std::wstring();
	int wn = MultiByteToWideChar(CP_UTF8, 0, s, (int)len, nullptr, 0);
	std::wstring w(wn, L'\0');
	MultiByteToWideChar(CP_UTF8, 0, s, (int)len, &w[0], wn);
	return w;
}

static std::wstring utf8ToWide(const std::string &s)
{
	return utf8ToWide(s.c_str(), s.size());
}

static std::string wideToUtf8(const std::wstring &w)
{
	if (w.empty())
		return std::string();
	int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(),
		nullptr, 0, nullptr, nullptr);
	std::string s(n, '\0');
	WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(),
		&s[0], n, nullptr, nullptr);
	return s;
}

static bool isAbsolutePath(const std::string &path)
{
	if (path.size() >= 2 && isalpha((unsigned char)path[0]) && path[1] == ':')
		return true;
	return !path.empty() && (path[0] == '\\' || path[0] == '/');
}

static bool isRegularFileW(const std::wstring &path)
{
	DWORD attr = GetFileAttributesW(path.c_str());
	return attr != INVALID_FILE_ATTRIBUTES &&
		!(attr & FILE_ATTRIBUTE_DIRECTORY);
}

// Make the path absolute and normalized (UTF-8 in/out).
// Falls back to the input on failure.
static std::string canonicalizePath(const std::wstring &wpath)
{
	DWORD need = GetFullPathNameW(wpath.c_str(), 0, nullptr, nullptr);
	if (need == 0)
		return wideToUtf8(wpath);
	std::wstring full(need, L'\0');
	DWORD n = GetFullPathNameW(wpath.c_str(), need, &full[0], nullptr);
	if (n == 0 || n >= need)
		return wideToUtf8(wpath);
	full.resize(n);
	return wideToUtf8(full);
}

// Resolve a .rb path to a canonical absolute path.  An absolute path is
// used as is; a relative path is searched in $LOAD_PATH (re-read on every
// call so that script-side modifications take effect).  Raises when the
// file is not found.
static std::string resolveRbPath(mrb_state *mrb, const std::string &path)
{
	if (isAbsolutePath(path)) {
		if (isRegularFileW(utf8ToWide(path)))
			return canonicalizePath(utf8ToWide(path));
		mrb_raisef(mrb, E_RUNTIME_ERROR,
			"cannot open file: %s", path.c_str());
	}

	std::string searched;
	mrb_value lp = mrb_gv_get(mrb, mrb_intern_lit(mrb, "$LOAD_PATH"));
	if (mrb_array_p(lp)) {
		mrb_int n = RARRAY_LEN(lp);
		for (mrb_int i = 0; i < n; ++i) {
			mrb_value dir = mrb_ary_ref(mrb, lp, i);
			if (!mrb_string_p(dir))
				continue;
			std::string d(RSTRING_PTR(dir), RSTRING_LEN(dir));
			if (d.empty())
				continue;
			std::wstring candidate = utf8ToWide(d + "\\" + path);
			if (isRegularFileW(candidate))
				return canonicalizePath(candidate);
			if (!searched.empty())
				searched += ";";
			searched += d;
		}
	}
	mrb_raisef(mrb, E_RUNTIME_ERROR,
		"file not found in $LOAD_PATH: %s (searched: %s)",
		path.c_str(), searched.c_str());
	return std::string();	// not reached
}

// Evaluate the .rb file at the canonical absolute path via instance_eval
// on the DSL object.  A pending exception (mrb->exc) is left to the caller.
static void evalRbFile(mrb_state *mrb, mrb_value self, const std::string &absPath)
{
	FILE *f = _wfopen(utf8ToWide(absPath).c_str(), L"rb");
	if (!f)
		mrb_raisef(mrb, E_RUNTIME_ERROR,
			"cannot open file: %s", absPath.c_str());
	fseek(f, 0, SEEK_END);
	long sz = ftell(f);
	fseek(f, 0, SEEK_SET);
	std::string buf(sz, '\0');
	fread(&buf[0], 1, sz, f);
	fclose(f);

	mrb_value code = mrb_str_new(mrb, buf.c_str(), buf.size());
	// Pass the file name and start line so that exception backtraces
	// point into the loaded script instead of "(eval)".
	mrb_funcall(mrb, self, "instance_eval", 3, code,
		mrb_str_new_cstr(mrb, absPath.c_str()),
		mrb_int_value(mrb, 1));
}

// Print the pending mruby exception (message and backtrace) to stderr with
// the given prefix, then clear it.  No-op when no exception is pending.
static void printPendingException(mrb_state *mrb, const char *prefix)
{
	if (!mrb->exc) return;
	mrb_value exc = mrb_obj_value(mrb->exc);
	mrb->exc = nullptr;

	mrb_value msg = mrb_funcall(mrb, exc, "inspect", 0);
	fprintf(stderr, "%s: %s\n", prefix,
		mrb_string_p(msg) ? mrb_string_cstr(mrb, msg) : "(unprintable exception)");

	mrb_value bt = mrb_funcall(mrb, exc, "backtrace", 0);
	if (mrb_array_p(bt)) {
		mrb_int n = RARRAY_LEN(bt);
		for (mrb_int i = 0; i < n; ++i) {
			mrb_value line = mrb_ary_ref(mrb, bt, i);
			if (mrb_string_p(line))
				fprintf(stderr, "    from %s\n",
					mrb_string_cstr(mrb, line));
		}
	}
	mrb->exc = nullptr;	// in case inspect/backtrace raised
}

// Resolve a Ruby value (String / Symbol / Yamy::KeySeq) to a keyseq index.
// A Symbol is treated exactly like the equivalent String: both are parsed
// as action text, where "$Name" refers to a named keyseq and a bare token
// is a key name.
static int resolveRhs(mrb_state *mrb, mrb_value rhs)
{
	if (mrb_string_p(rhs) || mrb_symbol_p(rhs)) {
		std::string actions;
		if (mrb_symbol_p(rhs)) {
			mrb_int len;
			const char *name =
				mrb_sym_name_len(mrb, mrb_symbol(rhs), &len);
			actions.assign(name, static_cast<size_t>(len));
		} else {
			actions = toStdStr(mrb, rhs);
		}
		int idx = ys_reg_keyseq(nullptr, actions.c_str());
		if (idx < 0)
			raiseApiError(mrb, "ys_reg_keyseq failed");
		return idx;
	}

	// Yamy::KeySeq: read @idx
	mrb_value idx_v = mrb_iv_get(mrb, rhs, mrb_intern_lit(mrb, "@idx"));
	if (!mrb_nil_p(idx_v) && mrb_integer_p(idx_v))
		return (int)mrb_integer(idx_v);

	mrb_raise(mrb, E_TYPE_ERROR,
		"rhs must be a String, Symbol, or Yamy::KeySeq");
	return -1;
}

// Build a YsStrs* from an mrb Array of strings or a single string value.
static YsStrs *buildYsStrs(mrb_state *mrb, mrb_value v)
{
	YsStrs *ss = ys_strs_new();
	if (mrb_array_p(v)) {
		mrb_int n = RARRAY_LEN(v);
		for (mrb_int i = 0; i < n; ++i) {
			std::string s = toStdStr(mrb, mrb_ary_ref(mrb, v, i));
			ys_strs_push(ss, s.c_str(), s.size());
		}
	} else {
		std::string s = toStdStr(mrb, v);
		ys_strs_push(ss, s.c_str(), s.size());
	}
	return ss;
}

// Build a YsStrs* for LHS key specs.  A single String is split on whitespace;
// an Array is treated as individual elements.
static YsStrs *buildLhsStrs(mrb_state *mrb, mrb_value lhs)
{
	if (mrb_string_p(lhs)) {
		YsStrs *ss = ys_strs_new();
		std::string s = toStdStr(mrb, lhs);
		const char *p = s.c_str();
		while (*p) {
			while (*p == ' ' || *p == '\t') ++p;
			if (!*p) break;
			const char *start = p;
			while (*p && *p != ' ' && *p != '\t') ++p;
			ys_strs_push(ss, start, (size_t)(p - start));
		}
		return ss;
	}
	return buildYsStrs(mrb, lhs);
}

// Convert a YsFuncArgs* to an mrb Array of Ruby values.
static mrb_value funcArgsToMrb(mrb_state *mrb, const YsFuncArgs *fas)
{
	mrb_value ary = mrb_ary_new(mrb);
	if (!fas) return ary;
	int n = ys_func_args_length(fas);
	for (int i = 0; i < n; ++i) {
		int64_t val = 0, len = 0;
		YsType t = ys_func_args_get(fas, i, &val, &len);
		mrb_value elem = mrb_nil_value();
		switch (t) {
		case YsType_String:
		case YsType_Regexp: {
			const char *p = reinterpret_cast<const char *>((uintptr_t)val);
			elem = mrb_str_new(mrb, p, (mrb_int)len);
			if (t == YsType_Regexp)
				elem = mrb_funcall(mrb, elem, "to_regexp", 0);
			break;
		}
		case YsType_Number:
			elem = mrb_int_value(mrb, (mrb_int)val);
			break;
		case YsType_KeySeqIdx: {
			struct RClass *cls = mrb_class_get_under(mrb,
				mrb_module_get(mrb, "Yamy"), "KeySeq");
			elem = mrb_obj_new(mrb, cls, 0, nullptr);
			mrb_iv_set(mrb, elem, mrb_intern_lit(mrb, "@idx"),
				mrb_int_value(mrb, (mrb_int)val));
			break;
		}
		case YsType_ModifierSpec: {
			struct RClass *cls = mrb_class_get_under(mrb,
				mrb_module_get(mrb, "Yamy"), "Modifier");
			mrb_value args[2] = {
				mrb_int_value(mrb, (mrb_int)(uint64_t)val),
				mrb_int_value(mrb, (mrb_int)(uint64_t)len),
			};
			elem = mrb_obj_new(mrb, cls, 2, args);
			break;
		}
		case YsType_TokenSeq: {
			const YsStrs *ss =
				reinterpret_cast<const YsStrs *>((uintptr_t)val);
			elem = mrb_ary_new(mrb);
			int sn = ys_strs_length(ss);
			for (int j = 0; j < sn; ++j) {
				const char *sp = nullptr; size_t sl = 0;
				ys_strs_get(ss, j, &sp, &sl);
				mrb_ary_push(mrb, elem,
					mrb_str_new(mrb, sp, (mrb_int)sl));
			}
			break;
		}
		default:
			elem = mrb_nil_value();
			break;
		}
		mrb_ary_push(mrb, ary, elem);
	}
	return ary;
}

// Extract a pattern string from a class:/title: keyword value.
// Accepts String or Regexp (.source); returns empty string for nil.
static std::string regexpOrStr(mrb_state *mrb, mrb_value v)
{
	if (mrb_nil_p(v)) return std::string();
	if (mrb_string_p(v)) return toStdStr(mrb, v);
	mrb_value src = mrb_funcall(mrb, v, "source", 0);
	return toStdStr(mrb, src);
}


//=============================================================================
// Yamy::KeySeq  (wraps a keyseq index)
//=============================================================================

static mrb_value keyseq_initialize(mrb_state *mrb, mrb_value self)
{
	mrb_int idx = -1;
	mrb_get_args(mrb, "|i", &idx);
	mrb_iv_set(mrb, self, mrb_intern_lit(mrb, "@idx"),
		mrb_int_value(mrb, idx));
	return self;
}

static mrb_value keyseq_idx(mrb_state *mrb, mrb_value self)
{
	(void)mrb;
	return mrb_iv_get(mrb, self, mrb_intern_lit(mrb, "@idx"));
}



//=============================================================================
// Yamy::KeyMap  (key assignment proxy; used as key[lhs] = rhs)
//=============================================================================

static mrb_value keymap_assign(mrb_state *mrb, mrb_value self)
{
	(void)self;
	mrb_value *argv;
	mrb_int argc;
	mrb_get_args(mrb, "*", &argv, &argc);
	if (argc < 2)
		mrb_raise(mrb, E_ARGUMENT_ERROR,
			"key[lhs] = rhs: wrong number of arguments");

	mrb_value rhs = argv[argc - 1];
	int rhs_idx = resolveRhs(mrb, rhs);

	YsStrs *lhs_ss;
	if (argc == 2) {
		lhs_ss = buildLhsStrs(mrb, argv[0]);
	} else {
		lhs_ss = ys_strs_new();
		for (mrb_int i = 0; i < argc - 1; ++i) {
			std::string s = toStdStr(mrb, argv[i]);
			ys_strs_push(lhs_ss, s.c_str(), s.size());
		}
	}

	bool ok = ys_assign_key(lhs_ss, rhs_idx);
	if (!ok) raiseApiError(mrb, "ys_assign_key failed");
	return rhs;
}


//=============================================================================
// Yamy::EventMap  (event assignment proxy; used as event[name] = rhs)
//=============================================================================

static mrb_value eventmap_assign(mrb_state *mrb, mrb_value self)
{
	(void)self;
	mrb_value name_v, rhs;
	mrb_get_args(mrb, "oo", &name_v, &rhs);
	std::string name = toStdStr(mrb, name_v);
	int rhs_idx = resolveRhs(mrb, rhs);
	if (!ys_assign_event(name.c_str(), rhs_idx))
		raiseApiError(mrb, "ys_assign_event failed");
	return rhs;
}


//=============================================================================
// Yamy::ModValue  (carries op + keys for ModMap operator+/-)
//=============================================================================

static mrb_value modvalue_initialize(mrb_state *mrb, mrb_value self)
{
	mrb_value op_v, keys_v;
	mrb_get_args(mrb, "oo", &op_v, &keys_v);
	mrb_iv_set(mrb, self, mrb_intern_lit(mrb, "@op"),   op_v);
	mrb_iv_set(mrb, self, mrb_intern_lit(mrb, "@keys"), keys_v);
	return self;
}

static mrb_value modvalue_op(mrb_state *mrb, mrb_value self)
{
	(void)mrb;
	return mrb_iv_get(mrb, self, mrb_intern_lit(mrb, "@op"));
}

static mrb_value modvalue_keys(mrb_state *mrb, mrb_value self)
{
	(void)mrb;
	return mrb_iv_get(mrb, self, mrb_intern_lit(mrb, "@keys"));
}

static mrb_value modvalue_plus(mrb_state *mrb, mrb_value self)
{
	mrb_value other;
	mrb_get_args(mrb, "o", &other);
	struct RClass *cls = mrb_obj_class(mrb, self);
	mrb_value args[2] = {
		mrb_str_new_lit(mrb, "+="),
		mrb_array_p(other) ? other : mrb_ary_new_from_values(mrb, 1, &other),
	};
	return mrb_obj_new(mrb, cls, 2, args);
}

static mrb_value modvalue_minus(mrb_state *mrb, mrb_value self)
{
	mrb_value other;
	mrb_get_args(mrb, "o", &other);
	struct RClass *cls = mrb_obj_class(mrb, self);
	mrb_value args[2] = {
		mrb_str_new_lit(mrb, "-="),
		mrb_array_p(other) ? other : mrb_ary_new_from_values(mrb, 1, &other),
	};
	return mrb_obj_new(mrb, cls, 2, args);
}


//=============================================================================
// Yamy::ModMap  (modifier assignment proxy)
//=============================================================================

static mrb_value modmap_initialize(mrb_state *mrb, mrb_value self)
{
	mrb_value prefixes = mrb_nil_value();
	mrb_get_args(mrb, "|o", &prefixes);
	mrb_iv_set(mrb, self, mrb_intern_lit(mrb, "@prefixes"), prefixes);
	return self;
}

// mod[:name] returns a ModValue placeholder (so +/- can be applied to it).
static mrb_value modmap_get(mrb_state *mrb, mrb_value self)
{
	(void)self;
	struct RClass *cls = mrb_class_get_under(mrb,
		mrb_module_get(mrb, "Yamy"), "ModValue");
	mrb_value args[2] = {
		mrb_str_new_lit(mrb, "="),
		mrb_ary_new(mrb),
	};
	return mrb_obj_new(mrb, cls, 2, args);
}

static mrb_value modmap_set(mrb_state *mrb, mrb_value self)
{
	mrb_value name_v, value_v;
	mrb_get_args(mrb, "oo", &name_v, &value_v);

	std::string name = toStdStr(mrb, name_v);

	struct RClass *mv_cls = mrb_class_get_under(mrb,
		mrb_module_get(mrb, "Yamy"), "ModValue");

	// Wrap plain value as "=" ModValue if not already one
	if (!mrb_obj_is_kind_of(mrb, value_v, mv_cls)) {
		mrb_value args[2] = {
			mrb_str_new_lit(mrb, "="),
			mrb_array_p(value_v)
				? value_v
				: mrb_ary_new_from_values(mrb, 1, &value_v),
		};
		value_v = mrb_obj_new(mrb, mv_cls, 2, args);
	}

	std::string op = toStdStr(mrb,
		mrb_iv_get(mrb, value_v, mrb_intern_lit(mrb, "@op")));
	mrb_value keys_v = mrb_iv_get(mrb, value_v,
		mrb_intern_lit(mrb, "@keys"));

	YsStrs *keys_ss = buildYsStrs(mrb, keys_v);

	mrb_value pfx_v = mrb_iv_get(mrb, self, mrb_intern_lit(mrb, "@prefixes"));
	YsStrs *pfx_ss = nullptr;
	if (!mrb_nil_p(pfx_v) && mrb_array_p(pfx_v))
		pfx_ss = buildYsStrs(mrb, pfx_v);

	bool ok = ys_assign_mod(pfx_ss, name.c_str(), op.c_str(), keys_ss);
	if (!ok) raiseApiError(mrb, "ys_assign_mod failed");
	return value_v;
}

// mod.prefix(prefixes) -> new ModMap with the given prefix list
static mrb_value modmap_prefix(mrb_state *mrb, mrb_value self)
{
	(void)self;
	mrb_value pfx;
	mrb_get_args(mrb, "o", &pfx);
	if (!mrb_array_p(pfx))
		pfx = mrb_ary_new_from_values(mrb, 1, &pfx);
	struct RClass *cls = mrb_class_get_under(mrb,
		mrb_module_get(mrb, "Yamy"), "ModMap");
	return mrb_obj_new(mrb, cls, 1, &pfx);
}


//=============================================================================
// Yamy::Modifier  (wraps modifiers + dontcares bitmasks from preset args)
//=============================================================================

static mrb_value modifier_initialize(mrb_state *mrb, mrb_value self)
{
	mrb_int modifiers = 0, dontcares = 0;
	mrb_get_args(mrb, "|ii", &modifiers, &dontcares);
	mrb_iv_set(mrb, self, mrb_intern_lit(mrb, "@modifiers"),
		mrb_int_value(mrb, modifiers));
	mrb_iv_set(mrb, self, mrb_intern_lit(mrb, "@dontcares"),
		mrb_int_value(mrb, dontcares));
	return self;
}


//=============================================================================
// Yamy::DSL  (main DSL object; .rb script is instance_eval'd on it)
//=============================================================================

static mrb_value dsl_load(mrb_state *mrb, mrb_value self)
{
	const char *path_cstr = nullptr;
	mrb_get_args(mrb, "z", &path_cstr);
	std::string path(path_cstr);

	bool is_rb = path.size() >= 3 &&
		path.compare(path.size() - 3, 3, ".rb") == 0;

	if (is_rb) {
		evalRbFile(mrb, self, resolveRbPath(mrb, path));
	} else {
		if (!ys_include_mayu(path.c_str()))
			raiseApiError(mrb, "ys_include_mayu failed");
	}
	return mrb_true_value();
}

static mrb_value dsl_require(mrb_state *mrb, mrb_value self)
{
	const char *name_cstr = nullptr;
	mrb_get_args(mrb, "z", &name_cstr);
	std::string name(name_cstr);
	if (name.size() < 3 || name.compare(name.size() - 3, 3, ".rb") != 0)
		name += ".rb";

	std::string resolved = resolveRbPath(mrb, name);
	mrb_value resolvedStr = mrb_str_new(mrb, resolved.c_str(), resolved.size());

	mrb_value features = mrb_gv_get(mrb,
		mrb_intern_lit(mrb, "$LOADED_FEATURES"));
	if (!mrb_array_p(features)) {
		features = mrb_ary_new(mrb);
		mrb_gv_set(mrb, mrb_intern_lit(mrb, "$LOADED_FEATURES"), features);
	}
	mrb_int n = RARRAY_LEN(features);
	for (mrb_int i = 0; i < n; ++i) {
		if (mrb_str_equal(mrb, mrb_ary_ref(mrb, features, i), resolvedStr))
			return mrb_false_value();
	}

	// Record the feature before evaluating so that circular requires
	// terminate; drop it again when the evaluation raised.
	mrb_ary_push(mrb, features, resolvedStr);
	evalRbFile(mrb, self, resolved);
	if (mrb->exc) {
		mrb_ary_pop(mrb, features);
		return mrb_nil_value();
	}
	return mrb_true_value();
}

static mrb_value dsl_load_mayu(mrb_state *mrb, mrb_value self)
{
	(void)self;
	if (!ys_load_mayu()) raiseApiError(mrb, "ys_load_mayu failed");
	return mrb_true_value();
}

static mrb_value dsl_keyseq(mrb_state *mrb, mrb_value self)
{
	(void)self;
	mrb_value arg1, arg2 = mrb_nil_value();
	mrb_get_args(mrb, "o|o", &arg1, &arg2);

	const char *name    = nullptr;
	const char *actions = nullptr;
	std::string name_s, actions_s;

	if (mrb_nil_p(arg2)) {
		actions_s = toStdStr(mrb, arg1);
		actions   = actions_s.c_str();
	} else {
		// The name must be a String starting with `$' so that the
		// definition matches its "$Name" references in action strings.
		// The `$' is a namespace sigil, not part of the registered name.
		if (!mrb_string_p(arg1))
			mrb_raise(mrb, E_TYPE_ERROR, "keyseq name must be a String");
		name_s = toStdStr(mrb, arg1);
		if (name_s.empty() || name_s[0] != '$')
			mrb_raise(mrb, E_ARGUMENT_ERROR,
				"keyseq name must start with `$' "
				"(e.g. keyseq \"$WindowClose\", ...)");
		name_s.erase(0, 1);
		if (name_s.empty())
			mrb_raise(mrb, E_ARGUMENT_ERROR, "keyseq name is empty");
		name      = name_s.c_str();
		actions_s = toStdStr(mrb, arg2);
		actions   = actions_s.c_str();
	}

	int idx = ys_reg_keyseq(name, actions);
	if (idx < 0) raiseApiError(mrb, "ys_reg_keyseq failed");

	struct RClass *cls = mrb_class_get_under(mrb,
		mrb_module_get(mrb, "Yamy"), "KeySeq");
	mrb_value ks = mrb_obj_new(mrb, cls, 0, nullptr);
	mrb_iv_set(mrb, ks, mrb_intern_lit(mrb, "@idx"),
		mrb_int_value(mrb, (mrb_int)idx));
	return ks;
}

static mrb_value dsl_defkey(mrb_state *mrb, mrb_value self)
{
	(void)self;
	mrb_value *argv;
	mrb_int argc;
	mrb_value kw_hash = mrb_nil_value();
	// "*" swallows the keyword hash as the trailing positional argument
	// (mrb_get_args without ":" packs keywords that way), so peel it off.
	mrb_get_args(mrb, "*", &argv, &argc);
	if (argc > 0 && mrb_hash_p(argv[argc - 1]))
		kw_hash = argv[--argc];

	mrb_value scan_v = mrb_nil_value();
	if (!mrb_nil_p(kw_hash) && mrb_hash_p(kw_hash)) {
		mrb_value k = mrb_symbol_value(mrb_intern_lit(mrb, "scan"));
		scan_v = mrb_hash_get(mrb, kw_hash, k);
	}
	if (mrb_nil_p(scan_v))
		mrb_raise(mrb, E_ARGUMENT_ERROR, "defkey requires scan: keyword");

	YsStrs *names_ss = ys_strs_new();
	for (mrb_int i = 0; i < argc; ++i) {
		if (mrb_array_p(argv[i])) {
			mrb_int n = RARRAY_LEN(argv[i]);
			for (mrb_int j = 0; j < n; ++j) {
				std::string s = toStdStr(mrb,
					mrb_ary_ref(mrb, argv[i], j));
				ys_strs_push(names_ss, s.c_str(), s.size());
			}
		} else {
			std::string s = toStdStr(mrb, argv[i]);
			ys_strs_push(names_ss, s.c_str(), s.size());
		}
	}

	YsStrs *scan_ss = buildYsStrs(mrb, scan_v);
	bool ok = ys_def_key(names_ss, scan_ss);
	if (!ok) raiseApiError(mrb, "ys_def_key failed");
	return mrb_true_value();
}

static mrb_value dsl_defmod(mrb_state *mrb, mrb_value self)
{
	(void)self;
	mrb_value name_v, kw_hash = mrb_nil_value();
	mrb_get_args(mrb, "o|H", &name_v, &kw_hash);

	mrb_value keys_v = mrb_nil_value();
	if (!mrb_nil_p(kw_hash) && mrb_hash_p(kw_hash)) {
		mrb_value k = mrb_symbol_value(mrb_intern_lit(mrb, "keys"));
		keys_v = mrb_hash_get(mrb, kw_hash, k);
	}
	if (mrb_nil_p(keys_v))
		mrb_raise(mrb, E_ARGUMENT_ERROR, "defmod requires keys: keyword");

	std::string name = toStdStr(mrb, name_v);
	YsStrs *keys_ss  = buildYsStrs(mrb, keys_v);
	bool ok = ys_def_mod(name.c_str(), keys_ss);
	if (!ok) raiseApiError(mrb, "ys_def_mod failed");
	return mrb_true_value();
}

static mrb_value dsl_defsync(mrb_state *mrb, mrb_value self)
{
	(void)self;
	mrb_value v;
	mrb_get_args(mrb, "o", &v);
	YsStrs *ss = buildYsStrs(mrb, v);
	bool ok = ys_def_sync(ss);
	if (!ok) raiseApiError(mrb, "ys_def_sync failed");
	return mrb_true_value();
}

static mrb_value dsl_defalias(mrb_state *mrb, mrb_value self)
{
	(void)self;
	mrb_value alias_v, kw_hash = mrb_nil_value();
	mrb_get_args(mrb, "o|H", &alias_v, &kw_hash);

	mrb_value as_v = mrb_nil_value();
	if (!mrb_nil_p(kw_hash) && mrb_hash_p(kw_hash)) {
		mrb_value k = mrb_symbol_value(mrb_intern_lit(mrb, "as"));
		as_v = mrb_hash_get(mrb, kw_hash, k);
	}
	if (mrb_nil_p(as_v))
		mrb_raise(mrb, E_ARGUMENT_ERROR, "defalias requires as: keyword");

	std::string alias_name = toStdStr(mrb, alias_v);
	std::string key_name   = toStdStr(mrb, as_v);
	if (!ys_def_alias(alias_name.c_str(), key_name.c_str()))
		raiseApiError(mrb, "ys_def_alias failed");
	return mrb_true_value();
}

static mrb_value dsl_defsubst(mrb_state *mrb, mrb_value self)
{
	(void)self;
	mrb_value lhs_v, kw_hash = mrb_nil_value();
	mrb_get_args(mrb, "o|H", &lhs_v, &kw_hash);

	mrb_value to_v = mrb_nil_value();
	if (!mrb_nil_p(kw_hash) && mrb_hash_p(kw_hash)) {
		mrb_value k = mrb_symbol_value(mrb_intern_lit(mrb, "to"));
		to_v = mrb_hash_get(mrb, kw_hash, k);
	}
	if (mrb_nil_p(to_v))
		mrb_raise(mrb, E_ARGUMENT_ERROR, "defsubst requires to: keyword");

	int rhs_idx = resolveRhs(mrb, to_v);
	YsStrs *lhs_ss = buildLhsStrs(mrb, lhs_v);
	bool ok = ys_def_subst(lhs_ss, rhs_idx);
	if (!ok) raiseApiError(mrb, "ys_def_subst failed");
	return mrb_true_value();
}

static mrb_value dsl_defoption(mrb_state *mrb, mrb_value self)
{
	(void)self;
	mrb_value name_v, kw_hash = mrb_nil_value();
	mrb_get_args(mrb, "o|H", &name_v, &kw_hash);

	mrb_value val_v = mrb_nil_value();
	if (!mrb_nil_p(kw_hash) && mrb_hash_p(kw_hash)) {
		mrb_value k = mrb_symbol_value(mrb_intern_lit(mrb, "value"));
		val_v = mrb_hash_get(mrb, kw_hash, k);
	}
	if (mrb_nil_p(val_v))
		mrb_raise(mrb, E_ARGUMENT_ERROR, "defoption requires value: keyword");

	std::string name = toStdStr(mrb, name_v);
	std::string val  = toStdStr(mrb, mrb_funcall(mrb, val_v, "to_s", 0));
	if (!ys_def_option(name.c_str(), val.c_str()))
		raiseApiError(mrb, "ys_def_option failed");
	return mrb_true_value();
}

// Shared implementation for keymap / keymap2 / window.
static mrb_value dsl_begin_keymap(mrb_state *mrb, mrb_value self,
	const char *keyword)
{
	mrb_value name_v, kw_hash = mrb_nil_value(), blk = mrb_nil_value();
	mrb_get_args(mrb, "o|H&", &name_v, &kw_hash, &blk);

	std::string name = toStdStr(mrb, name_v);

	auto getKw = [&](const char *sym_name) -> mrb_value {
		if (mrb_nil_p(kw_hash) || !mrb_hash_p(kw_hash))
			return mrb_nil_value();
		mrb_value k = mrb_symbol_value(mrb_intern_cstr(mrb, sym_name));
		return mrb_hash_get(mrb, kw_hash, k);
	};

	mrb_value class_v  = getKw("class");
	mrb_value title_v  = getKw("title");
	mrb_value op_v     = getKw("op");
	mrb_value parent_v = getKw("parent");
	mrb_value default_v = getKw("default");

	std::string class_s  = regexpOrStr(mrb, class_v);
	std::string title_s  = regexpOrStr(mrb, title_v);
	std::string op_s     = !mrb_nil_p(op_v)     ? toStdStr(mrb, op_v)     : std::string();
	std::string parent_s = !mrb_nil_p(parent_v) ? toStdStr(mrb, parent_v) : std::string();

	int default_idx = -1;
	if (!mrb_nil_p(default_v))
		default_idx = resolveRhs(mrb, default_v);

	if (!ys_begin_keymap(
			keyword,
			name.c_str(),
			class_s.empty()  ? nullptr : class_s.c_str(),
			title_s.empty()  ? nullptr : title_s.c_str(),
			op_s.empty()     ? nullptr : op_s.c_str(),
			parent_s.empty() ? nullptr : parent_s.c_str(),
			default_idx))
		raiseApiError(mrb, "ys_begin_keymap failed");

	if (!mrb_nil_p(blk) && mrb_proc_p(blk))
		mrb_funcall_with_block(mrb, self,
			mrb_intern_lit(mrb, "instance_eval"), 0, nullptr, blk);

	return mrb_true_value();
}

static mrb_value dsl_keymap(mrb_state *mrb, mrb_value self)
{
	return dsl_begin_keymap(mrb, self, "keymap");
}

static mrb_value dsl_keymap2(mrb_state *mrb, mrb_value self)
{
	return dsl_begin_keymap(mrb, self, "keymap2");
}

static mrb_value dsl_window(mrb_state *mrb, mrb_value self)
{
	return dsl_begin_keymap(mrb, self, "window");
}

// key  -> Yamy::KeyMap singleton on the DSL instance
static mrb_value dsl_key(mrb_state *mrb, mrb_value self)
{
	mrb_sym iv = mrb_intern_lit(mrb, "@__keymap__");
	mrb_value km = mrb_iv_get(mrb, self, iv);
	if (mrb_nil_p(km)) {
		struct RClass *cls = mrb_class_get_under(mrb,
			mrb_module_get(mrb, "Yamy"), "KeyMap");
		km = mrb_obj_new(mrb, cls, 0, nullptr);
		mrb_iv_set(mrb, self, iv, km);
	}
	return km;
}

// event -> Yamy::EventMap singleton on the DSL instance
static mrb_value dsl_event(mrb_state *mrb, mrb_value self)
{
	mrb_sym iv = mrb_intern_lit(mrb, "@__eventmap__");
	mrb_value em = mrb_iv_get(mrb, self, iv);
	if (mrb_nil_p(em)) {
		struct RClass *cls = mrb_class_get_under(mrb,
			mrb_module_get(mrb, "Yamy"), "EventMap");
		em = mrb_obj_new(mrb, cls, 0, nullptr);
		mrb_iv_set(mrb, self, iv, em);
	}
	return em;
}

// mod -> Yamy::ModMap singleton on the DSL instance (no prefixes)
static mrb_value dsl_mod(mrb_state *mrb, mrb_value self)
{
	mrb_sym iv = mrb_intern_lit(mrb, "@__modmap__");
	mrb_value mm = mrb_iv_get(mrb, self, iv);
	if (mrb_nil_p(mm)) {
		struct RClass *cls = mrb_class_get_under(mrb,
			mrb_module_get(mrb, "Yamy"), "ModMap");
		mm = mrb_obj_new(mrb, cls, 0, nullptr);
		mrb_iv_set(mrb, self, iv, mm);
	}
	return mm;
}

// deffunc(name) { |trigger, *args| ... }
static mrb_value dsl_deffunc(mrb_state *mrb, mrb_value self)
{
	(void)self;
	mrb_value name_v, blk = mrb_nil_value();
	mrb_get_args(mrb, "o&", &name_v, &blk);

	std::string func_name = toStdStr(mrb, name_v);

	if (!ys_reg_user_func(func_name.c_str(), mruby_on_exec_user_func))
		raiseApiError(mrb, "ys_reg_user_func failed");

	if (!mrb_nil_p(blk) && mrb_proc_p(blk)) {
		mrb_value key = mrb_str_new(mrb, func_name.c_str(),
			func_name.size());
		mrb_hash_set(mrb, g_funcTable, key, blk);
	}
	return mrb_true_value();
}

// DSL#define(name)  ->  define <name>   (adds symbol to the current set)
static mrb_value dsl_define(mrb_state *mrb, mrb_value self)
{
	(void)self;
	mrb_value name_v;
	mrb_get_args(mrb, "o", &name_v);
	std::string name = toStdStr(mrb, name_v);
	if (!ys_define_symbol(name.c_str()))
		raiseApiError(mrb, "ys_define_symbol failed");
	return mrb_true_value();
}

// DSL#symbol_defined?(name)  ->  true if <name> is defined   (mirrors .mayu `if (name)`)
static mrb_value dsl_symbol_defined_p(mrb_state *mrb, mrb_value self)
{
	(void)self;
	mrb_value name_v;
	mrb_get_args(mrb, "o", &name_v);
	std::string name = toStdStr(mrb, name_v);
	return mrb_bool_value(ys_has_symbol(name.c_str()));
}

// DSL#exec_keyseq(actions)  (runtime API; valid from on_exec_user_func)
static mrb_value dsl_exec_keyseq(mrb_state *mrb, mrb_value self)
{
	(void)self;
	const char *actions = nullptr;
	mrb_get_args(mrb, "z", &actions);
	return mrb_bool_value(ys_exec_keyseq(actions));
}

// Yamy.last_error  (module-level method)
static mrb_value yamy_last_error(mrb_state *mrb, mrb_value)
{
	(void)mrb;
	const char *msg = ys_last_error();
	if (!msg) return mrb_nil_value();
	return mrb_str_new_cstr(mrb, msg);
}

// Resolve a scan-code argument (Integer or key/scan-code String) to a WORD
// value in the range 0x00-0xFF / 0xE000-0xE1FF, or raise ArgumentError.
// Integer: range-checked and returned as is.  Symbols / other objects are
// coerced to string (matching the DSL's symbol-as-string convention).
static int scResolveArgOrRaise(mrb_state *mrb, mrb_value v)
{
	if (mrb_integer_p(v)) {
		mrb_int n = mrb_integer(v);
		bool inRange = (n >= 0 && n <= 0xFF) || (n >= 0xE000 && n <= 0xE1FF);
		if (!inRange)
			mrb_raisef(mrb, E_ARGUMENT_ERROR,
				"scan code out of range: 0x%x "
				"(expected 0x00-0xFF, 0xE000-0xE1FF)", (int)n);
		return (int)n;
	}
	std::string s = toStdStr(mrb, v);
	int word = ys_sc_resolve(s.c_str());
	if (word < 0)
		mrb_raisef(mrb, E_ARGUMENT_ERROR,
			"unknown key name or scan code: %s", s.c_str());
	return word;
}

// DSL#sc(key_or_scancode)  ->  scan-code integer (0x00-0xFF / 0xE000-0xE1FF)
static mrb_value dsl_sc(mrb_state *mrb, mrb_value self)
{
	(void)self;
	mrb_value v;
	mrb_get_args(mrb, "o", &v);
	return mrb_int_value(mrb, scResolveArgOrRaise(mrb, v));
}

// ScancodeMap[from] / ScancodeMap.from(from)  ->  remapped scan code or nil
static mrb_value scancodemap_from(mrb_state *mrb, mrb_value self)
{
	(void)self;
	mrb_value v;
	mrb_get_args(mrb, "o", &v);
	int from = scResolveArgOrRaise(mrb, v);
	int n = ys_scancode_map_length();
	for (int i = 0; i < n; ++i) {
		unsigned f = 0, t = 0;
		if (ys_scancode_map_entry(i, &f, &t) && (int)f == from)
			return mrb_int_value(mrb, (mrb_int)t);
	}
	return mrb_nil_value();
}

// ScancodeMap.to(to)  ->  [original scan code, ...]  (empty when unmapped)
static mrb_value scancodemap_to(mrb_state *mrb, mrb_value self)
{
	(void)self;
	mrb_value v;
	mrb_get_args(mrb, "o", &v);
	int to = scResolveArgOrRaise(mrb, v);
	mrb_value ary = mrb_ary_new(mrb);
	int n = ys_scancode_map_length();
	for (int i = 0; i < n; ++i) {
		unsigned f = 0, t = 0;
		if (ys_scancode_map_entry(i, &f, &t) && (int)t == to)
			mrb_ary_push(mrb, ary, mrb_int_value(mrb, (mrb_int)f));
	}
	return ary;
}


//=============================================================================
// yamy_mruby_init_internal  (static; called from mruby_on_load_setting)
//=============================================================================

static void yamy_mruby_init_internal(mrb_state *mrb)
{
	struct RClass *yamy = mrb_define_module(mrb, "Yamy");
	mrb_define_module_function(mrb, yamy, "last_error",
		yamy_last_error, MRB_ARGS_NONE());

	struct RClass *ks_cls = mrb_define_class_under(mrb, yamy, "KeySeq",
		mrb->object_class);
	mrb_define_method(mrb, ks_cls, "initialize",
		keyseq_initialize, MRB_ARGS_OPT(1));
	mrb_define_method(mrb, ks_cls, "idx", keyseq_idx, MRB_ARGS_NONE());

	struct RClass *km_cls = mrb_define_class_under(mrb, yamy, "KeyMap",
		mrb->object_class);
	mrb_define_method(mrb, km_cls, "[]=", keymap_assign, MRB_ARGS_ANY());

	struct RClass *em_cls = mrb_define_class_under(mrb, yamy, "EventMap",
		mrb->object_class);
	mrb_define_method(mrb, em_cls, "[]=", eventmap_assign, MRB_ARGS_REQ(2));

	struct RClass *mv_cls = mrb_define_class_under(mrb, yamy, "ModValue",
		mrb->object_class);
	mrb_define_method(mrb, mv_cls, "initialize",
		modvalue_initialize, MRB_ARGS_REQ(2));
	mrb_define_method(mrb, mv_cls, "op",   modvalue_op,    MRB_ARGS_NONE());
	mrb_define_method(mrb, mv_cls, "keys", modvalue_keys,  MRB_ARGS_NONE());
	mrb_define_method(mrb, mv_cls, "+",    modvalue_plus,  MRB_ARGS_REQ(1));
	mrb_define_method(mrb, mv_cls, "-",    modvalue_minus, MRB_ARGS_REQ(1));

	struct RClass *mm_cls = mrb_define_class_under(mrb, yamy, "ModMap",
		mrb->object_class);
	mrb_define_method(mrb, mm_cls, "initialize",
		modmap_initialize, MRB_ARGS_OPT(1));
	mrb_define_method(mrb, mm_cls, "[]",     modmap_get,    MRB_ARGS_REQ(1));
	mrb_define_method(mrb, mm_cls, "[]=",    modmap_set,    MRB_ARGS_REQ(2));
	mrb_define_method(mrb, mm_cls, "prefix", modmap_prefix, MRB_ARGS_REQ(1));

	struct RClass *mod_cls = mrb_define_class_under(mrb, yamy, "Modifier",
		mrb->object_class);
	mrb_define_method(mrb, mod_cls, "initialize",
		modifier_initialize, MRB_ARGS_OPT(2));

	struct RClass *dsl_cls = mrb_define_class_under(mrb, yamy, "DSL",
		mrb->object_class);
	mrb_define_method(mrb, dsl_cls, "load",      dsl_load,     MRB_ARGS_REQ(1));
	mrb_define_method(mrb, dsl_cls, "require",   dsl_require,  MRB_ARGS_REQ(1));
	mrb_define_method(mrb, dsl_cls, "load_mayu", dsl_load_mayu, MRB_ARGS_NONE());
	mrb_define_method(mrb, dsl_cls, "keyseq",    dsl_keyseq,   MRB_ARGS_ARG(1, 1));
	mrb_define_method(mrb, dsl_cls, "defkey",    dsl_defkey,   MRB_ARGS_ANY());
	mrb_define_method(mrb, dsl_cls, "defmod",    dsl_defmod,   MRB_ARGS_ARG(1, 1));
	mrb_define_method(mrb, dsl_cls, "defsync",   dsl_defsync,  MRB_ARGS_REQ(1));
	mrb_define_method(mrb, dsl_cls, "defalias",  dsl_defalias, MRB_ARGS_ARG(1, 1));
	mrb_define_method(mrb, dsl_cls, "defsubst",  dsl_defsubst, MRB_ARGS_ARG(1, 1));
	mrb_define_method(mrb, dsl_cls, "defoption", dsl_defoption, MRB_ARGS_ARG(1, 1));
	mrb_define_method(mrb, dsl_cls, "keymap",    dsl_keymap,   MRB_ARGS_ANY());
	mrb_define_method(mrb, dsl_cls, "keymap2",   dsl_keymap2,  MRB_ARGS_ANY());
	mrb_define_method(mrb, dsl_cls, "window",    dsl_window,   MRB_ARGS_ANY());
	mrb_define_method(mrb, dsl_cls, "key",       dsl_key,      MRB_ARGS_NONE());
	mrb_define_method(mrb, dsl_cls, "event",     dsl_event,    MRB_ARGS_NONE());
	mrb_define_method(mrb, dsl_cls, "mod",       dsl_mod,      MRB_ARGS_NONE());
	mrb_define_method(mrb, dsl_cls, "define",          dsl_define,          MRB_ARGS_REQ(1));
	mrb_define_method(mrb, dsl_cls, "symbol_defined?", dsl_symbol_defined_p, MRB_ARGS_REQ(1));
	mrb_define_method(mrb, dsl_cls, "deffunc",     dsl_deffunc,     MRB_ARGS_ANY());
	mrb_define_method(mrb, dsl_cls, "exec_keyseq", dsl_exec_keyseq, MRB_ARGS_REQ(1));
	mrb_define_method(mrb, dsl_cls, "sc",          dsl_sc,          MRB_ARGS_REQ(1));

	// ScancodeMap: read-only view of the registry Scancode Map (top-level module).
	struct RClass *scmap = mrb_define_module(mrb, "ScancodeMap");
	mrb_define_module_function(mrb, scmap, "[]",   scancodemap_from, MRB_ARGS_REQ(1));
	mrb_define_module_function(mrb, scmap, "from", scancodemap_from, MRB_ARGS_REQ(1));
	mrb_define_module_function(mrb, scmap, "to",   scancodemap_to,   MRB_ARGS_REQ(1));

	// GC-protect the func table hash as a module constant
	g_funcTable = mrb_hash_new(mrb);
	mrb_define_const(mrb, yamy, "FUNC_TABLE", g_funcTable);
}


//=============================================================================
// mruby_on_load_setting
//=============================================================================

bool mruby_on_load_setting(void* exeCtx)
{
	MRubyContext *ctx = static_cast<MRubyContext *>(exeCtx);

	// 1. Resolve script path.
	//    If argv[1] is supplied use it; otherwise search home directories for .mayu.rb.
	const char *script = nullptr;
	int argv_start = 2;
	std::string scriptStorage;

	if (ctx->argc >= 2) {
		script = ctx->argv[1];
	} else {
		const char *found = nullptr;
		if (ys_resolve_config_path(".mayu.rb", &found) && found) {
			scriptStorage = found;
			script = scriptStorage.c_str();
		}
		argv_start = 1;
	}

	if (!script) {
		fprintf(stderr,
			"error: script file required (or place .mayu.rb in a home directory)\n"
			"usage: yamy-scripter.exe path.rb [args...]\n");
		return false;
	}

	// 2. Open mruby state and register DSL classes.
	mrb_state *mrb = mrb_open();
	if (!mrb) {
		fprintf(stderr, "error: failed to initialise mruby\n");
		return false;
	}
	ctx->mrb = mrb;
	yamy_mruby_init_internal(mrb);

	// 3. Set $LOAD_PATH to the script's directory followed by the home
	//    directories, and $LOADED_FEATURES to an empty array.  DSL#load and
	//    DSL#require search $LOAD_PATH when given a relative .rb path.
	{
		mrb_value load_path = mrb_ary_new(mrb);

		std::string scriptDir;
		std::string scriptAbs =
			canonicalizePath(utf8ToWide(script, strlen(script)));
		size_t sep = scriptAbs.find_last_of("\\/");
		if (sep != std::string::npos && sep > 0) {
			scriptDir = scriptAbs.substr(0, sep);
			mrb_ary_push(mrb, load_path,
				mrb_str_new(mrb, scriptDir.c_str(), scriptDir.size()));
		}

		YsStrs *dirs = ys_get_home_directories();
		int n = dirs ? ys_strs_length(dirs) : 0;
		for (int i = 0; i < n; ++i) {
			const char *p = nullptr; size_t len = 0;
			ys_strs_get(dirs, i, &p, &len);
			if (p && len > 0 && scriptDir.compare(0, scriptDir.size(),
					p, len) != 0)
				mrb_ary_push(mrb, load_path,
					mrb_str_new(mrb, p, (mrb_int)len));
		}
		mrb_gv_set(mrb, mrb_intern_cstr(mrb, "$LOAD_PATH"), load_path);
		mrb_gv_set(mrb, mrb_intern_cstr(mrb, "$LOADED_FEATURES"),
			mrb_ary_new(mrb));
	}

	// 4. Set $0 to the script path.
	mrb_gv_set(mrb, mrb_intern_cstr(mrb, "$0"),
		mrb_str_new_cstr(mrb, script));

	// 5. Set ARGV constant from arguments after the script name.
	{
		mrb_value argv_val = mrb_ary_new(mrb);
		for (int i = argv_start; i < ctx->argc; ++i)
			mrb_ary_push(mrb, argv_val, mrb_str_new_cstr(mrb, ctx->argv[i]));
		mrb_define_global_const(mrb, "ARGV", argv_val);
	}

	// 6. Reset the func table for this load cycle.
	g_funcTable = mrb_hash_new(mrb);
	struct RClass *yamy = mrb_module_get(mrb, "Yamy");
	mrb_define_const(mrb, yamy, "FUNC_TABLE", g_funcTable);

	// 7. Evaluate the script on a fresh Yamy::DSL instance so that the DSL
	//    methods (keymap, key, event, mod, symbol_defined?, define, load, ...) are in
	//    scope for the top-level script.  DSL#load instance_eval's a .rb file
	//    on the same object (and compiles a .mayu file via ys_include_mayu).
	{
		struct RClass *dsl_cls = mrb_class_get_under(mrb,
			mrb_module_get(mrb, "Yamy"), "DSL");
		mrb_value dsl = mrb_obj_new(mrb, dsl_cls, 0, nullptr);
		mrb_funcall(mrb, dsl, "load", 1, mrb_str_new_cstr(mrb, script));
	}

	if (mrb->exc) {
		printPendingException(mrb, "[mruby] on_load_setting error");
		return false;
	}
	return true;
}


//=============================================================================
// mruby_on_quit
//=============================================================================

void mruby_on_quit(void *exeCtx)
{
	MRubyContext *ctx = static_cast<MRubyContext *>(exeCtx);
	if (ctx->mrb) {
		mrb_close(ctx->mrb);
		ctx->mrb = nullptr;
	}
}


//=============================================================================
// mruby_on_exec_user_func
//=============================================================================

void mruby_on_exec_user_func(void *exeCtx, const char *func_name,
	const YsFuncArgs *args)
{
	MRubyContext *ctx = static_cast<MRubyContext *>(exeCtx);
	if (!ctx->mrb) return;
	mrb_state *mrb = ctx->mrb;

	mrb_value key = mrb_str_new_cstr(mrb, func_name);
	mrb_value blk = mrb_hash_get(mrb, g_funcTable, key);
	if (mrb_nil_p(blk)) {
		fprintf(stderr, "[mruby] no handler registered for func: %s\n",
			func_name);
		return;
	}

	mrb_value args_ary = funcArgsToMrb(mrb, args);
	mrb_int n = (mrb_int)RARRAY_LEN(args_ary);
	mrb_value *call_args = new mrb_value[n];
	for (mrb_int i = 0; i < n; ++i)
		call_args[i] = mrb_ary_ref(mrb, args_ary, i);

	mrb_yield_argv(mrb, blk, n, call_args);
	delete[] call_args;

	if (mrb->exc) {
		std::string prefix = std::string("[mruby] exec_user_func error (")
			+ func_name + ")";
		printPendingException(mrb, prefix.c_str());
	}
}
