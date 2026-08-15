//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// mruby_binding.cpp
//
// mruby DSL binding for nyamy-scripter.exe (mruby variant).
// Implements NYamy::DSL, NYamy::KeySeq, NYamy::KeyMap,
// NYamy::EventMap, NYamy::ModMap, NYamy::ModValue, and NYamy::Modifier.
//
// The .rb script is evaluated via instance_eval on a NYamy::DSL object
// each time on_load_setting is called.

#include "mruby_binding.h"
#include "nyamy_scripter.h"
#include "cli_options.h"		// normalizeDirectory / splitDirectoryList
#include "../stringtool.h"		// wregex_stored

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
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <windows.h>


//=============================================================================
// Logging helpers
//=============================================================================

// Render a Ruby value the way the script author wrote it.
static std::string inspectToUtf8(mrb_state *mrb, mrb_value v)
{
	mrb_value s = mrb_funcall(mrb, v, "inspect", 0);
	if (mrb->exc) {
		mrb->exc = nullptr;	// inspect is best effort
		return "?";
	}
	if (!mrb_string_p(s))
		return "?";
	return std::string(RSTRING_PTR(s), RSTRING_LEN(s));
}

/** Trace one DSL call at debug level, showing the arguments as written.

    Nothing is built unless detail logging is on, since this sits on every
    definition in the configuration.  The arguments are copied out of the VM
    stack before the first inspect call: that call can grow the stack and move
    the slots argv points at, but the mrb_values themselves stay valid.
*/
static void traceDslCall(mrb_state *mrb, const char *i_name)
{
	if (!nysWouldLog(LogLevel::Debug))
		return;

	mrb_value *argv = nullptr;
	mrb_int argc = 0;
	mrb_get_args(mrb, "*", &argv, &argc);
	std::vector<mrb_value> args(argv, argv + argc);

	std::string line = i_name;
	for (size_t i = 0; i < args.size(); ++i) {
		line += (i == 0) ? " " : ", ";
		line += inspectToUtf8(mrb, args[i]);
	}
	nysLogUtf8(LogLevel::Debug, line.c_str());
}


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
	const char *msg = nys_last_error();
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
	// not reached
}

// Evaluate the .rb file at the canonical absolute path via instance_eval
// on the DSL object.  A pending exception (mrb->exc) is left to the caller.
static void evalRbFile(mrb_state *mrb, mrb_value self, const std::string &absPath)
{
	// The .mayu path reports every file it opens; without this a .rb-only
	// configuration left no trace of what had actually been read.
	nysLogUtf8(LogLevel::Info, ("  loading: " + absPath).c_str());

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
	nysLogUtf8(LogLevel::Error,
			   (std::string(prefix) + ": " +
				(mrb_string_p(msg) ? mrb_string_cstr(mrb, msg)
				 : "(unprintable exception)")).c_str());

	mrb_value bt = mrb_funcall(mrb, exc, "backtrace", 0);
	if (mrb_array_p(bt)) {
		mrb_int n = RARRAY_LEN(bt);
		for (mrb_int i = 0; i < n; ++i) {
			mrb_value line = mrb_ary_ref(mrb, bt, i);
			if (mrb_string_p(line))
				nysLogUtf8(LogLevel::Error,
						   (std::string("    from ") +
							mrb_string_cstr(mrb, line)).c_str());
		}
	}
	mrb->exc = nullptr;	// in case inspect/backtrace raised
}

// Resolve a Ruby value (String / Symbol / NYamy::KeySeq) to a keyseq index.
// A Symbol is treated exactly like the equivalent String: both are parsed
// as action text, where "$Name" refers to a named keyseq and a bare token
// is a key name.
// context is one of NYS_MODCTX_*: it decides which modifiers the action text
// may carry (see nys_reg_keyseq).
static int resolveRhs(mrb_state *mrb, mrb_value rhs,
					  int context = NYS_MODCTX_KEYSEQ)
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
		int idx = nys_reg_keyseq(nullptr, actions.c_str(), context);
		if (idx < 0)
			raiseApiError(mrb, "nys_reg_keyseq failed");
		return idx;
	}

	// NYamy::KeySeq: read @idx
	mrb_value idx_v = mrb_iv_get(mrb, rhs, mrb_intern_lit(mrb, "@idx"));
	if (!mrb_nil_p(idx_v) && mrb_integer_p(idx_v))
		return (int)mrb_integer(idx_v);

	mrb_raise(mrb, E_TYPE_ERROR,
		"rhs must be a String, Symbol, or NYamy::KeySeq");
	// not reached
}

// Build a NYsStrs* from an mrb Array of strings or a single string value.
static NYsStrs *buildYsStrs(mrb_state *mrb, mrb_value v)
{
	NYsStrs *ss = nys_strs_new();
	if (mrb_array_p(v)) {
		mrb_int n = RARRAY_LEN(v);
		for (mrb_int i = 0; i < n; ++i) {
			std::string s = toStdStr(mrb, mrb_ary_ref(mrb, v, i));
			nys_strs_push(ss, s.c_str(), s.size());
		}
	} else {
		std::string s = toStdStr(mrb, v);
		nys_strs_push(ss, s.c_str(), s.size());
	}
	return ss;
}

// Build a NYsStrs* for LHS key specs.  A single String is split on whitespace;
// an Array is treated as individual elements.
static NYsStrs *buildLhsStrs(mrb_state *mrb, mrb_value lhs)
{
	if (mrb_string_p(lhs)) {
		NYsStrs *ss = nys_strs_new();
		std::string s = toStdStr(mrb, lhs);
		const char *p = s.c_str();
		while (*p) {
			while (*p == ' ' || *p == '\t') ++p;
			if (!*p) break;
			const char *start = p;
			while (*p && *p != ' ' && *p != '\t') ++p;
			nys_strs_push(ss, start, (size_t)(p - start));
		}
		return ss;
	}
	return buildYsStrs(mrb, lhs);
}

//=============================================================================
// Regexp / MatchData
//
// mruby has no Regexp class, but its parser does understand /.../ literals:
// the code generator emits ::Regexp.compile(source, flags, encoding) for one.
// The real classes live under NYamy like every other binding class, and the
// top-level constants are aliases (see nyamy_mruby_init_internal).
//
// Matching is std::wregex, the same engine and character type nyamy itself
// uses for window class / title patterns, so a pattern that compiles here
// behaves the same way there.
//=============================================================================

// Option bits.  These are the values CRuby's Regexp constants use, which are
// also the ones mruby's lexer builds internally before flattening them into
// the flag string handed to Regexp.compile.
enum {
	RE_IGNORECASE		= 1,
	RE_EXTENDED			= 2,
	RE_MULTILINE		= 4,
	RE_FIXEDENCODING	= 16,
	RE_NOENCODING		= 32,
};

// Options that cannot survive the trip to the engine: the wire format carries
// the pattern string alone, and window matching is always case-insensitive.
// EXTENDED is absent on purpose - it is folded into the pattern itself.
static const uint32_t RE_NOT_ON_WIRE =
	RE_IGNORECASE | RE_MULTILINE | RE_FIXEDENCODING | RE_NOENCODING;

struct RegexpData {
	wregex_stored	re;			///< compiled, plus the pattern it came from
	std::wstring	source;		///< as written in the literal
	uint32_t		options;	///< RE_* bits
};

struct MatchDataData {
	struct Group {
		bool	matched;
		size_t	begin;			///< offset into subject, in wchar_t units
		size_t	end;
	};
	std::wstring		subject;
	std::vector<Group>	groups;
};

static void regexpDataFree(mrb_state *, void *p)
{
	delete static_cast<RegexpData *>(p);
}

static void matchDataFree(mrb_state *, void *p)
{
	delete static_cast<MatchDataData *>(p);
}

static const mrb_data_type g_regexpDataType = { "NYamy::Regexp", regexpDataFree };
static const mrb_data_type g_matchDataType  = { "NYamy::MatchData", matchDataFree };

// Sources already reported by noticeMultiline(), so that a literal inside a
// loop does not repeat its notice.  Cleared at the start of every load.
static std::unordered_set<std::wstring> g_multilineNoticed;

static struct RClass *regexpClass(mrb_state *mrb)
{
	return mrb_class_get_under(mrb, mrb_module_get(mrb, "NYamy"), "Regexp");
}

// Null unless the value is one of ours; never raises.
static RegexpData *regexpPtr(mrb_state *mrb, mrb_value v)
{
	return static_cast<RegexpData *>(
		mrb_data_check_get_ptr(mrb, v, &g_regexpDataType));
}

static RegexpData *regexpPtrOrRaise(mrb_state *mrb, mrb_value v)
{
	return static_cast<RegexpData *>(
		mrb_data_get_ptr(mrb, v, &g_regexpDataType));
}

static MatchDataData *matchPtrOrRaise(mrb_state *mrb, mrb_value v)
{
	return static_cast<MatchDataData *>(
		mrb_data_get_ptr(mrb, v, &g_matchDataType));
}

static mrb_value wideToStr(mrb_state *mrb, const std::wstring &w)
{
	std::string s = wideToUtf8(w);
	return mrb_str_new(mrb, s.c_str(), (mrb_int)s.size());
}

/** Apply the /x (extended) transformation to a pattern.

    Unescaped whitespace and unescaped '#'-to-end-of-line comments are dropped.
    An escaped character is copied through as a pair, and the contents of a
    [...] character class are left completely alone: whitespace and '#' keep
    their meaning in there.  ECMAScript requires ']' inside a class to be
    escaped, so the Ruby "[]]" special case cannot arise.
*/
static std::wstring applyExtended(const std::wstring &i_src)
{
	std::wstring out;
	out.reserve(i_src.size());
	bool inClass = false;

	for (size_t i = 0; i < i_src.size(); ++i) {
		wchar_t c = i_src[i];

		if (c == L'\\') {
			out += c;
			if (i + 1 < i_src.size())
				out += i_src[++i];
			continue;
		}
		if (inClass) {
			out += c;
			if (c == L']')
				inClass = false;
			continue;
		}
		if (c == L'[') {
			inClass = true;
			out += c;
			continue;
		}
		if (c == L' ' || c == L'\t' || c == L'\n' || c == L'\r' ||
			c == L'\f' || c == L'\v')
			continue;
		if (c == L'#') {
			while (i + 1 < i_src.size() && i_src[i + 1] != L'\n')
				++i;
			continue;
		}
		out += c;
	}
	return out;
}

// Decode the flag string ("ixm", as mruby's code generator spells it) and the
// separate encoding argument ("u" / "n") into RE_* bits.  This is the only
// place flag letters are interpreted, so it is also where a bad one is caught.
static uint32_t regexpOptionsFromChars(mrb_state *mrb, const char *s, size_t len)
{
	uint32_t o = 0;
	for (size_t i = 0; i < len; ++i) {
		switch (s[i]) {
		case 'i': o |= RE_IGNORECASE;    break;
		case 'x': o |= RE_EXTENDED;      break;
		case 'm': o |= RE_MULTILINE;     break;
		case 'u': o |= RE_FIXEDENCODING; break;
		case 'n': o |= RE_NOENCODING;    break;
		case 'o': break;	// "compile once" has no meaning here
		default:
			mrb_raisef(mrb, E_ARGUMENT_ERROR,
				"unknown regexp option: %c", s[i]);
		}
	}
	return o;
}

// Ruby accepts nil / false / true / Integer / String where options go.
static uint32_t regexpOptionsFromValue(mrb_state *mrb, mrb_value v)
{
	if (mrb_nil_p(v) || mrb_false_p(v))
		return 0;
	if (mrb_integer_p(v))
		return (uint32_t)mrb_integer(v);
	if (mrb_string_p(v))
		return regexpOptionsFromChars(mrb, RSTRING_PTR(v),
			(size_t)RSTRING_LEN(v));
	return RE_IGNORECASE;	// any other truthy value, as in Ruby
}

static std::regex::flag_type regexpSyntaxFlags(uint32_t i_options)
{
	std::regex::flag_type f = std::regex::ECMAScript;
	if (i_options & RE_IGNORECASE)
		f |= std::regex::icase;
	// Ruby's /m means "dot matches newline", which ECMAScript cannot express;
	// it is mapped to multiline instead (see the notice in noticeMultiline).
	if (i_options & RE_MULTILINE)
		f |= std::regex::multiline;
	return f;
}

// Flag letters in Ruby's canonical inspect order.  Encoding bits are not
// shown, matching Ruby.
static std::string regexpFlagString(uint32_t i_options)
{
	std::string s;
	if (i_options & RE_MULTILINE)  s += 'm';
	if (i_options & RE_IGNORECASE) s += 'i';
	if (i_options & RE_EXTENDED)   s += 'x';
	return s;
}

// The literal as written, shortened so that the notice stays on one line.
static std::string regexpLiteralForLog(const RegexpData *i_d)
{
	std::wstring src = i_d->source;
	if (src.size() > 40) {
		src.resize(40);
		if (!src.empty() && (src.back() & 0xFC00) == 0xD800)
			src.pop_back();		// never cut a surrogate pair in half
		src += L"...";
	}
	return "/" + wideToUtf8(src) + "/" + regexpFlagString(i_d->options);
}

// Tell the user once that /m is not what Ruby's /m is.
static void noticeMultiline(const RegexpData *i_d)
{
	if (!(i_d->options & RE_MULTILINE) || !nysWouldLog(LogLevel::Info))
		return;
	if (!g_multilineNoticed.insert(i_d->source).second)
		return;
	nysLogUtf8(LogLevel::Info,
		("[mruby] " + regexpLiteralForLog(i_d) +
		 ": 'm' = ECMAScript multiline (^ $ at line breaks), not dotall").c_str());
}

// Build a Regexp of the given class.  Raises RegexpError if it does not
// compile, with the message std::regex produced.
static mrb_value regexpNew(mrb_state *mrb, struct RClass *i_cls,
	const std::wstring &i_source, uint32_t i_options)
{
	RegexpData *d = new RegexpData();
	d->source  = i_source;
	d->options = i_options;

	std::wstring pattern = (i_options & RE_EXTENDED)
		? applyExtended(i_source) : i_source;

	// The pattern is kept by the regex itself, so the compiled object and the
	// string it was compiled from can never drift apart.
	try {
		d->re.assign(pattern, regexpSyntaxFlags(i_options));
	} catch (const std::regex_error &e) {
		std::string what = e.what();
		delete d;
		mrb_raise(mrb, mrb_class_get(mrb, "RegexpError"), what.c_str());
	}

	noticeMultiline(d);
	return mrb_obj_value(
		mrb_data_object_alloc(mrb, i_cls, d, &g_regexpDataType));
}

// Regexp.compile(source, options = nil, encoding = nil)
// This is the entry point mruby's code generator calls for a /.../ literal.
static mrb_value regexp_s_compile(mrb_state *mrb, mrb_value self)
{
	mrb_value src_v = mrb_nil_value();
	mrb_value opt_v = mrb_nil_value();
	mrb_value enc_v = mrb_nil_value();
	mrb_get_args(mrb, "o|oo", &src_v, &opt_v, &enc_v);

	struct RClass *cls = mrb_class_ptr(self);

	// Regexp.new(other_regexp) copies it, as in Ruby.
	if (RegexpData *other = regexpPtr(mrb, src_v)) {
		uint32_t options = mrb_nil_p(opt_v)
			? other->options : regexpOptionsFromValue(mrb, opt_v);
		return regexpNew(mrb, cls, other->source, options);
	}

	if (!mrb_string_p(src_v))
		mrb_raise(mrb, E_TYPE_ERROR,
			"regexp source must be a String or Regexp");

	uint32_t options = regexpOptionsFromValue(mrb, opt_v);
	if (mrb_string_p(enc_v))
		options |= regexpOptionsFromChars(mrb, RSTRING_PTR(enc_v),
			(size_t)RSTRING_LEN(enc_v));

	return regexpNew(mrb, cls,
		utf8ToWide(RSTRING_PTR(src_v), (size_t)RSTRING_LEN(src_v)), options);
}

// Characters Ruby's Regexp.escape escapes.  '-' and '#' are in there because
// they are special inside a character class and under /x respectively.
static mrb_value regexp_s_escape(mrb_state *mrb, mrb_value)
{
	const char *p = nullptr;
	mrb_int len = 0;
	mrb_get_args(mrb, "s", &p, &len);

	std::string out;
	out.reserve((size_t)len);
	for (mrb_int i = 0; i < len; ++i) {
		char c = p[i];
		switch (c) {
		case '\n': out += "\\n"; continue;
		case '\r': out += "\\r"; continue;
		case '\t': out += "\\t"; continue;
		case '\f': out += "\\f"; continue;
		case '\v': out += "\\v"; continue;
		default: break;
		}
		if (strchr(".*?+^$()[]{}|\\/- #", c) != nullptr && c != '\0')
			out += '\\';
		out += c;
	}
	return mrb_str_new(mrb, out.c_str(), (mrb_int)out.size());
}

static mrb_value regexp_source(mrb_state *mrb, mrb_value self)
{
	return wideToStr(mrb, regexpPtrOrRaise(mrb, self)->source);
}

// Not a Ruby method: the pattern actually compiled, i.e. the source after the
// /x transformation.  This is what goes on the wire.
static mrb_value regexp_pattern(mrb_state *mrb, mrb_value self)
{
	return wideToStr(mrb, regexpPtrOrRaise(mrb, self)->re.str());
}

static mrb_value regexp_options(mrb_state *mrb, mrb_value self)
{
	return mrb_int_value(mrb, (mrb_int)regexpPtrOrRaise(mrb, self)->options);
}

static mrb_value regexp_inspect(mrb_state *mrb, mrb_value self)
{
	RegexpData *d = regexpPtrOrRaise(mrb, self);
	std::string s = "/" + wideToUtf8(d->source) + "/" +
		regexpFlagString(d->options);
	return mrb_str_new(mrb, s.c_str(), (mrb_int)s.size());
}

// Ruby renders a Regexp as "(?on-off:source)".
static mrb_value regexp_to_s(mrb_state *mrb, mrb_value self)
{
	RegexpData *d = regexpPtrOrRaise(mrb, self);
	static const struct { uint32_t bit; char letter; } table[] = {
		{ RE_MULTILINE,  'm' },
		{ RE_IGNORECASE, 'i' },
		{ RE_EXTENDED,   'x' },
	};
	std::string on, off;
	for (const auto &t : table)
		((d->options & t.bit) ? on : off) += t.letter;

	std::string s = "(?" + on;
	if (!off.empty())
		s += "-" + off;
	s += ":" + wideToUtf8(d->source) + ")";
	return mrb_str_new(mrb, s.c_str(), (mrb_int)s.size());
}

static mrb_value regexp_eq(mrb_state *mrb, mrb_value self)
{
	mrb_value other_v = mrb_nil_value();
	mrb_get_args(mrb, "o", &other_v);
	RegexpData *a = regexpPtrOrRaise(mrb, self);
	RegexpData *b = regexpPtr(mrb, other_v);
	return mrb_bool_value(b != nullptr &&
		a->source == b->source && a->options == b->options);
}

// Number of leading UTF-16 units that count as characters, i.e. the mruby
// string index of a wchar_t offset (MRB_UTF8_STRING indexes by character).
static mrb_int wideOffsetToCharIndex(const std::wstring &i_s, size_t i_off)
{
	mrb_int n = 0;
	size_t end = (i_off < i_s.size()) ? i_off : i_s.size();
	for (size_t i = 0; i < end; ++i)
		if ((i_s[i] & 0xFC00) != 0xDC00)
			++n;
	return n;
}

static size_t charIndexToWideOffset(const std::wstring &i_s, mrb_int i_index)
{
	if (i_index <= 0)
		return 0;
	mrb_int n = 0;
	for (size_t i = 0; i < i_s.size(); ++i) {
		if ((i_s[i] & 0xFC00) != 0xDC00) {
			if (n == i_index)
				return i;
			++n;
		}
	}
	return i_s.size();
}

static bool regexpSearch(const RegexpData *i_d, const std::wstring &i_subject,
	size_t i_start, std::wsmatch *o_m)
{
	if (i_start > i_subject.size())
		return false;
	std::regex_constants::match_flag_type f =
		std::regex_constants::match_default;
	if (i_start > 0)
		f |= std::regex_constants::match_prev_avail;
	return std::regex_search(i_subject.cbegin() + i_start, i_subject.cend(),
		*o_m, i_d->re, f);
}

static mrb_value makeMatchData(mrb_state *mrb, const std::wstring &i_subject,
	size_t i_start, const std::wsmatch &i_m)
{
	MatchDataData *md = new MatchDataData();
	md->subject = i_subject;
	md->groups.reserve(i_m.size());
	for (size_t i = 0; i < i_m.size(); ++i) {
		MatchDataData::Group g = { false, 0, 0 };
		if (i_m[i].matched) {
			g.matched = true;
			g.begin = i_start + (size_t)i_m.position((int)i);
			g.end   = g.begin + (size_t)i_m.length((int)i);
		}
		md->groups.push_back(g);
	}
	struct RClass *cls = mrb_class_get_under(mrb,
		mrb_module_get(mrb, "NYamy"), "MatchData");
	return mrb_obj_value(
		mrb_data_object_alloc(mrb, cls, md, &g_matchDataType));
}

static mrb_value matchGroup(mrb_state *mrb, MatchDataData *i_md, size_t i_n)
{
	if (i_n >= i_md->groups.size() || !i_md->groups[i_n].matched)
		return mrb_nil_value();
	const MatchDataData::Group &g = i_md->groups[i_n];
	return wideToStr(mrb, i_md->subject.substr(g.begin, g.end - g.begin));
}

/** Publish the result of a match in $~ and $1..$9.

    mruby compiles both of those to plain global variable reads, so setting the
    globals is all it takes.  Unlike Ruby they are not frame local: the last
    match stays visible across method boundaries.
*/
static void setLastMatch(mrb_state *mrb, mrb_value i_md)
{
	mrb_gv_set(mrb, mrb_intern_lit(mrb, "$~"), i_md);

	MatchDataData *md = mrb_nil_p(i_md) ? nullptr
		: static_cast<MatchDataData *>(DATA_PTR(i_md));
	for (int i = 1; i <= 9; ++i) {
		char name[3] = { '$', (char)('0' + i), '\0' };
		mrb_gv_set(mrb, mrb_intern_cstr(mrb, name),
			md ? matchGroup(mrb, md, (size_t)i) : mrb_nil_value());
	}
}

// Shared body of Regexp#match / #=~ / #===.  Returns the MatchData (or nil)
// and publishes it unless i_quiet.
static mrb_value regexpMatchValue(mrb_state *mrb, mrb_value self,
	mrb_value i_str, mrb_int i_pos, bool i_quiet)
{
	RegexpData *d = regexpPtrOrRaise(mrb, self);
	if (mrb_nil_p(i_str)) {
		if (!i_quiet)
			setLastMatch(mrb, mrb_nil_value());
		return mrb_nil_value();
	}
	if (!mrb_string_p(i_str))
		mrb_raise(mrb, E_TYPE_ERROR, "expected String to match against");

	std::wstring subject = utf8ToWide(RSTRING_PTR(i_str),
		(size_t)RSTRING_LEN(i_str));
	size_t start = charIndexToWideOffset(subject, i_pos);

	std::wsmatch m;
	if (!regexpSearch(d, subject, start, &m)) {
		if (!i_quiet)
			setLastMatch(mrb, mrb_nil_value());
		return mrb_nil_value();
	}
	mrb_value md = makeMatchData(mrb, subject, start, m);
	if (!i_quiet)
		setLastMatch(mrb, md);
	return md;
}

static mrb_value regexp_match(mrb_state *mrb, mrb_value self)
{
	mrb_value str = mrb_nil_value();
	mrb_int pos = 0;
	mrb_get_args(mrb, "o|i", &str, &pos);
	return regexpMatchValue(mrb, self, str, pos, false);
}

// Ruby's match? deliberately leaves $~ alone.
static mrb_value regexp_match_p(mrb_state *mrb, mrb_value self)
{
	mrb_value str = mrb_nil_value();
	mrb_int pos = 0;
	mrb_get_args(mrb, "o|i", &str, &pos);
	return mrb_bool_value(!mrb_nil_p(regexpMatchValue(mrb, self, str, pos, true)));
}

// Character index of the match, or nil.  Shared by Regexp#=~ and String#=~.
static mrb_value regexpMatchIndex(mrb_state *mrb, mrb_value i_re, mrb_value i_str)
{
	mrb_value md = regexpMatchValue(mrb, i_re, i_str, 0, false);
	if (mrb_nil_p(md))
		return mrb_nil_value();
	MatchDataData *m = static_cast<MatchDataData *>(DATA_PTR(md));
	return mrb_int_value(mrb,
		wideOffsetToCharIndex(m->subject, m->groups[0].begin));
}

static mrb_value regexp_match_op(mrb_state *mrb, mrb_value self)
{
	mrb_value str = mrb_nil_value();
	mrb_get_args(mrb, "o", &str);
	return regexpMatchIndex(mrb, self, str);
}

static mrb_value regexp_eqq(mrb_state *mrb, mrb_value self)
{
	mrb_value str = mrb_nil_value();
	mrb_get_args(mrb, "o", &str);
	if (!mrb_string_p(str))
		return mrb_false_value();
	return mrb_bool_value(!mrb_nil_p(regexpMatchValue(mrb, self, str, 0, false)));
}

static mrb_value regexp_s_last_match(mrb_state *mrb, mrb_value)
{
	mrb_int n = -1;
	mrb_get_args(mrb, "|i", &n);
	mrb_value md = mrb_gv_get(mrb, mrb_intern_lit(mrb, "$~"));
	if (n < 0 || mrb_nil_p(md))
		return md;
	return matchGroup(mrb, matchPtrOrRaise(mrb, md), (size_t)n);
}

static mrb_value matchdata_aref(mrb_state *mrb, mrb_value self)
{
	mrb_int n = 0;
	mrb_get_args(mrb, "i", &n);
	MatchDataData *md = matchPtrOrRaise(mrb, self);
	if (n < 0)
		n += (mrb_int)md->groups.size();
	if (n < 0)
		return mrb_nil_value();
	return matchGroup(mrb, md, (size_t)n);
}

static mrb_value matchdata_to_a(mrb_state *mrb, mrb_value self)
{
	MatchDataData *md = matchPtrOrRaise(mrb, self);
	mrb_value ary = mrb_ary_new(mrb);
	for (size_t i = 0; i < md->groups.size(); ++i)
		mrb_ary_push(mrb, ary, matchGroup(mrb, md, i));
	return ary;
}

static mrb_value matchdata_captures(mrb_state *mrb, mrb_value self)
{
	MatchDataData *md = matchPtrOrRaise(mrb, self);
	mrb_value ary = mrb_ary_new(mrb);
	for (size_t i = 1; i < md->groups.size(); ++i)
		mrb_ary_push(mrb, ary, matchGroup(mrb, md, i));
	return ary;
}

static mrb_value matchdata_size(mrb_state *mrb, mrb_value self)
{
	return mrb_int_value(mrb,
		(mrb_int)matchPtrOrRaise(mrb, self)->groups.size());
}

static mrb_value matchdata_to_s(mrb_state *mrb, mrb_value self)
{
	return matchGroup(mrb, matchPtrOrRaise(mrb, self), 0);
}

static mrb_value matchdata_pre_match(mrb_state *mrb, mrb_value self)
{
	MatchDataData *md = matchPtrOrRaise(mrb, self);
	return wideToStr(mrb, md->subject.substr(0, md->groups[0].begin));
}

static mrb_value matchdata_post_match(mrb_state *mrb, mrb_value self)
{
	MatchDataData *md = matchPtrOrRaise(mrb, self);
	return wideToStr(mrb, md->subject.substr(md->groups[0].end));
}

static mrb_value matchdata_begin(mrb_state *mrb, mrb_value self)
{
	mrb_int n = 0;
	mrb_get_args(mrb, "i", &n);
	MatchDataData *md = matchPtrOrRaise(mrb, self);
	if (n < 0 || (size_t)n >= md->groups.size() || !md->groups[(size_t)n].matched)
		return mrb_nil_value();
	return mrb_int_value(mrb,
		wideOffsetToCharIndex(md->subject, md->groups[(size_t)n].begin));
}

static mrb_value matchdata_end(mrb_state *mrb, mrb_value self)
{
	mrb_int n = 0;
	mrb_get_args(mrb, "i", &n);
	MatchDataData *md = matchPtrOrRaise(mrb, self);
	if (n < 0 || (size_t)n >= md->groups.size() || !md->groups[(size_t)n].matched)
		return mrb_nil_value();
	return mrb_int_value(mrb,
		wideOffsetToCharIndex(md->subject, md->groups[(size_t)n].end));
}

static mrb_value matchdata_inspect(mrb_state *mrb, mrb_value self)
{
	MatchDataData *md = matchPtrOrRaise(mrb, self);
	std::string s = "#<MatchData " +
		wideToUtf8(md->subject.substr(md->groups[0].begin,
			md->groups[0].end - md->groups[0].begin)) + ">";
	return mrb_str_new(mrb, s.c_str(), (mrb_int)s.size());
}

// String#to_regexp: also the hook the legacy .mayu path uses to turn a
// NYsType_Regexp function argument into a Ruby object.
static mrb_value string_to_regexp(mrb_state *mrb, mrb_value self)
{
	return regexpNew(mrb, regexpClass(mrb),
		utf8ToWide(RSTRING_PTR(self), (size_t)RSTRING_LEN(self)), 0);
}

// A String pattern is accepted wherever a Regexp is, as in Ruby.
static mrb_value stringToRegexp(mrb_state *mrb, mrb_value v)
{
	if (regexpPtr(mrb, v) != nullptr)
		return v;
	if (mrb_string_p(v))
		return regexpNew(mrb, regexpClass(mrb),
			utf8ToWide(RSTRING_PTR(v), (size_t)RSTRING_LEN(v)), 0);
	mrb_raise(mrb, E_TYPE_ERROR, "expected Regexp or String");
	return mrb_nil_value();	// not reached
}

// String#=~ and String#match / #match? are commented out in mruby's mrblib
// because they need a Regexp; define them now that there is one.
static mrb_value string_match_op(mrb_state *mrb, mrb_value self)
{
	mrb_value re = mrb_nil_value();
	mrb_get_args(mrb, "o", &re);
	if (mrb_string_p(re))
		mrb_raise(mrb, E_TYPE_ERROR, "type mismatch: String given");
	return regexpMatchIndex(mrb, stringToRegexp(mrb, re), self);
}

static mrb_value string_match(mrb_state *mrb, mrb_value self)
{
	mrb_value re = mrb_nil_value();
	mrb_int pos = 0;
	mrb_get_args(mrb, "o|i", &re, &pos);
	return regexpMatchValue(mrb, stringToRegexp(mrb, re), self, pos, false);
}

static mrb_value string_match_p(mrb_state *mrb, mrb_value self)
{
	mrb_value re = mrb_nil_value();
	mrb_int pos = 0;
	mrb_get_args(mrb, "o|i", &re, &pos);
	return mrb_bool_value(!mrb_nil_p(
		regexpMatchValue(mrb, stringToRegexp(mrb, re), self, pos, true)));
}


// Convert a NYsFuncArgs* to an mrb Array of Ruby values.
static mrb_value funcArgsToMrb(mrb_state *mrb, const NYsFuncArgs *fas)
{
	mrb_value ary = mrb_ary_new(mrb);
	if (!fas) return ary;
	int n = nys_func_args_length(fas);
	for (int i = 0; i < n; ++i) {
		int64_t val = 0, len = 0;
		NYsType t = nys_func_args_get(fas, i, &val, &len);
		mrb_value elem = mrb_nil_value();
		switch (t) {
		case NYsType_String:
		case NYsType_Regexp: {
			const char *p = reinterpret_cast<const char *>((uintptr_t)val);
			elem = mrb_str_new(mrb, p, (mrb_int)len);
			if (t == NYsType_Regexp)
				elem = mrb_funcall(mrb, elem, "to_regexp", 0);
			break;
		}
		case NYsType_Number:
			elem = mrb_int_value(mrb, (mrb_int)val);
			break;
		case NYsType_KeySeqIdx: {
			struct RClass *cls = mrb_class_get_under(mrb,
				mrb_module_get(mrb, "NYamy"), "KeySeq");
			elem = mrb_obj_new(mrb, cls, 0, nullptr);
			mrb_iv_set(mrb, elem, mrb_intern_lit(mrb, "@idx"),
				mrb_int_value(mrb, (mrb_int)val));
			break;
		}
		case NYsType_ModifierSpec: {
			struct RClass *cls = mrb_class_get_under(mrb,
				mrb_module_get(mrb, "NYamy"), "Modifier");
			mrb_value args[2] = {
				mrb_int_value(mrb, (mrb_int)(uint64_t)val),
				mrb_int_value(mrb, (mrb_int)(uint64_t)len),
			};
			elem = mrb_obj_new(mrb, cls, 2, args);
			break;
		}
		case NYsType_TokenSeq: {
			const NYsStrs *ss =
				reinterpret_cast<const NYsStrs *>((uintptr_t)val);
			elem = mrb_ary_new(mrb);
			int sn = nys_strs_length(ss);
			for (int j = 0; j < sn; ++j) {
				const char *sp = nullptr; size_t sl = 0;
				nys_strs_get(ss, j, &sp, &sl);
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

/** Extract a pattern string from a class:/title: keyword value.

    Accepts String or Regexp; returns an empty string for nil.  What a Regexp
    contributes is the compiled pattern, not its source: with /x the two differ
    and it is the compiled one the engine has to see.

    Flags other than /x cannot be carried over the wire - the command stream
    holds the pattern string alone - so their use is reported here, naming the
    keymap and keyword so the line can be found.
*/
static std::string regexpOrStr(mrb_state *mrb, mrb_value v,
	const char *i_keyword, const char *i_kw, const std::string &i_name)
{
	if (mrb_nil_p(v)) return std::string();
	if (mrb_string_p(v)) return toStdStr(mrb, v);

	if (RegexpData *d = regexpPtr(mrb, v)) {
		if ((d->options & RE_NOT_ON_WIRE) != 0 && nysWouldLog(LogLevel::Warn))
			nysLogUtf8(LogLevel::Warn,
				("[mruby] " + std::string(i_keyword) + " \"" + i_name +
				 "\": flags on " + i_kw +
				 ": are ignored (always case-insensitive)").c_str());
		return wideToUtf8(d->re.str());
	}

	// Anything else that answers to #source, so a script may pass its own
	// pattern object.
	mrb_value src = mrb_funcall(mrb, v, "source", 0);
	return toStdStr(mrb, src);
}


//=============================================================================
// NYamy::KeySeq  (wraps a keyseq index)
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
// NYamy::KeyMap  (key assignment proxy; used as key[lhs] = rhs)
//=============================================================================

static mrb_value keymap_assign(mrb_state *mrb, mrb_value self)
{
	traceDslCall(mrb, "key[...] =");
	(void)self;
	mrb_value *argv;
	mrb_int argc;
	mrb_get_args(mrb, "*", &argv, &argc);
	if (argc < 2)
		mrb_raise(mrb, E_ARGUMENT_ERROR,
			"key[lhs] = rhs: wrong number of arguments");

	mrb_value rhs = argv[argc - 1];
	int rhs_idx = resolveRhs(mrb, rhs);

	NYsStrs *lhs_ss;
	if (argc == 2) {
		lhs_ss = buildLhsStrs(mrb, argv[0]);
	} else {
		lhs_ss = nys_strs_new();
		for (mrb_int i = 0; i < argc - 1; ++i) {
			std::string s = toStdStr(mrb, argv[i]);
			nys_strs_push(lhs_ss, s.c_str(), s.size());
		}
	}

	bool ok = nys_assign_key(lhs_ss, rhs_idx);
	if (!ok) raiseApiError(mrb, "nys_assign_key failed");
	return rhs;
}


//=============================================================================
// NYamy::EventMap  (event assignment proxy; used as event[name] = rhs)
//=============================================================================

static mrb_value eventmap_assign(mrb_state *mrb, mrb_value self)
{
	traceDslCall(mrb, "event[...] =");
	(void)self;
	mrb_value name_v, rhs;
	mrb_get_args(mrb, "oo", &name_v, &rhs);
	std::string name = toStdStr(mrb, name_v);
	int rhs_idx = resolveRhs(mrb, rhs);
	if (!nys_assign_event(name.c_str(), rhs_idx))
		raiseApiError(mrb, "nys_assign_event failed");
	return rhs;
}


//=============================================================================
// NYamy::ModValue  (carries op + keys for ModMap operator+/-)
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
// NYamy::ModMap  (modifier assignment proxy)
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
		mrb_module_get(mrb, "NYamy"), "ModValue");
	mrb_value args[2] = {
		mrb_str_new_lit(mrb, "="),
		mrb_ary_new(mrb),
	};
	return mrb_obj_new(mrb, cls, 2, args);
}

static mrb_value modmap_set(mrb_state *mrb, mrb_value self)
{
	traceDslCall(mrb, "mod[...] =");
	mrb_value name_v, value_v;
	mrb_get_args(mrb, "oo", &name_v, &value_v);

	std::string name = toStdStr(mrb, name_v);

	struct RClass *mv_cls = mrb_class_get_under(mrb,
		mrb_module_get(mrb, "NYamy"), "ModValue");

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

	NYsStrs *keys_ss = buildYsStrs(mrb, keys_v);

	mrb_value pfx_v = mrb_iv_get(mrb, self, mrb_intern_lit(mrb, "@prefixes"));
	NYsStrs *pfx_ss = nullptr;
	if (!mrb_nil_p(pfx_v) && mrb_array_p(pfx_v))
		pfx_ss = buildYsStrs(mrb, pfx_v);

	bool ok = nys_assign_mod(pfx_ss, name.c_str(), op.c_str(), keys_ss);
	if (!ok) raiseApiError(mrb, "nys_assign_mod failed");
	return value_v;
}

// mod.prefix(prefixes) -> new ModMap with the given prefix list
static mrb_value modmap_prefix(mrb_state *mrb, mrb_value self)
{
	traceDslCall(mrb, "mod.prefix");
	(void)self;
	mrb_value pfx;
	mrb_get_args(mrb, "o", &pfx);
	if (!mrb_array_p(pfx))
		pfx = mrb_ary_new_from_values(mrb, 1, &pfx);
	struct RClass *cls = mrb_class_get_under(mrb,
		mrb_module_get(mrb, "NYamy"), "ModMap");
	return mrb_obj_new(mrb, cls, 1, &pfx);
}


//=============================================================================
// NYamy::Modifier  (wraps modifiers + dontcares bitmasks from preset args)
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
// NYamy::DSL  (main DSL object; .rb script is instance_eval'd on it)
//=============================================================================

/** Bracket a .mayu file so that its keymap context does not escape.

    In .mayu a keymap statement stays in effect until the next one, so a file
    that ends inside a keymap would otherwise decide where the assignments
    written after the `load' land.  Bracketing the include keeps that rule
    inside the file and hands the caller back its own context - Global at the
    top level of a script.  Global always exists (Reset creates it) and
    re-declaring it changes nothing.
*/
static void beginMayuScope(mrb_state *mrb)
{
	if (!nys_def_keymap(NYsKeymapScope_Block, "keymap", "Global",
			nullptr, nullptr, nullptr, nullptr, -1))
		raiseApiError(mrb, "nys_def_keymap failed");
}

static void endMayuScope(mrb_state *mrb)
{
	if (!nys_end_keymap())
		raiseApiError(mrb, "nys_end_keymap failed");
}

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
		beginMayuScope(mrb);
		if (!nys_include_mayu(path.c_str()))
			raiseApiError(mrb, "nys_include_mayu failed");
		endMayuScope(mrb);
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
	beginMayuScope(mrb);
	if (!nys_load_mayu()) raiseApiError(mrb, "nys_load_mayu failed");
	endMayuScope(mrb);
	return mrb_true_value();
}

static mrb_value dsl_keyseq(mrb_state *mrb, mrb_value self)
{
	traceDslCall(mrb, "keyseq");
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

	// a keyseq definition may carry ASSIGN-class modifiers, the same as
	// `keyseq $NAME = ...' in a .mayu file
	int idx = nys_reg_keyseq(name, actions, NYS_MODCTX_ASSIGN);
	if (idx < 0) raiseApiError(mrb, "nys_reg_keyseq failed");

	struct RClass *cls = mrb_class_get_under(mrb,
		mrb_module_get(mrb, "NYamy"), "KeySeq");
	mrb_value ks = mrb_obj_new(mrb, cls, 0, nullptr);
	mrb_iv_set(mrb, ks, mrb_intern_lit(mrb, "@idx"),
		mrb_int_value(mrb, (mrb_int)idx));
	return ks;
}

static mrb_value dsl_defkey(mrb_state *mrb, mrb_value self)
{
	traceDslCall(mrb, "defkey");
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

	NYsStrs *names_ss = nys_strs_new();
	for (mrb_int i = 0; i < argc; ++i) {
		if (mrb_array_p(argv[i])) {
			mrb_int n = RARRAY_LEN(argv[i]);
			for (mrb_int j = 0; j < n; ++j) {
				std::string s = toStdStr(mrb,
					mrb_ary_ref(mrb, argv[i], j));
				nys_strs_push(names_ss, s.c_str(), s.size());
			}
		} else {
			std::string s = toStdStr(mrb, argv[i]);
			nys_strs_push(names_ss, s.c_str(), s.size());
		}
	}

	NYsStrs *scan_ss = buildYsStrs(mrb, scan_v);
	bool ok = nys_def_key(names_ss, scan_ss);
	if (!ok) raiseApiError(mrb, "nys_def_key failed");
	return mrb_true_value();
}

static mrb_value dsl_defmod(mrb_state *mrb, mrb_value self)
{
	traceDslCall(mrb, "defmod");
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
	NYsStrs *keys_ss  = buildYsStrs(mrb, keys_v);
	bool ok = nys_def_mod(name.c_str(), keys_ss);
	if (!ok) raiseApiError(mrb, "nys_def_mod failed");
	return mrb_true_value();
}

static mrb_value dsl_defsync(mrb_state *mrb, mrb_value self)
{
	traceDslCall(mrb, "defsync");
	(void)self;
	mrb_value v;
	mrb_get_args(mrb, "o", &v);
	NYsStrs *ss = buildYsStrs(mrb, v);
	bool ok = nys_def_sync(ss);
	if (!ok) raiseApiError(mrb, "nys_def_sync failed");
	return mrb_true_value();
}

static mrb_value dsl_defalias(mrb_state *mrb, mrb_value self)
{
	traceDslCall(mrb, "defalias");
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
	if (!nys_def_alias(alias_name.c_str(), key_name.c_str()))
		raiseApiError(mrb, "nys_def_alias failed");
	return mrb_true_value();
}

static mrb_value dsl_defsubst(mrb_state *mrb, mrb_value self)
{
	traceDslCall(mrb, "defsubst");
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

	// the substitute target may carry ASSIGN-class modifiers
	int rhs_idx = resolveRhs(mrb, to_v, NYS_MODCTX_ASSIGN);
	NYsStrs *lhs_ss = buildLhsStrs(mrb, lhs_v);
	bool ok = nys_def_subst(lhs_ss, rhs_idx);
	if (!ok) raiseApiError(mrb, "nys_def_subst failed");
	return mrb_true_value();
}

static mrb_value dsl_defoption(mrb_state *mrb, mrb_value self)
{
	traceDslCall(mrb, "defoption");
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
	if (!nys_def_option(name.c_str(), val.c_str()))
		raiseApiError(mrb, "nys_def_option failed");
	return mrb_true_value();
}

// Shared implementation for keymap / keymap2 / window.
static mrb_value defineKeymap(mrb_state *mrb, mrb_value self,
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

	std::string class_s  = regexpOrStr(mrb, class_v, keyword, "class", name);
	std::string title_s  = regexpOrStr(mrb, title_v, keyword, "title", name);
	std::string op_s     = !mrb_nil_p(op_v)     ? toStdStr(mrb, op_v)     : std::string();
	std::string parent_s = !mrb_nil_p(parent_v) ? toStdStr(mrb, parent_v) : std::string();

	int default_idx = -1;
	if (!mrb_nil_p(default_v))
		default_idx = resolveRhs(mrb, default_v);

	// A keymap statement declares the keymap; it does not decide where the
	// assignments that follow it go.  With a block, the block is that place and
	// the keymap in effect on the way in is restored on the way out; without
	// one, nothing moves and the assignments stay where they were written.
	// (.mayu keeps its own rule: its compiler emits Enter, which this DSL never
	// does.)
	bool scoped = !mrb_nil_p(blk) && mrb_proc_p(blk);

	if (!nys_def_keymap(
			scoped ? NYsKeymapScope_Block : NYsKeymapScope_Declare,
			keyword,
			name.c_str(),
			class_s.empty()  ? nullptr : class_s.c_str(),
			title_s.empty()  ? nullptr : title_s.c_str(),
			op_s.empty()     ? nullptr : op_s.c_str(),
			parent_s.empty() ? nullptr : parent_s.c_str(),
			default_idx))
		raiseApiError(mrb, "nys_def_keymap failed");

	if (scoped) {
		mrb_funcall_with_block(mrb, self,
			mrb_intern_lit(mrb, "instance_eval"), 0, nullptr, blk);
		// An exception in the block unwinds past this, leaving the block
		// unclosed.  That is harmless: the setting is discarded wholesale when
		// on_load_setting reports failure, so no half-scoped stream is applied.
		if (!nys_end_keymap())
			raiseApiError(mrb, "nys_end_keymap failed");
	}

	return mrb_true_value();
}

static mrb_value dsl_keymap(mrb_state *mrb, mrb_value self)
{
	traceDslCall(mrb, "keymap");
	return defineKeymap(mrb, self, "keymap");
}

static mrb_value dsl_keymap2(mrb_state *mrb, mrb_value self)
{
	traceDslCall(mrb, "keymap2");
	return defineKeymap(mrb, self, "keymap2");
}

static mrb_value dsl_window(mrb_state *mrb, mrb_value self)
{
	traceDslCall(mrb, "window");
	return defineKeymap(mrb, self, "window");
}

// key  -> NYamy::KeyMap singleton on the DSL instance
static mrb_value dsl_key(mrb_state *mrb, mrb_value self)
{
	mrb_sym iv = mrb_intern_lit(mrb, "@__keymap__");
	mrb_value km = mrb_iv_get(mrb, self, iv);
	if (mrb_nil_p(km)) {
		struct RClass *cls = mrb_class_get_under(mrb,
			mrb_module_get(mrb, "NYamy"), "KeyMap");
		km = mrb_obj_new(mrb, cls, 0, nullptr);
		mrb_iv_set(mrb, self, iv, km);
	}
	return km;
}

// event -> NYamy::EventMap singleton on the DSL instance
static mrb_value dsl_event(mrb_state *mrb, mrb_value self)
{
	mrb_sym iv = mrb_intern_lit(mrb, "@__eventmap__");
	mrb_value em = mrb_iv_get(mrb, self, iv);
	if (mrb_nil_p(em)) {
		struct RClass *cls = mrb_class_get_under(mrb,
			mrb_module_get(mrb, "NYamy"), "EventMap");
		em = mrb_obj_new(mrb, cls, 0, nullptr);
		mrb_iv_set(mrb, self, iv, em);
	}
	return em;
}

// mod -> NYamy::ModMap singleton on the DSL instance (no prefixes)
static mrb_value dsl_mod(mrb_state *mrb, mrb_value self)
{
	mrb_sym iv = mrb_intern_lit(mrb, "@__modmap__");
	mrb_value mm = mrb_iv_get(mrb, self, iv);
	if (mrb_nil_p(mm)) {
		struct RClass *cls = mrb_class_get_under(mrb,
			mrb_module_get(mrb, "NYamy"), "ModMap");
		mm = mrb_obj_new(mrb, cls, 0, nullptr);
		mrb_iv_set(mrb, self, iv, mm);
	}
	return mm;
}

//=============================================================================
// NYamy::Log
//=============================================================================

// Symbol / string <-> LogLevel.  Named after Ruby's Logger so that the method
// names are the ones a Ruby author already expects.
static bool logLevelFromRuby(mrb_state *mrb, mrb_value v, LogLevel *o_level)
{
	std::string s;
	if (mrb_symbol_p(v)) {
		mrb_int len = 0;
		const char *p = mrb_sym_name_len(mrb, mrb_symbol(v), &len);
		s.assign(p, static_cast<size_t>(len));
	} else if (mrb_string_p(v)) {
		s.assign(RSTRING_PTR(v), RSTRING_LEN(v));
	} else {
		return false;
	}
	if (s == "error") { *o_level = LogLevel::Error; return true; }
	if (s == "warn")  { *o_level = LogLevel::Warn;  return true; }
	if (s == "info")  { *o_level = LogLevel::Info;  return true; }
	if (s == "debug") { *o_level = LogLevel::Debug; return true; }
	return false;
}

static mrb_value logLevelToRuby(mrb_state *mrb, LogLevel i_level)
{
	const char *name = "info";
	switch (i_level) {
	case LogLevel::Error: name = "error"; break;
	case LogLevel::Warn:  name = "warn";  break;
	case LogLevel::Debug: name = "debug"; break;
	default: break;
	}
	return mrb_symbol_value(mrb_intern_cstr(mrb, name));
}

// Shared body of log.error / log.warn / log.info / log.debug
static mrb_value logWrite(mrb_state *mrb, LogLevel i_level)
{
	mrb_value msg;
	mrb_get_args(mrb, "o", &msg);
	if (!nysWouldLog(i_level))
		return mrb_nil_value();
	nysLogUtf8(i_level, toStdStr(mrb, msg).c_str());
	return mrb_nil_value();
}

static mrb_value log_error(mrb_state *mrb, mrb_value) { return logWrite(mrb, LogLevel::Error); }
static mrb_value log_warn (mrb_state *mrb, mrb_value) { return logWrite(mrb, LogLevel::Warn);  }
static mrb_value log_info (mrb_state *mrb, mrb_value) { return logWrite(mrb, LogLevel::Info);  }
static mrb_value log_debug(mrb_state *mrb, mrb_value) { return logWrite(mrb, LogLevel::Debug); }

static mrb_value log_error_p(mrb_state *, mrb_value) { return mrb_bool_value(nysWouldLog(LogLevel::Error)); }
static mrb_value log_warn_p (mrb_state *, mrb_value) { return mrb_bool_value(nysWouldLog(LogLevel::Warn));  }
static mrb_value log_info_p (mrb_state *, mrb_value) { return mrb_bool_value(nysWouldLog(LogLevel::Info));  }
static mrb_value log_debug_p(mrb_state *, mrb_value) { return mrb_bool_value(nysWouldLog(LogLevel::Debug)); }

// Reader and writer are deliberately asymmetric: the reader answers "what
// actually gets through", which is the stricter of nyamy's threshold and this
// one, while the writer only sets this one.
static mrb_value log_level_get(mrb_state *mrb, mrb_value)
{
	return logLevelToRuby(mrb, nysEffectiveLogLevel());
}

static mrb_value log_level_set(mrb_state *mrb, mrb_value)
{
	mrb_value v;
	mrb_get_args(mrb, "o", &v);
	LogLevel level;
	if (!logLevelFromRuby(mrb, v, &level))
		mrb_raise(mrb, E_ARGUMENT_ERROR,
				  "log level must be :error, :warn, :info or :debug");
	nysSetLogLevelFromScript(level);
	return v;
}

// log -> NYamy::Log singleton on the DSL instance
static mrb_value dsl_log(mrb_state *mrb, mrb_value self)
{
	mrb_sym iv = mrb_intern_lit(mrb, "@__log__");
	mrb_value lg = mrb_iv_get(mrb, self, iv);
	if (mrb_nil_p(lg)) {
		struct RClass *cls = mrb_class_get_under(mrb,
			mrb_module_get(mrb, "NYamy"), "Log");
		lg = mrb_obj_new(mrb, cls, 0, nullptr);
		mrb_iv_set(mrb, self, iv, lg);
	}
	return lg;
}

//=============================================================================
// ENV
//=============================================================================

// A read-only view of the process environment, enough of ENV to branch a
// configuration on a machine-specific variable and to build a path from one.
//
// Read-only on purpose: the scripter loads the configuration and launches
// nothing, so a write would reach nobody.  In particular it would not reach the
// ini "cmdLine", which nyamy expands in its own process before this one starts.
//
// Not the mruby-env gem: pulling in a gem changes how mruby is built, and this
// is a hundred lines.

// Name of the variable being asked about.  Symbols are accepted so that
// ENV[:HOME] reads naturally, though ENV["HOME"] is the Ruby spelling.
static std::wstring envNameArg(mrb_state *mrb, mrb_value v)
{
	if (mrb_symbol_p(v)) {
		mrb_int len = 0;
		const char *p = mrb_sym_name_len(mrb, mrb_symbol(v), &len);
		return utf8ToWide(std::string(p, (size_t)len));
	}
	return utf8ToWide(toStdStr(mrb, v));
}

// GetEnvironmentVariableW into a std::wstring.  Returns false when unset, which
// is not the same as set-but-empty: the length is 0 for both, so the error code
// has to settle it.
static bool envLookup(const std::wstring &name, std::wstring *o_value)
{
	SetLastError(ERROR_SUCCESS);
	DWORD need = GetEnvironmentVariableW(name.c_str(), nullptr, 0);
	if (need == 0) {
		if (GetLastError() == ERROR_ENVVAR_NOT_FOUND)
			return false;
		o_value->clear();
		return true;
	}
	std::wstring buf(need, L'\0');
	DWORD got = GetEnvironmentVariableW(name.c_str(), &buf[0], need);
	if (got == 0 || need <= got)
		return false;
	buf.resize(got);
	*o_value = std::move(buf);
	return true;
}

static mrb_value envToMrb(mrb_state *mrb, const std::wstring &value)
{
	std::string utf8 = wideToUtf8(value);
	return mrb_str_new(mrb, utf8.c_str(), utf8.size());
}

static mrb_value env_aref(mrb_state *mrb, mrb_value)
{
	mrb_value name_v;
	mrb_get_args(mrb, "o", &name_v);
	std::wstring value;
	if (!envLookup(envNameArg(mrb, name_v), &value))
		return mrb_nil_value();
	return envToMrb(mrb, value);
}

static mrb_value env_fetch(mrb_state *mrb, mrb_value)
{
	mrb_value name_v, default_v = mrb_undef_value();
	mrb_get_args(mrb, "o|o", &name_v, &default_v);
	std::wstring value;
	if (envLookup(envNameArg(mrb, name_v), &value))
		return envToMrb(mrb, value);
	if (!mrb_undef_p(default_v))
		return default_v;
	mrb_raisef(mrb, E_KEY_ERROR, "key not found: %v", name_v);
	return mrb_nil_value();	// not reached
}

static mrb_value env_key_p(mrb_state *mrb, mrb_value)
{
	mrb_value name_v;
	mrb_get_args(mrb, "o", &name_v);
	std::wstring value;
	return mrb_bool_value(envLookup(envNameArg(mrb, name_v), &value));
}

// Walk the whole block once, handing each "NAME=VALUE" to visit().  An entry
// whose name is empty is skipped: those are the "=C:=C:\..." per-drive current
// directories Windows keeps in the block, which are not environment variables
// in any sense a script cares about.
static void envForEach(const std::function<void(const std::wstring &,
                                                const std::wstring &)> &visit)
{
	wchar_t *block = GetEnvironmentStringsW();
	if (!block)
		return;
	for (const wchar_t *p = block; *p; ) {
		std::wstring entry(p);
		p += entry.size() + 1;
		size_t eq = entry.find(L'=');
		if (eq == std::wstring::npos || eq == 0)
			continue;
		visit(entry.substr(0, eq), entry.substr(eq + 1));
	}
	FreeEnvironmentStringsW(block);
}

static mrb_value env_keys(mrb_state *mrb, mrb_value)
{
	mrb_value ary = mrb_ary_new(mrb);
	envForEach([&](const std::wstring &name, const std::wstring &) {
		mrb_ary_push(mrb, ary, envToMrb(mrb, name));
	});
	return ary;
}

static mrb_value env_to_h(mrb_state *mrb, mrb_value)
{
	mrb_value hash = mrb_hash_new(mrb);
	envForEach([&](const std::wstring &name, const std::wstring &value) {
		mrb_hash_set(mrb, hash, envToMrb(mrb, name), envToMrb(mrb, value));
	});
	return hash;
}

// each yields [name, value] pairs, so Enumerable-style use reads the same as
// it would over a Hash.
static mrb_value env_each(mrb_state *mrb, mrb_value self)
{
	mrb_value blk = mrb_nil_value();
	mrb_get_args(mrb, "&", &blk);
	if (mrb_nil_p(blk))
		return mrb_funcall(mrb, self, "to_h", 0);
	envForEach([&](const std::wstring &name, const std::wstring &value) {
		mrb_value args[2] = { envToMrb(mrb, name), envToMrb(mrb, value) };
		mrb_yield_argv(mrb, blk, 2, args);
	});
	return self;
}


// deffunc(name) { |trigger, *args| ... }
static mrb_value dsl_deffunc(mrb_state *mrb, mrb_value self)
{
	traceDslCall(mrb, "deffunc");
	(void)self;
	mrb_value name_v, blk = mrb_nil_value();
	mrb_get_args(mrb, "o&", &name_v, &blk);

	std::string func_name = toStdStr(mrb, name_v);

	if (!nys_reg_user_func(func_name.c_str(), mruby_on_exec_user_func))
		raiseApiError(mrb, "nys_reg_user_func failed");

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
	traceDslCall(mrb, "define");
	(void)self;
	mrb_value name_v;
	mrb_get_args(mrb, "o", &name_v);
	std::string name = toStdStr(mrb, name_v);
	if (!nys_define_symbol(name.c_str()))
		raiseApiError(mrb, "nys_define_symbol failed");
	return mrb_true_value();
}

// DSL#symbol_defined?(name)  ->  true if <name> is defined   (mirrors .mayu `if (name)`)
static mrb_value dsl_symbol_defined_p(mrb_state *mrb, mrb_value self)
{
	(void)self;
	mrb_value name_v;
	mrb_get_args(mrb, "o", &name_v);
	std::string name = toStdStr(mrb, name_v);
	return mrb_bool_value(nys_has_symbol(name.c_str()));
}

// DSL#exec_keyseq(actions)  (runtime API; valid from on_exec_user_func)
static mrb_value dsl_exec_keyseq(mrb_state *mrb, mrb_value self)
{
	traceDslCall(mrb, "exec_keyseq");
	(void)self;
	const char *actions = nullptr;
	mrb_get_args(mrb, "z", &actions);
	return mrb_bool_value(nys_exec_keyseq(actions));
}

// NYamy.last_error  (module-level method)
static mrb_value nyamy_last_error(mrb_state *mrb, mrb_value)
{
	(void)mrb;
	const char *msg = nys_last_error();
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
	int word = nys_sc_resolve(s.c_str());
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

// DSL#nls_key?(key_or_scancode)  ->  true if registered by
// `defoption "nls-keys"' (key names take priority, see #sc)
static mrb_value dsl_nls_key_p(mrb_state *mrb, mrb_value self)
{
	(void)self;
	mrb_value v;
	mrb_get_args(mrb, "o", &v);
	return mrb_bool_value(nys_is_nls_key_word(scResolveArgOrRaise(mrb, v)));
}

// ScancodeMap[from]  ->  remapped scan code or nil
static mrb_value scancodemap_from(mrb_state *mrb, mrb_value self)
{
	(void)self;
	mrb_value v;
	mrb_get_args(mrb, "o", &v);
	int from = scResolveArgOrRaise(mrb, v);
	int n = nys_scancode_map_length();
	for (int i = 0; i < n; ++i) {
		unsigned f = 0, t = 0;
		if (nys_scancode_map_entry(i, &f, &t) && (int)f == from)
			return mrb_int_value(mrb, (mrb_int)t);
	}
	return mrb_nil_value();
}

// ScancodeMap.to[to]  ->  [original scan code, ...]  (empty when unmapped)
static mrb_value scancodemap_to_index(mrb_state *mrb, mrb_value self)
{
	(void)self;
	mrb_value v;
	mrb_get_args(mrb, "o", &v);
	int to = scResolveArgOrRaise(mrb, v);
	mrb_value ary = mrb_ary_new(mrb);
	int n = nys_scancode_map_length();
	for (int i = 0; i < n; ++i) {
		unsigned f = 0, t = 0;
		if (nys_scancode_map_entry(i, &f, &t) && (int)t == to)
			mrb_ary_push(mrb, ary, mrb_int_value(mrb, (mrb_int)f));
	}
	return ary;
}

// ScancodeMap.to  ->  the reverse-lookup module, so that both directions are
// written the same way: ScancodeMap[x] and ScancodeMap.to[x].  self is the
// ScancodeMap module itself, so the constant is fetched from it directly and
// needs no separate GC root.
static mrb_value scancodemap_to(mrb_state *mrb, mrb_value self)
{
	// Rejects the withdrawn ScancodeMap.to(x) form with an ArgumentError
	// instead of quietly handing back the module.
	mrb_get_args(mrb, "");
	return mrb_const_get(mrb, self, mrb_intern_lit(mrb, "To"));
}


//=============================================================================
// nyamy_mruby_init_internal  (static; called from mruby_on_load_setting)
//=============================================================================

static void nyamy_mruby_init_internal(mrb_state *mrb)
{
	struct RClass *nyamy = mrb_define_module(mrb, "NYamy");
	mrb_define_module_function(mrb, nyamy, "last_error",
		nyamy_last_error, MRB_ARGS_NONE());

	struct RClass *ks_cls = mrb_define_class_under(mrb, nyamy, "KeySeq",
		mrb->object_class);
	mrb_define_method(mrb, ks_cls, "initialize",
		keyseq_initialize, MRB_ARGS_OPT(1));
	mrb_define_method(mrb, ks_cls, "idx", keyseq_idx, MRB_ARGS_NONE());

	struct RClass *km_cls = mrb_define_class_under(mrb, nyamy, "KeyMap",
		mrb->object_class);
	mrb_define_method(mrb, km_cls, "[]=", keymap_assign, MRB_ARGS_ANY());

	struct RClass *em_cls = mrb_define_class_under(mrb, nyamy, "EventMap",
		mrb->object_class);
	mrb_define_method(mrb, em_cls, "[]=", eventmap_assign, MRB_ARGS_REQ(2));

	struct RClass *mv_cls = mrb_define_class_under(mrb, nyamy, "ModValue",
		mrb->object_class);
	mrb_define_method(mrb, mv_cls, "initialize",
		modvalue_initialize, MRB_ARGS_REQ(2));
	mrb_define_method(mrb, mv_cls, "op",   modvalue_op,    MRB_ARGS_NONE());
	mrb_define_method(mrb, mv_cls, "keys", modvalue_keys,  MRB_ARGS_NONE());
	mrb_define_method(mrb, mv_cls, "+",    modvalue_plus,  MRB_ARGS_REQ(1));
	mrb_define_method(mrb, mv_cls, "-",    modvalue_minus, MRB_ARGS_REQ(1));

	struct RClass *mm_cls = mrb_define_class_under(mrb, nyamy, "ModMap",
		mrb->object_class);
	mrb_define_method(mrb, mm_cls, "initialize",
		modmap_initialize, MRB_ARGS_OPT(1));
	mrb_define_method(mrb, mm_cls, "[]",     modmap_get,    MRB_ARGS_REQ(1));
	mrb_define_method(mrb, mm_cls, "[]=",    modmap_set,    MRB_ARGS_REQ(2));
	mrb_define_method(mrb, mm_cls, "prefix", modmap_prefix, MRB_ARGS_REQ(1));

	struct RClass *mod_cls = mrb_define_class_under(mrb, nyamy, "Modifier",
		mrb->object_class);
	mrb_define_method(mrb, mod_cls, "initialize",
		modifier_initialize, MRB_ARGS_OPT(2));

	// Regexp / MatchData.  The real names sit under NYamy like every other
	// binding class; the top-level constants are aliases, because mruby's
	// code generator looks up ::Regexp on Object for a /.../ literal.  The
	// guard hands the name over without a code change should a future mruby
	// ship a Regexp of its own.
	struct RClass *re_cls = mrb_define_class_under(mrb, nyamy, "Regexp",
		mrb->object_class);
	MRB_SET_INSTANCE_TT(re_cls, MRB_TT_CDATA);
	mrb_define_class_method(mrb, re_cls, "compile", regexp_s_compile, MRB_ARGS_ARG(1, 2));
	mrb_define_class_method(mrb, re_cls, "new",     regexp_s_compile, MRB_ARGS_ARG(1, 2));
	mrb_define_class_method(mrb, re_cls, "escape",  regexp_s_escape,  MRB_ARGS_REQ(1));
	mrb_define_class_method(mrb, re_cls, "quote",   regexp_s_escape,  MRB_ARGS_REQ(1));
	mrb_define_class_method(mrb, re_cls, "last_match",
		regexp_s_last_match, MRB_ARGS_OPT(1));
	mrb_define_method(mrb, re_cls, "source",  regexp_source,   MRB_ARGS_NONE());
	mrb_define_method(mrb, re_cls, "pattern", regexp_pattern,  MRB_ARGS_NONE());
	mrb_define_method(mrb, re_cls, "options", regexp_options,  MRB_ARGS_NONE());
	mrb_define_method(mrb, re_cls, "inspect", regexp_inspect,  MRB_ARGS_NONE());
	mrb_define_method(mrb, re_cls, "to_s",    regexp_to_s,     MRB_ARGS_NONE());
	mrb_define_method(mrb, re_cls, "==",      regexp_eq,       MRB_ARGS_REQ(1));
	mrb_define_method(mrb, re_cls, "eql?",    regexp_eq,       MRB_ARGS_REQ(1));
	mrb_define_method(mrb, re_cls, "match",   regexp_match,    MRB_ARGS_ARG(1, 1));
	mrb_define_method(mrb, re_cls, "match?",  regexp_match_p,  MRB_ARGS_ARG(1, 1));
	mrb_define_method(mrb, re_cls, "=~",      regexp_match_op, MRB_ARGS_REQ(1));
	mrb_define_method(mrb, re_cls, "===",     regexp_eqq,      MRB_ARGS_REQ(1));
	mrb_define_const(mrb, re_cls, "IGNORECASE",    mrb_int_value(mrb, RE_IGNORECASE));
	mrb_define_const(mrb, re_cls, "EXTENDED",      mrb_int_value(mrb, RE_EXTENDED));
	mrb_define_const(mrb, re_cls, "MULTILINE",     mrb_int_value(mrb, RE_MULTILINE));
	mrb_define_const(mrb, re_cls, "FIXEDENCODING", mrb_int_value(mrb, RE_FIXEDENCODING));
	mrb_define_const(mrb, re_cls, "NOENCODING",    mrb_int_value(mrb, RE_NOENCODING));

	struct RClass *md_cls = mrb_define_class_under(mrb, nyamy, "MatchData",
		mrb->object_class);
	MRB_SET_INSTANCE_TT(md_cls, MRB_TT_CDATA);
	mrb_define_method(mrb, md_cls, "[]",         matchdata_aref,       MRB_ARGS_REQ(1));
	mrb_define_method(mrb, md_cls, "to_a",       matchdata_to_a,       MRB_ARGS_NONE());
	mrb_define_method(mrb, md_cls, "captures",   matchdata_captures,   MRB_ARGS_NONE());
	mrb_define_method(mrb, md_cls, "size",       matchdata_size,       MRB_ARGS_NONE());
	mrb_define_method(mrb, md_cls, "length",     matchdata_size,       MRB_ARGS_NONE());
	mrb_define_method(mrb, md_cls, "to_s",       matchdata_to_s,       MRB_ARGS_NONE());
	mrb_define_method(mrb, md_cls, "pre_match",  matchdata_pre_match,  MRB_ARGS_NONE());
	mrb_define_method(mrb, md_cls, "post_match", matchdata_post_match, MRB_ARGS_NONE());
	mrb_define_method(mrb, md_cls, "begin",      matchdata_begin,      MRB_ARGS_REQ(1));
	mrb_define_method(mrb, md_cls, "end",        matchdata_end,        MRB_ARGS_REQ(1));
	mrb_define_method(mrb, md_cls, "inspect",    matchdata_inspect,    MRB_ARGS_NONE());

	if (!mrb_const_defined(mrb, mrb_obj_value(mrb->object_class),
			mrb_intern_lit(mrb, "Regexp")))
		mrb_define_const(mrb, mrb->object_class, "Regexp", mrb_obj_value(re_cls));
	if (!mrb_const_defined(mrb, mrb_obj_value(mrb->object_class),
			mrb_intern_lit(mrb, "MatchData")))
		mrb_define_const(mrb, mrb->object_class, "MatchData", mrb_obj_value(md_cls));

	// mruby leaves these commented out in mrblib/string.rb because they need
	// a Regexp; there is one now.
	mrb_define_method(mrb, mrb->string_class, "=~",        string_match_op, MRB_ARGS_REQ(1));
	mrb_define_method(mrb, mrb->string_class, "match",     string_match,    MRB_ARGS_ARG(1, 1));
	mrb_define_method(mrb, mrb->string_class, "match?",    string_match_p,  MRB_ARGS_ARG(1, 1));
	mrb_define_method(mrb, mrb->string_class, "to_regexp", string_to_regexp, MRB_ARGS_NONE());

	g_multilineNoticed.clear();

	struct RClass *dsl_cls = mrb_define_class_under(mrb, nyamy, "DSL",
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
	mrb_define_method(mrb, dsl_cls, "nls_key?",    dsl_nls_key_p,   MRB_ARGS_REQ(1));
	mrb_define_method(mrb, dsl_cls, "log",         dsl_log,         MRB_ARGS_NONE());

	struct RClass *log_cls = mrb_define_class_under(mrb, nyamy, "Log",
		mrb->object_class);
	mrb_define_method(mrb, log_cls, "error",  log_error,     MRB_ARGS_REQ(1));
	mrb_define_method(mrb, log_cls, "warn",   log_warn,      MRB_ARGS_REQ(1));
	mrb_define_method(mrb, log_cls, "info",   log_info,      MRB_ARGS_REQ(1));
	mrb_define_method(mrb, log_cls, "debug",  log_debug,     MRB_ARGS_REQ(1));
	mrb_define_method(mrb, log_cls, "error?", log_error_p,   MRB_ARGS_NONE());
	mrb_define_method(mrb, log_cls, "warn?",  log_warn_p,    MRB_ARGS_NONE());
	mrb_define_method(mrb, log_cls, "info?",  log_info_p,    MRB_ARGS_NONE());
	mrb_define_method(mrb, log_cls, "debug?", log_debug_p,   MRB_ARGS_NONE());
	mrb_define_method(mrb, log_cls, "level",  log_level_get, MRB_ARGS_NONE());
	mrb_define_method(mrb, log_cls, "level=", log_level_set, MRB_ARGS_REQ(1));

	// ENV: read-only view of the process environment, as a top-level constant
	// so that scripts spell it the way Ruby does.
	{
		struct RClass *env_cls = mrb_define_class_under(mrb, nyamy, "Env",
			mrb->object_class);
		mrb_define_method(mrb, env_cls, "[]",       env_aref,  MRB_ARGS_REQ(1));
		mrb_define_method(mrb, env_cls, "fetch",    env_fetch, MRB_ARGS_ARG(1, 1));
		mrb_define_method(mrb, env_cls, "key?",     env_key_p, MRB_ARGS_REQ(1));
		mrb_define_method(mrb, env_cls, "include?", env_key_p, MRB_ARGS_REQ(1));
		mrb_define_method(mrb, env_cls, "has_key?", env_key_p, MRB_ARGS_REQ(1));
		mrb_define_method(mrb, env_cls, "keys",     env_keys,  MRB_ARGS_NONE());
		mrb_define_method(mrb, env_cls, "to_h",     env_to_h,  MRB_ARGS_NONE());
		mrb_define_method(mrb, env_cls, "each",     env_each,  MRB_ARGS_BLOCK());
		mrb_define_const(mrb, mrb->object_class, "ENV",
			mrb_obj_new(mrb, env_cls, 0, nullptr));
	}

	// ScancodeMap: read-only view of the registry Scancode Map (top-level module).
	// Forward lookup is ScancodeMap[x]; reverse lookup goes through the nested
	// ScancodeMap::To module so that it reads as ScancodeMap.to[x].
	struct RClass *scmap = mrb_define_module(mrb, "ScancodeMap");
	struct RClass *scmapTo = mrb_define_module_under(mrb, scmap, "To");
	mrb_define_module_function(mrb, scmapTo, "[]", scancodemap_to_index, MRB_ARGS_REQ(1));
	mrb_define_module_function(mrb, scmap, "[]",   scancodemap_from, MRB_ARGS_REQ(1));
	mrb_define_module_function(mrb, scmap, "to",   scancodemap_to,   MRB_ARGS_NONE());

	// GC-protect the func table hash as a module constant
	g_funcTable = mrb_hash_new(mrb);
	mrb_define_const(mrb, nyamy, "FUNC_TABLE", g_funcTable);
}


//=============================================================================
// mruby_on_load_setting
//=============================================================================

bool mruby_on_load_setting(void* exeCtx)
{
	MRubyContext *ctx = static_cast<MRubyContext *>(exeCtx);

	// 1. Resolve the script to an absolute path.  A relative script argument is
	//    searched in the config search path (NYAMY_CONFIG, then NYAMY_ROOT);
	//    the current directory is never consulted, so where nyamy happened to
	//    be started from cannot decide which configuration is loaded.
	//    main() rejects a missing script before nys_start, so reaching this
	//    without one is an internal error.
	int scriptIdx = ctx->scriptArgIndex;
	if (scriptIdx <= 0 || ctx->argc <= scriptIdx) {
		nysLogUtf8(LogLevel::Error,
				   "no script path was passed to on_load_setting");
		return false;
	}

	const char *found = nullptr;
	if (!nys_resolve_config_path(ctx->argv[scriptIdx], &found) || !found) {
		nysLogUtf8(LogLevel::Error,
				   (std::string("script not found: ") + ctx->argv[scriptIdx] +
					" (searched: " + nys_paths_config() + ";" +
					nys_paths_root() + ")").c_str());
		return false;
	}
	std::string script = canonicalizePath(utf8ToWide(found));

	// 2. Open mruby state and register DSL classes.
	//    A second Start reloads in-process (the test harness does this; the
	//    product restarts the process instead).  Close the previous state
	//    first: g_funcTable and every registered handler belong to it, and
	//    nys_start has already cleared the user-function table for this cycle.
	if (ctx->mrb) {
		mrb_close(ctx->mrb);
		ctx->mrb = nullptr;
	}
	mrb_state *mrb = mrb_open();
	if (!mrb) {
		nysLogUtf8(LogLevel::Error, "failed to initialise mruby");
		return false;
	}
	ctx->mrb = mrb;
	nyamy_mruby_init_internal(mrb);

	// 3. Set $LOAD_PATH to the script's own directory, the -I and
	//    NYAMY_LOAD_PATH directories, the config directory, the user library
	//    directory and the installation, and $LOADED_FEATURES to an empty
	//    array.  DSL#load and DSL#require search $LOAD_PATH when given a
	//    relative .rb path.
	//
	//    The script's own directory stays first so that a file sitting next to
	//    the configuration still wins; the added directories go straight after
	//    it, ahead of the standard three.
	{
		mrb_value load_path = mrb_ary_new(mrb);
		auto pushUnique = [&](const std::string &dir) {
			if (dir.empty()) return;
			mrb_int n = RARRAY_LEN(load_path);
			for (mrb_int i = 0; i < n; ++i) {
				mrb_value e = mrb_ary_ref(mrb, load_path, i);
				std::string s(RSTRING_PTR(e), RSTRING_LEN(e));
				if (_stricmp(s.c_str(), dir.c_str()) == 0)
					return;
			}
			mrb_ary_push(mrb, load_path,
				mrb_str_new(mrb, dir.c_str(), dir.size()));
		};
		// A directory that does not exist is worth a word but not a refusal:
		// it may be created later, and refusing would cost the whole setting.
		auto pushDirectory = [&](const std::string &dir, const char *origin) {
			std::string norm = normalizeDirectory(dir);
			DWORD attr = GetFileAttributesW(utf8ToWide(norm).c_str());
			if (attr == INVALID_FILE_ATTRIBUTES ||
			    !(attr & FILE_ATTRIBUTE_DIRECTORY))
				nysLogUtf8(LogLevel::Warn,
						   (std::string(origin) + ": not a directory: " +
							norm).c_str());
			pushUnique(norm);
		};

		size_t sep = script.find_last_of("\\/");
		if (sep != std::string::npos && sep > 0)
			pushUnique(script.substr(0, sep));

		if (ctx->includeDirs)
			for (const char *const *p = ctx->includeDirs; *p; ++p)
				pushDirectory(*p, "-I");

		// NYAMY_LOAD_PATH comes after -I: the command line is the more
		// specific statement of the two.  A relative element is dropped with a
		// warning rather than taken as a usage error - an environment variable
		// left over from somewhere else should not stop nyamy from starting.
		{
			std::wstring wenv;
			DWORD need = GetEnvironmentVariableW(L"NYAMY_LOAD_PATH", nullptr, 0);
			if (need > 0) {
				wenv.resize(need);
				DWORD got = GetEnvironmentVariableW(L"NYAMY_LOAD_PATH",
													&wenv[0], need);
				wenv.resize(got);
			}
			for (const auto &dir : splitDirectoryList(wideToUtf8(wenv))) {
				std::string norm = normalizeDirectory(dir);
				if (!isAbsoluteDirectory(norm)) {
					nysLogUtf8(LogLevel::Warn,
							   ("NYAMY_LOAD_PATH: ignoring relative path: " +
								dir).c_str());
					continue;
				}
				pushDirectory(norm, "NYAMY_LOAD_PATH");
			}
		}

		pushUnique(nys_paths_config());
		pushUnique(std::string(nys_paths_home()) + "\\Lib");
		pushUnique(nys_paths_root());

		mrb_gv_set(mrb, mrb_intern_cstr(mrb, "$LOAD_PATH"), load_path);
		mrb_gv_set(mrb, mrb_intern_cstr(mrb, "$LOADED_FEATURES"),
			mrb_ary_new(mrb));

		// One line per entry: these are full paths, and a single ";"-joined
		// line is too wide to read in the log dialog.  The index keeps each
		// line meaningful on its own once the log is interleaved with others.
		nysLogUtf8(LogLevel::Info, ("script: " + script).c_str());
		mrb_int n = RARRAY_LEN(load_path);
		for (mrb_int i = 0; i < n; ++i) {
			mrb_value e = mrb_ary_ref(mrb, load_path, i);
			std::string line = "  $LOAD_PATH[" + std::to_string((int)i) + "]: " +
				std::string(RSTRING_PTR(e), RSTRING_LEN(e));
			nysLogUtf8(LogLevel::Info, line.c_str());
		}
	}

	// 4. Set $0 to the script path.
	mrb_gv_set(mrb, mrb_intern_cstr(mrb, "$0"),
		mrb_str_new(mrb, script.c_str(), script.size()));

	// 5. Set ARGV constant from arguments after the script name.
	{
		mrb_value argv_val = mrb_ary_new(mrb);
		for (int i = scriptIdx + 1; i < ctx->argc; ++i)
			mrb_ary_push(mrb, argv_val, mrb_str_new_cstr(mrb, ctx->argv[i]));
		mrb_define_global_const(mrb, "ARGV", argv_val);
	}

	// 6. Reset the func table for this load cycle.
	g_funcTable = mrb_hash_new(mrb);
	struct RClass *nyamy = mrb_module_get(mrb, "NYamy");
	mrb_define_const(mrb, nyamy, "FUNC_TABLE", g_funcTable);

	// 7. Evaluate the script on a fresh NYamy::DSL instance so that the DSL
	//    methods (keymap, key, event, mod, symbol_defined?, define, load, ...) are in
	//    scope for the top-level script.  DSL#load instance_eval's a .rb file
	//    on the same object (and compiles a .mayu file via nys_include_mayu).
	{
		struct RClass *dsl_cls = mrb_class_get_under(mrb,
			mrb_module_get(mrb, "NYamy"), "DSL");
		mrb_value dsl = mrb_obj_new(mrb, dsl_cls, 0, nullptr);
		mrb_funcall(mrb, dsl, "load", 1,
			mrb_str_new(mrb, script.c_str(), script.size()));
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
	const NYsFuncArgs *args)
{
	MRubyContext *ctx = static_cast<MRubyContext *>(exeCtx);
	if (!ctx->mrb) return;
	mrb_state *mrb = ctx->mrb;

	mrb_value key = mrb_str_new_cstr(mrb, func_name);
	mrb_value blk = mrb_hash_get(mrb, g_funcTable, key);
	if (mrb_nil_p(blk)) {
		nysLogUtf8(LogLevel::Warn,
				   (std::string("[mruby] no handler registered for func: ") +
					func_name).c_str());
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
