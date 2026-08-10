//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// nyamy_scripter.cpp - nyamy-scripter DLL implementation
//
// Implements the C API centred on nys_start().
//
// Reads CtrlStream commands from an inherited pipe handle (NYS_CTRL env var).
// Writes compiled setting commands to a CmdStream pipe handle (NYS_CMD env var).
// stdout / stderr are binary UTF-8 log channels (one message per line).
//
// Design -- threading:
//   nys_start() runs two threads.  A ctrl reader thread does nothing but read
//   the ctrl stream and queue Jobs; the calling thread ("script thread") pops
//   Jobs and runs the callbacks, which is where every global below is touched.
//   The split exists so that Quit and the ctrl-pipe EOF are observed even while
//   a script is running, since a script that never returns cannot be
//   interrupted.  When the script thread does not finish in time, the ctrl
//   thread terminates the process (see nys_set_quit_timeout).
//
// Design -- command queueing:
//   nys_* API calls made during on_load_setting push commands into a typed
//   command queue (g_cmdQueue).  When on_load_setting returns true the queue
//   is flushed to the Engine pipe followed by CmdCommit.
//   If on_load_setting returns false the queue is discarded.
//
//   Queue layout:
//     1. g_keyseqEntries -- one entry per nys_reg_keyseq call, stored as
//        (name, compiled CmdActions).  Actions are parsed and compiled at
//        registration time, not at flush time, to amortise the parse cost.
//        Anonymous keyseqs get generated names "__ks{n}__".
//        g_keyseqEntries and g_keyseqByName survive a successful CmdCommit
//        so that nys_get_keyseq_idx() remains valid during on_exec_user_func.
//
//     2. g_cmdQueue -- sequential list of QueueEntry items, each of which is
//        either a direct CmdArgs struct (written verbatim to the stream) or
//        an Include directive (compiled at flush time with initialKeySeqIdx
//        set to the pre-registered keyseq count).
//
//   Modifier-prefix bit resolution and scan-code parsing are done at API
//   call time using the public static helpers MayuCompiler::compileModifierSpecs
//   and MayuParser::parseModifiedKey / parseScanCode.


#define _NYAMY_SCRIPTER_IMPL
#include "nyamy_scripter.h"  // must come first so NYS_API = dllexport before nys_types.h re-includes it

#include "misc.h"
#include "nys_types.h"

#include "ctrl_stream_reader.h"
#include "cmd_stream_writer.h"
#include "config_files.h"
#include "mayu_parser.h"
#include "mayu_compiler.h"

#include "pipe_streambuf.h"

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <fcntl.h>
#include <io.h>
#include <mutex>
#include <set>
#include <sstream>
#include <thread>
#include <unordered_map>
#include <map>
#include <utility>
#include <vector>
#include <string>
#include <windows.h>


//=============================================================================
// Logging helpers
//=============================================================================

// Two thresholds, kept apart on purpose.  nyamy publishes one whenever the
// "detail" box is toggled; the script sets the other through log.level=.  The
// effective threshold is whichever is stricter, recomputed on every call, so a
// script that logs at debug can still be silenced and un-silenced from nyamy
// without the script's own setting being lost.  Comparing two ints is far
// below the noise floor of everything else here, so nothing is cached.
static std::atomic<LogLevel> g_logLevelFromNyamy{kLogLevelNormal};
static std::atomic<LogLevel> g_logLevelFromScript{LogLevel::Debug};


NYS_API LogLevel nysEffectiveLogLevel()
{
	LogLevel a = g_logLevelFromNyamy.load(std::memory_order_relaxed);
	LogLevel b = g_logLevelFromScript.load(std::memory_order_relaxed);
	return (a < b) ? a : b;
}


NYS_API void nysSetLogLevelFromNyamy(LogLevel level)
{
	g_logLevelFromNyamy.store(level, std::memory_order_relaxed);
}


NYS_API void nysSetLogLevelFromScript(LogLevel level)
{
	g_logLevelFromScript.store(level, std::memory_order_relaxed);
}


NYS_API bool nysWouldLog(LogLevel level)
{
	return level <= nysEffectiveLogLevel();
}


// Write one tagged line to stderr.  nyamy strips the tag, turns it back into a
// level and prefixes the timestamp; an untagged line is taken as info, which
// is what a bare puts from a user script produces.
static void logLine(LogLevel level, const std::wstring& msg)
{
	if (!nysWouldLog(level))
		return;
	std::string utf8;
	utf8 += static_cast<char>(logLevelChar(level));
	utf8 += '|';
	utf8 += to_UTF8(msg);
	utf8 += '\n';
	fwrite(utf8.c_str(), 1, utf8.size(), stderr);
}


static void logLine(const std::wstring& msg)
{
	logLine(LogLevel::Info, msg);
}


// Split on newlines so that every physical line carries its own tag; an mruby
// backtrace arrives as a single multi-line string.
NYS_API void nysLogUtf8(LogLevel level, const char* msg)
{
	if (!msg || !nysWouldLog(level))
		return;
	std::wstring w = from_UTF8(msg);
	size_t begin = 0;
	for (;;) {
		size_t nl = w.find(L'\n', begin);
		std::wstring line = w.substr(begin, (nl == std::wstring::npos)
									 ? std::wstring::npos : nl - begin);
		if (!line.empty() && line.back() == L'\r')
			line.pop_back();
		logLine(level, line);
		if (nl == std::wstring::npos)
			break;
		begin = nl + 1;
		if (begin >= w.size())
			break;	// a trailing newline terminates, it is not a blank line
	}
}


class Utf8LineWStreambuf : public std::wstreambuf
{
public:
	explicit Utf8LineWStreambuf(LogLevel level = LogLevel::Info)
			: m_level(level) {}
	~Utf8LineWStreambuf() { flush(); }
	void flush() {
		if (!m_buf.empty()) { logLine(m_level, m_buf); m_buf.clear(); }
	}
protected:
	int_type overflow(int_type c) override {
		if (c == traits_type::eof()) { flush(); return traits_type::eof(); }
		wchar_t wc = static_cast<wchar_t>(c);
		if (wc == L'\n') { logLine(m_level, m_buf); m_buf.clear(); }
		else              m_buf += wc;
		return c;
	}
	std::streamsize xsputn(const wchar_t* s, std::streamsize n) override {
		for (std::streamsize i = 0; i < n; ++i) overflow(s[i]);
		return n;
	}
private:
	LogLevel     m_level;
	std::wstring m_buf;
};


//=============================================================================
// Internal global state
//=============================================================================

namespace {

// Keyseq registry: one entry per registered sequence (index == virtual index).
// name:     internal name (user-supplied, or "__ks{n}__" for anonymous).
// compiled: action list compiled at registration time; cleared after CmdCommit.
struct KeyseqEntry {
	std::wstring          name;
	std::vector<CmdAction> compiled;
};
static std::vector<KeyseqEntry>             g_keyseqEntries;
// name -> virtual index (persists across CmdCommit for nys_get_keyseq_idx)
static std::unordered_map<std::string, int> g_keyseqByName;

// Command queue: direct CmdArgs items or include-file directives.
// Processed in order by flushQueue().
struct QueueEntry {
	enum class Kind { Direct, Include } kind;
	CmdArgs       cmd;         // for Direct
	std::wstring  includePath; // for Include (fully resolved path)

	static QueueEntry makeDirect(CmdArgs c) {
		QueueEntry e; e.kind = Kind::Direct; e.cmd = std::move(c); return e;
	}
	static QueueEntry makeInclude(std::wstring p) {
		QueueEntry e; e.kind = Kind::Include; e.includePath = std::move(p); return e;
	}
};
static std::vector<QueueEntry> g_cmdQueue;

// Callback state
enum class CallbackState { None, LoadSetting, ExecUserFunc };
static CallbackState g_callbackState = CallbackState::None;

// Current trigger context (used by nys_exec_keyseq)
static const TriggerInfo* g_currentTrigger = nullptr;

// Active CmdStreamWriter / ostream (valid while nys_start is running)
static CmdStreamWriter* g_dataWriter = nullptr;
static std::ostream*    g_dataStream = nullptr;

// Symbol set received from the most recent Start command
static Symbols   g_symbols;
static wstringi  g_configName;
static wstringi  g_configPath;

// User-function registry: funcName -> handler
static std::unordered_map<std::string, nys_on_exec_user_func> g_userFuncs;

// Key-name -> scan-code WORD table, populated alongside nys_def_key.
// The WORD is (prefix<<8)|code, prefix 0x00/0xE0/0xE1 (see cmdScanToWord).
// Used by nys_sc_resolve to turn a defined key name into its first scan code.
// Case-insensitive lookup via wstringi's comparator; cleared on reset.
static std::map<wstringi, uint16_t> g_keyNameToScan;

// Scan-code WORD set from the most recent "def option nls-keys" value,
// re-parsed by nys_def_option so nys_is_nls_key_word can answer without
// waiting for the downstream Setting build.  Cleared on reset.
static std::set<uint16_t> g_nlsKeys;

// Cached registry Scancode Map, as (from, to) WORD pairs.
// Read lazily on first query (g_scancodeMapLoaded guards the one-shot read),
// discarded by resetQueue so a fresh setting load re-reads the registry.
static std::vector<std::pair<uint16_t, uint16_t>> g_scancodeMap;
static bool g_scancodeMapLoaded = false;

// Scan-code extension flag bits.  These mirror ScanCode::E0 / ScanCode::E1 in
// keyboard.h (via KEYBOARD_INPUT_DATA), which is exactly what
// CmdScanCode.flags carries after MayuCompiler::compileScanCode.  Duplicated
// here as small constants to avoid pulling the engine headers into the DLL.
static const uint16_t kScanFlagE0 = 2;
static const uint16_t kScanFlagE1 = 4;

// Pack a CmdScanCode into the (prefix<<8)|code WORD form used by the DSL and
// the registry Scancode Map (high byte 0xE0/0xE1 for extended keys).
static uint16_t cmdScanToWord(const CmdScanCode& sc)
{
	uint16_t prefix = 0;
	if (sc.flags & kScanFlagE1)      prefix = 0xE100;
	else if (sc.flags & kScanFlagE0) prefix = 0xE000;
	return static_cast<uint16_t>(prefix | (sc.scan & 0xFF));
}

// Last error string (UTF-8)
static std::string g_lastError;

// RAII allocator for NYsFuncArgs / NYsStrs created during a callback session.
// A stack instance is live during on_load_setting and on_exec_user_func;
// g_sessionAlloc points to it so nys_*_new can register allocations.
struct SessionAllocator {
	std::vector<std::unique_ptr<NYsFuncArgs>> funcArgs;
	std::vector<std::unique_ptr<NYsStrs>>     strs;

	NYsFuncArgs* newFuncArgs() {
		auto p = std::make_unique<NYsFuncArgs>();
		auto* r = p.get();
		funcArgs.push_back(std::move(p));
		return r;
	}
	NYsStrs* newStrs() {
		auto p = std::make_unique<NYsStrs>();
		auto* r = p.get();
		strs.push_back(std::move(p));
		return r;
	}
};
static SessionAllocator* g_sessionAlloc = nullptr;

// Milliseconds to wait for the script thread after Quit; 0 disables the kill.
// Written before nys_start, read by the ctrl thread.
static std::atomic<uint32_t> g_quitTimeoutMillisec{ 0 };


//-----------------------------------------------------------------------------
// Job queue: ctrl reader thread (producer) -> script thread (consumer)
//-----------------------------------------------------------------------------

// A unit of work handed between the two threads.  Both payloads are
// self-contained -- readExecUserFunc allocates no NYsStrs -- so a Job can cross
// threads without the session allocator, which stays on the script thread.
struct Job {
	enum class Kind { Start, ExecUserFunc, Quit };
	Kind                 kind = Kind::Quit;
	CtrlArgsStart        start;   // Kind::Start
	CtrlArgsExecUserFunc exec;    // Kind::ExecUserFunc
};

// Upper bound on queued ExecUserFunc jobs.  The ctrl pipe used to provide the
// back pressure by itself: nyamy writes it in PIPE_NOWAIT mode and drops when
// it is full.  The reader now drains the pipe unconditionally, so the bound has
// to live here instead - otherwise a script blocked inside a user function
// would let key events accumulate for ever.
static const size_t kMaxPendingExecUserFunc = 64;

class JobQueue
{
public:
	// Queue a setting load, dropping the ExecUserFunc jobs queued before it:
	// they belong to the setting being replaced, and the handler table is
	// cleared by the load, so at best they would do nothing and at worst reach
	// a same-named handler of the new setting.
	void pushStart(Job i_job) {
		std::lock_guard<std::mutex> lock(m_mutex);
		m_jobs.erase(std::remove_if(m_jobs.begin(), m_jobs.end(),
			[](const Job &j) { return j.kind == Job::Kind::ExecUserFunc; }),
			m_jobs.end());
		m_jobs.push_back(std::move(i_job));
		m_cond.notify_one();
	}

	// Returns false when the queue is full and the job was dropped.  The newest
	// is dropped rather than the oldest so that what does run stays in order.
	bool pushExec(Job i_job) {
		std::lock_guard<std::mutex> lock(m_mutex);
		size_t pending = 0;
		for (const Job &j : m_jobs)
			if (j.kind == Job::Kind::ExecUserFunc) ++pending;
		if (pending >= kMaxPendingExecUserFunc)
			return false;
		m_jobs.push_back(std::move(i_job));
		m_cond.notify_one();
		return true;
	}

	// Queued at the back, never jumped ahead: whatever was queued before Quit
	// still runs.  The watchdog, not the queue, bounds how long that may take.
	void pushQuit() {
		std::lock_guard<std::mutex> lock(m_mutex);
		Job j;
		j.kind = Job::Kind::Quit;
		m_jobs.push_back(std::move(j));
		m_cond.notify_one();
	}

	Job pop() {
		std::unique_lock<std::mutex> lock(m_mutex);
		m_cond.wait(lock, [this] { return !m_jobs.empty(); });
		Job j = std::move(m_jobs.front());
		m_jobs.pop_front();
		return j;
	}

private:
	std::mutex              m_mutex;
	std::condition_variable m_cond;
	std::deque<Job>         m_jobs;
};

} // namespace


//=============================================================================
// Internal utilities
//=============================================================================

static bool setError(const std::string& msg)
{
	g_lastError = msg;
	logLine(L"[nys] error: " + from_UTF8(msg));
	return false;
}

// Assign the next virtual index for a new key sequence.
static int allocVidx(const char* name)
{
	int vidx = static_cast<int>(g_keyseqEntries.size());
	KeyseqEntry e;
	if (name && *name) {
		e.name = from_UTF8(name);
		g_keyseqByName[name] = vidx;
	} else {
		// anonymous: generate an internal name
		e.name = L"__ks" + std::to_wstring(vidx) + L"__";
	}
	g_keyseqEntries.push_back(std::move(e));
	return vidx;
}

// Discard all queued commands and reset name/func tables.
// Called before a fresh setting load (pre-commit only).
static void resetQueue()
{
	g_keyseqEntries.clear();
	g_keyseqByName.clear();
	g_cmdQueue.clear();
	g_userFuncs.clear();
	g_keyNameToScan.clear();
	g_nlsKeys.clear();
	g_scancodeMap.clear();
	g_scancodeMapLoaded = false;
}

// Release queue buffers after a successful CmdCommit.
// g_keyseqEntries.name and g_keyseqByName persist so that
// nys_get_keyseq_idx() remains valid during on_exec_user_func calls.
static void clearQueueBuffers()
{
	for (auto& e : g_keyseqEntries)
		e.compiled.clear();
	g_cmdQueue.clear();
}

// Parse "!Shift", "!!!Ctrl" etc. into (assignMode, name).
// assignMode = leading '!' characters; name = remainder.
static std::pair<wstringi, wstringi> splitAssignEntry(const char* p, size_t len)
{
	size_t n = 0;
	const char* q = p;
	while (*q == '!') { ++n; ++q; }
	return { wstringi(n, L'!'), from_UTF8(std::string(q, len - n)) };
}

// Build a CmdModifiedKey from a modifier-key string (e.g. "C-A", "*-LButton").
// Returns false on parse failure; sets g_lastError.
static bool parseModifiedKey(const char* funcName,
	const char* str, size_t len, CmdModifiedKey& out)
{
	std::vector<AstModifierSpec> mods;
	wstringi keyName;
	if (!MayuParser::parseModifiedKey(wstringi(from_UTF8(std::string(str, len))),
	                                  mods, keyName))
		return setError(std::string(funcName) + ": invalid modifier key: "
		                + std::string(str, len));
	// a key being assigned to may carry every modifier type, so nothing here
	// can be out of context
	out.modifier =
		MayuCompiler::compileModifierSpecs(mods, ModifierContext::Assign);
	out.keyName  = keyName;
	return true;
}

// Build a CmdScanCode from a scan-code string (e.g. "0x1c", "E0-0x1c").
static bool parseScanCode(const char* funcName,
	const char* str, size_t len, CmdScanCode& out)
{
	AstScanCode sc;
	if (!MayuParser::parseScanCode(wstringi(from_UTF8(std::string(str, len))), sc))
		return setError(std::string(funcName) + ": invalid scan code: "
		                + std::string(str, len));
	out = MayuCompiler::compileScanCode(sc);
	return true;
}

// Flush all queued commands to the data stream and write CmdCommit.
// Returns false on error (stream not flushed).
//
// The consumer defers reference resolution until CmdCommit (keyseq contents
// and substitutes are materialized only after the whole stream has been
// received), so key definitions, keyseqs and keymaps may appear in any
// order.  The only ordering constraint is that RegKeySeq must precede the
// commands that reference a keyseq by index, hence step 2 before step 3.
static bool flushQueue()
{
	if (!g_dataWriter) return false;

	Utf8LineWStreambuf logBuf;
	std::wostream logStream(&logBuf);
	ConfigFiles cf;

	// Step 0: Reset opens the setting definition block.  The consumer starts
	// a fresh Setting here, which also discards the partial block left behind
	// when a previous flush failed halfway (no Commit written).
	g_dataWriter->writeReset();

	// Step 1: DefSymbol commands for the current symbol set.
	for (const auto& sym : g_symbols) {
		CmdArgsDefSymbol d;
		d.symbolName = sym;
		g_dataWriter->writeDefSymbol(d);
	}

	// Step 2: Pre-registered keyseqs (indices 0 .. n-1).
	for (const auto& e : g_keyseqEntries) {
		CmdArgsRegKeySeq ks;
		ks.name    = wstringi(e.name);
		ks.mode    = 0;
		ks.actions = e.compiled;
		g_dataWriter->writeRegKeySeq(ks);
	}

	// Step 3: Remaining commands in queue order.
	// Include entries are compiled with initialKeySeqIdx = current keyseq count.
	uint32_t nextKeySeqIdx = static_cast<uint32_t>(g_keyseqEntries.size());

	for (const auto& entry : g_cmdQueue) {
		if (entry.kind == QueueEntry::Kind::Direct) {
			std::visit(overloaded{
				[](const CmdArgsRegKeySeq&)   {},  // not placed in cmdQueue
				[](const CmdArgsExecKeySeq&)  {},  // not placed in cmdQueue
				[](const CmdArgsReset&)       {},  // not placed in cmdQueue
				[](const CmdArgsCommit&)      {},  // not placed in cmdQueue
				[](const CmdArgsDefKey& a)    { g_dataWriter->writeDefKey(a); },
				[](const CmdArgsDefMod& a)    { g_dataWriter->writeDefMod(a); },
				[](const CmdArgsDefSync& a)   { g_dataWriter->writeDefSync(a); },
				[](const CmdArgsDefAlias& a)  { g_dataWriter->writeDefAlias(a); },
				[](const CmdArgsDefSubst& a)  { g_dataWriter->writeDefSubst(a); },
				[](const CmdArgsDefOption& a) { g_dataWriter->writeDefOption(a); },
				[](const CmdArgsDefSymbol& a) { g_dataWriter->writeDefSymbol(a); },
				[](const CmdArgsBeginKeymap& a){ g_dataWriter->writeBeginKeymap(a); },
				[](const CmdArgsAssignKey& a) { g_dataWriter->writeAssignKey(a); },
				[](const CmdArgsAssignEvent& a){ g_dataWriter->writeAssignEvent(a); },
				[](const CmdArgsAssignMod& a) { g_dataWriter->writeAssignMod(a); },
			}, entry.cmd);
		} else {
			// Include: parse + compile with the current keyseq count.
			MayuParser parser;
			auto ast = parser.parseFile(wstringi(entry.includePath), cf);
			if (parser.hasErrors()) {
				for (const auto& msg : parser.getMessages())
					logLine(msg);
				return false;
			}
			if (ast) {
				MayuCompiler compiler(*g_dataWriter, g_symbols, cf,
				                      nullptr, &logStream);
				compiler.compile(*ast, nextKeySeqIdx, /*writeSymbols=*/false);
				if (compiler.hasErrors()) {
					logLine(L"[nys] error: compile failed.");
					return false;
				}
				nextKeySeqIdx = compiler.nextKeySeqIdx();
			}
		}
	}

	g_dataWriter->writeCommit();
	g_dataStream->flush();
	return true;
}

// Guard: return false if not called from within on_load_setting.
static bool checkInLoadSetting(const char* funcName)
{
	if (g_callbackState != CallbackState::LoadSetting) {
		g_lastError = std::string(funcName) + ": must be called from on_load_setting";
		return false;
	}
	return true;
}

// Guard: return false if not called from within any scripter callback.
static bool checkInCallback(const char* funcName)
{
	if (g_callbackState == CallbackState::None) {
		g_lastError = std::string(funcName) + ": must be called from a scripter callback";
		return false;
	}
	return true;
}



NYS_API NYsFuncArgs* nys_func_args_new(void) {
	if (!g_sessionAlloc) {
		setError("nys_func_args_new: must be called from a scripter callback");
		return nullptr;
	}
	return g_sessionAlloc->newFuncArgs();
}
NYS_API NYsStrs* nys_strs_new(void) {
	if (!g_sessionAlloc) {
		setError("nys_strs_new: must be called from a scripter callback");
		return nullptr;
	}
	return g_sessionAlloc->newStrs();
}


NYS_API int nys_func_args_length(const NYsFuncArgs* fas)
{
	if (!fas) return 0;
	return static_cast<int>(fas->entries.size());
}

NYS_API int nys_strs_length(const NYsStrs* ss)
{
	if (!ss) return 0;
	return static_cast<int>(ss->strs.size());
}

NYS_API NYsType nys_func_args_get(const NYsFuncArgs* fas, int idx,
	int64_t* p_value, int64_t* p_length)
{
	if (!fas || idx < 0 || idx >= static_cast<int>(fas->entries.size()))
		return NYsType_Error;
	const NYsFuncArg& e = fas->entries[idx];
	switch (e.type) {
	case NYsType_String:
	case NYsType_Regexp:
		*p_value  = reinterpret_cast<int64_t>(e.str.c_str());
		*p_length = static_cast<int64_t>(e.str.size());
		return e.type;
	case NYsType_Number:
	case NYsType_KeySeqIdx:
		*p_value  = e.numval;
		*p_length = 0;
		return e.type;
	case NYsType_ModifierSpec:
		*p_value  = e.numval;
		*p_length = e.numval2;
		return e.type;
	case NYsType_TokenSeq:
		*p_value  = reinterpret_cast<int64_t>(e.strs);
		*p_length = e.strs ? static_cast<int64_t>(e.strs->strs.size()) : 0;
		return e.type;
	default:
		return NYsType_Error;
	}
}

NYS_API bool nys_strs_get(const NYsStrs* ss, int idx,
	const char** p_value, size_t* p_length)
{
	if (!ss || idx < 0 || idx >= static_cast<int>(ss->strs.size())) return false;
	*p_value  = ss->strs[idx].c_str();
	*p_length = ss->strs[idx].size();
	return true;
}

NYS_API bool nys_func_args_push(NYsFuncArgs* fas, NYsType type,
	int64_t value, int64_t length)
{
	if (!fas) return false;

	NYsFuncArg e;
	e.type = type;
	switch (type) {
	case NYsType_String:
	case NYsType_Regexp: {
		const char* p = reinterpret_cast<const char*>(static_cast<uintptr_t>(value));
		e.str.assign(p, static_cast<size_t>(length));
		break;
	}
	case NYsType_Number:
		e.numval = static_cast<int32_t>(value);
		break;
	case NYsType_KeySeqIdx:
		e.numval = static_cast<uint32_t>(value);
		break;
	case NYsType_ModifierSpec:
		e.numval  = value;
		e.numval2 = length;
		break;
	case NYsType_TokenSeq: {
		const NYsStrs* ss = reinterpret_cast<const NYsStrs*>(static_cast<uintptr_t>(value));
		e.strs = g_sessionAlloc->newStrs();
		if (ss) e.strs->strs = ss->strs;
		break;
	}
	default:
		return false;
	}
	fas->entries.push_back(std::move(e));
	return true;
}

NYS_API bool nys_strs_push(NYsStrs* ss, const char* value, size_t length)
{
	if (!ss || !value) return false;
	ss->strs.emplace_back(value, length);
	return true;
}




//=============================================================================
// nys_start -- main event loop
//=============================================================================

// Defined with the scan-code query API below, called once per Start command.
static void defineScancodeMapSymbols();

NYS_API int nys_start(const NYsCallbacks* callbacks, void* exeCtx)
{
	if (!callbacks || !callbacks->on_load_setting) return 1;

	// Retrieve pipe handles from environment variables set by the parent process.
	HANDLE hCtrlRead  = INVALID_HANDLE_VALUE;
	HANDLE hDataWrite = INVALID_HANDLE_VALUE;
	{
		wchar_t buf[32];
		if (GetEnvironmentVariableW(L"NYS_CTRL", buf, 32) > 0)
			hCtrlRead  = reinterpret_cast<HANDLE>(
				static_cast<uintptr_t>(wcstoull(buf, nullptr, 10)));
		if (GetEnvironmentVariableW(L"NYS_CMD",  buf, 32) > 0)
			hDataWrite = reinterpret_cast<HANDLE>(
				static_cast<uintptr_t>(wcstoull(buf, nullptr, 10)));
	}

	_setmode(_fileno(stdout), _O_BINARY);
	_setmode(_fileno(stderr), _O_BINARY);

	if (hCtrlRead == INVALID_HANDLE_VALUE || hDataWrite == INVALID_HANDLE_VALUE) {
		logLine(L"error: NYS_CTRL and NYS_CMD environment variables are required");
		return 1;
	}

	PipeReadStreambuf  ctrlBuf(hCtrlRead);
	PipeWriteStreambuf dataBuf(hDataWrite);
	std::istream ctrlStream(&ctrlBuf);
	std::ostream dataStream(&dataBuf);

	CmdStreamWriter dataWriter(dataStream);
	g_dataWriter = &dataWriter;
	g_dataStream = &dataStream;

	CtrlStreamReader ctrlReader(ctrlStream);

	JobQueue queue;

	// Signalled by the script thread once it has run the Quit job.  Manual
	// reset: the ctrl thread may get here after it is already set.
	HANDLE hScriptDone = CreateEvent(NULL, TRUE, FALSE, NULL);

	std::thread ctrlThread([&]() {
		// Nothing but reading and queueing happens here, so Quit and the
		// ctrl-pipe EOF are observed even while a script is running.
		try {
			for (;;) {
				CtrlId id;
				if (!ctrlReader.readNext(id))
					break;                        // EOF: nyamy closed the pipe
				if (id == CtrlId::Quit) {
					break;
				} else if (id == CtrlId::Start) {
					Job j;
					j.kind  = Job::Kind::Start;
					j.start = ctrlReader.readStart();
					queue.pushStart(std::move(j));
				} else if (id == CtrlId::ExecUserFunc) {
					Job j;
					j.kind = Job::Kind::ExecUserFunc;
					j.exec = ctrlReader.readExecUserFunc();
					wstringi name = j.exec.name;
					if (!queue.pushExec(std::move(j)))
						logLine(LogLevel::Warn,
								L"[nys] job queue full; discarded ExecUserFunc("
								+ std::wstring(name) + L")");
				} else if (id == CtrlId::SetLogLevel) {
					// Applied straight from the ctrl thread: it only stores an
					// atomic, and going through the job queue would leave it
					// waiting behind a running script.
					nysSetLogLevelFromNyamy(ctrlReader.readSetLogLevel());
				}
			}
		} catch (...) {
			// truncated payload: treat as end of stream
		}
		queue.pushQuit();

		// A script that never returns cannot be interrupted, so the only way to
		// stop is to die - and if nyamy went down first, nobody else is going
		// to.  TerminateProcess rather than ExitProcess: ExitProcess would try
		// to unwind a thread that is still holding CRT and loader locks.
		uint32_t timeout = g_quitTimeoutMillisec.load();
		if (timeout > 0 &&
		    WaitForSingleObject(hScriptDone, timeout) == WAIT_TIMEOUT) {
			logLine(L"[nys] script did not stop within "
			        + std::to_wstring(timeout) + L" ms after Quit; terminating.");
			fflush(stderr);
			TerminateProcess(GetCurrentProcess(), 2);
		}
	});

	for (;;) {
		Job job = queue.pop();

		if (job.kind == Job::Kind::Quit) {
			if (callbacks->on_quit) callbacks->on_quit(exeCtx);
			break;
		} else if (job.kind == Job::Kind::Start) {
			g_configName = std::move(job.start.configName);
			g_configPath = std::move(job.start.configPath);
			g_symbols    = std::move(job.start.symbols);
			// The threshold rides along with Start because the log dialog
			// restores its "detail" state from the ini, so detail can already
			// be on before the scripter has written its first line.
			nysSetLogLevelFromNyamy(job.start.logLevel);

			resetQueue();
			// Both .mayu and .mayu.rb branch on these, and the .mayu compiler
			// reads the symbol set in flushQueue, so they have to be settled
			// before on_load_setting runs.  resetQueue dropped the cached map,
			// so this re-reads the registry for the new setting.
			defineScancodeMapSymbols();
			g_callbackState = CallbackState::LoadSetting;
			g_lastError.clear();
			{
				SessionAllocator sa;
				g_sessionAlloc = &sa;
				bool ok = callbacks->on_load_setting(exeCtx);
				g_sessionAlloc = nullptr;
				// sa goes out of scope here, freeing all NYsFuncArgs/NYsStrs
				g_callbackState = CallbackState::None;
				if (ok) {
					if (!flushQueue())
						logLine(L"[nys] error: flushQueue failed; setting not applied.");
					else
						clearQueueBuffers();
				} else {
					resetQueue();
					logLine(L"[nys] on_load_setting returned false; setting discarded.");
				}
			}
		} else {
			SessionAllocator sa;
			g_sessionAlloc = &sa;
			CtrlArgsExecUserFunc &req = job.exec;
			std::string funcName = to_UTF8(std::wstring(req.name));
			auto it = g_userFuncs.find(funcName);
			if (it != g_userFuncs.end() && it->second) {
				g_callbackState = CallbackState::ExecUserFunc;
				g_currentTrigger = &req.context;
				it->second(exeCtx, funcName.c_str(), &req.args);
				g_currentTrigger = nullptr;
				g_callbackState = CallbackState::None;
				dataStream.flush();
			}
			g_sessionAlloc = nullptr;
			// sa goes out of scope here, freeing all NYsFuncArgs/NYsStrs
		}
	}

	SetEvent(hScriptDone);
	ctrlThread.join();
	CloseHandle(hScriptDone);

	g_dataWriter = nullptr;
	g_dataStream = nullptr;

	CloseHandle(hCtrlRead);
	CloseHandle(hDataWrite);
	return 0;
}


NYS_API void nys_set_quit_timeout(uint32_t millisec)
{
	g_quitTimeoutMillisec.store(millisec);
}

// FFI compatibility version.  While NYamy is 0.9.x this tracks NYamy's own
// version; it is pinned to 1.0.0 when NYamy 1.0.0 ships and only moves after
// that when this C API changes.  Deliberately not derived from the VERSION
// macro: the two stop tracking each other at 1.0.0.
NYS_API uint32_t nys_version(void)
{
	return (0u << 16) | (9u << 8) | 0u;
}


//=============================================================================
// Setting registration API
//=============================================================================

// Remap FuncArgKeySeqIdx values in compiled actions using a local-index ->
// global-vidx map.  Used to fix up inline key-sequence literals after each
// sub-sequence has been registered as its own keyseq entry.
static void remapKeySeqIdx(std::vector<CmdAction>& actions,
	const std::vector<int>& localToGlobal)
{
	for (auto& a : actions) {
		for (auto& arg : a.arguments) {
			if (std::holds_alternative<FuncArgKeySeqIdx>(arg)) {
				uint32_t v = std::get<FuncArgKeySeqIdx>(arg);
				if (v < localToGlobal.size() && localToGlobal[v] >= 0)
					arg = FuncArgKeySeqIdx{ static_cast<uint32_t>(localToGlobal[v]) };
			}
		}
		if (!a.subActions.empty())
			remapKeySeqIdx(a.subActions, localToGlobal);
	}
}


NYS_API int nys_reg_keyseq(const char* name, const char* actions, int context)
{
	if (!checkInLoadSetting("nys_reg_keyseq")) return -1;
	if (!actions || !*actions) {
		setError("nys_reg_keyseq: actions must not be empty");
		return -1;
	}
	ModifierContext modContext = context == NYS_MODCTX_ASSIGN
		? ModifierContext::Assign : ModifierContext::KeySeq;

	// Parse and compile the action string at registration time.
	std::wstring wactions = from_UTF8(actions);
	MayuParser parser;
	auto seq = parser.parseActions(wactions.c_str(), wactions.size());
	if (parser.hasErrors() || !seq) {
		setError("nys_reg_keyseq: failed to parse actions");
		return -1;
	}

	// Compile via a null-stream compiler.  Inline KeySeqLiteral arguments
	// (parenthesised sub-sequences used as function args) are collected so each
	// can be registered as its own keyseq entry below, then referenced by index.
	std::ostringstream nullSink;
	CmdStreamWriter nullWriter(nullSink);
	ConfigFiles cf;
	MayuCompiler compiler(nullWriter, g_symbols, cf, nullptr, nullptr);
	std::vector<std::vector<CmdAction>> subSeqs;
	compiler.setSubSeqCollector(&subSeqs);
	std::vector<CmdAction> compiled = compiler.compileActions(*seq, modContext);
	compiler.setSubSeqCollector(nullptr);
	if (compiler.hasErrors()) {
		setError("nys_reg_keyseq: failed to compile actions");
		return -1;
	}

	// Register each collected sub-sequence as an anonymous keyseq.  Sub-sequences
	// are in post-order, so sub-sequence i only references earlier ones; remap its
	// indices (already-global) before storing, then record its own global index.
	// Sub-sequences are allocated before the main keyseq so that flushQueue writes
	// them first and the consumer can resolve the references by index.
	std::vector<int> localToGlobal(subSeqs.size(), -1);
	for (size_t i = 0; i < subSeqs.size(); ++i) {
		remapKeySeqIdx(subSeqs[i], localToGlobal);
		int sub = allocVidx(nullptr);
		g_keyseqEntries[sub].compiled = std::move(subSeqs[i]);
		localToGlobal[i] = sub;
	}
	remapKeySeqIdx(compiled, localToGlobal);

	// Update existing entry when name is already registered.
	if (name && *name) {
		auto it = g_keyseqByName.find(name);
		if (it != g_keyseqByName.end()) {
			g_keyseqEntries[it->second].compiled = std::move(compiled);
			return it->second;
		}
	}

	int vidx = allocVidx(name);
	g_keyseqEntries[vidx].compiled = std::move(compiled);
	return vidx;
}

NYS_API int nys_get_keyseq_idx(const char* name)
{
	if (!name || !*name) return -1;
	auto it = g_keyseqByName.find(name);
	return (it != g_keyseqByName.end()) ? it->second : -1;
}

// Shared by nys_sc_resolve and the "nls-keys" option parser: a key name
// defined by a prior nys_def_key takes priority, falling back to a
// scan-code literal ("0x1c", "E0-0x1c", "28").
static bool resolveScanWord(const std::string& str, uint16_t* out)
{
	if (str.empty()) return false;

	auto it = g_keyNameToScan.find(wstringi(from_UTF8(str)));
	if (it != g_keyNameToScan.end()) {
		*out = it->second;
		return true;
	}

	AstScanCode sc;
	if (!MayuParser::parseScanCode(wstringi(from_UTF8(str)), sc))
		return false;
	*out = cmdScanToWord(MayuCompiler::compileScanCode(sc));
	return true;
}

// "0x3a, E0-0x29, 112, NLS-1" -> a set of scan-code WORDs, mirroring
// CmdProcessor::parseNlsKeys.  Separators are commas and whitespace.
// On an unresolvable item the set is left empty, matching the downstream
// Setting build's behavior of discarding the whole option on error.
static void parseNlsKeysOption(const std::string& value)
{
	g_nlsKeys.clear();

	size_t i = 0, n = value.size();
	while (i < n) {
		if (value[i] == ',' || value[i] == ' ' || value[i] == '\t') { ++i; continue; }

		size_t begin = i;
		while (i < n && value[i] != ',' && value[i] != ' ' && value[i] != '\t') ++i;

		uint16_t word = 0;
		if (!resolveScanWord(value.substr(begin, i - begin), &word)) {
			g_nlsKeys.clear();
			return;
		}
		g_nlsKeys.insert(word);
	}
}

NYS_API bool nys_def_key(const NYsStrs* names, const NYsStrs* scancodes)
{
	if (!checkInLoadSetting("nys_def_key")) return false;
	if (!names     || nys_strs_length(names)     == 0) return setError("nys_def_key: names is empty");
	if (!scancodes || nys_strs_length(scancodes) == 0) return setError("nys_def_key: scancodes is empty");

	CmdArgsDefKey d;
	int nn = nys_strs_length(names);
	for (int i = 0; i < nn; ++i) {
		const char* p = nullptr; size_t len = 0;
		nys_strs_get(names, i, &p, &len);
		d.names.push_back(wstringi(from_UTF8(std::string(p, len))));
	}
	int ns = nys_strs_length(scancodes);
	for (int i = 0; i < ns; ++i) {
		const char* p = nullptr; size_t len = 0;
		nys_strs_get(scancodes, i, &p, &len);
		CmdScanCode sc;
		if (!parseScanCode("nys_def_key", p, len, sc)) return false;
		d.scanCodes.push_back(sc);
	}

	// Record name -> first-scan-code so nys_sc_resolve can map key names.
	// Later definitions win, matching Keyboard::addKey's last-wins lookup.
	uint16_t word = cmdScanToWord(d.scanCodes[0]);
	for (const auto& n : d.names)
		g_keyNameToScan[n] = word;

	g_cmdQueue.push_back(QueueEntry::makeDirect(std::move(d)));
	return true;
}

NYS_API bool nys_def_mod(const char* modifier_name, const NYsStrs* key_names)
{
	if (!checkInLoadSetting("nys_def_mod")) return false;
	if (!modifier_name || !*modifier_name) return setError("nys_def_mod: modifier_name is empty");
	if (!key_names || nys_strs_length(key_names) == 0) return setError("nys_def_mod: key_names is empty");

	CmdArgsDefMod d;
	d.modifierName = from_UTF8(modifier_name);
	int n = nys_strs_length(key_names);
	for (int i = 0; i < n; ++i) {
		const char* p = nullptr; size_t len = 0;
		nys_strs_get(key_names, i, &p, &len);
		d.keyNames.push_back(wstringi(from_UTF8(std::string(p, len))));
	}
	g_cmdQueue.push_back(QueueEntry::makeDirect(std::move(d)));
	return true;
}

NYS_API bool nys_def_sync(const NYsStrs* scan_codes)
{
	if (!checkInLoadSetting("nys_def_sync")) return false;
	if (!scan_codes || nys_strs_length(scan_codes) == 0) return setError("nys_def_sync: scan_codes is empty");

	CmdArgsDefSync d;
	int n = nys_strs_length(scan_codes);
	for (int i = 0; i < n; ++i) {
		const char* p = nullptr; size_t len = 0;
		nys_strs_get(scan_codes, i, &p, &len);
		CmdScanCode sc;
		if (!parseScanCode("nys_def_sync", p, len, sc)) return false;
		d.scanCodes.push_back(sc);
	}
	g_cmdQueue.push_back(QueueEntry::makeDirect(std::move(d)));
	return true;
}

NYS_API bool nys_def_alias(const char* alias_name, const char* key_name)
{
	if (!checkInLoadSetting("nys_def_alias")) return false;
	if (!alias_name || !*alias_name) return setError("nys_def_alias: alias_name is empty");
	if (!key_name   || !*key_name)   return setError("nys_def_alias: key_name is empty");

	CmdArgsDefAlias d;
	d.aliasName = from_UTF8(alias_name);
	d.keyName   = from_UTF8(key_name);
	g_cmdQueue.push_back(QueueEntry::makeDirect(std::move(d)));
	return true;
}

NYS_API bool nys_def_subst(const NYsStrs* lhs_mod_keys, int rhs_keyseq_idx)
{
	if (!checkInLoadSetting("nys_def_subst")) return false;
	if (!lhs_mod_keys || nys_strs_length(lhs_mod_keys) == 0) return setError("nys_def_subst: lhs_mod_keys is empty");

	CmdArgsDefSubst d;
	d.rhsKeySeqIdx = static_cast<uint32_t>(rhs_keyseq_idx);
	int n = nys_strs_length(lhs_mod_keys);
	for (int i = 0; i < n; ++i) {
		const char* p = nullptr; size_t len = 0;
		nys_strs_get(lhs_mod_keys, i, &p, &len);
		CmdModifiedKey mk;
		if (!parseModifiedKey("nys_def_subst", p, len, mk)) return false;
		d.lhsKeys.push_back(std::move(mk));
	}
	g_cmdQueue.push_back(QueueEntry::makeDirect(std::move(d)));
	return true;
}

NYS_API bool nys_def_option(const char* option_name, const char* value)
{
	if (!checkInLoadSetting("nys_def_option")) return false;
	if (!option_name || !*option_name) return setError("nys_def_option: option_name is empty");
	if (!value)                         return setError("nys_def_option: value is null");

	if (std::strcmp(option_name, "nls-keys") == 0)
		parseNlsKeysOption(value);

	CmdArgsDefOption d;
	d.optionName = from_UTF8(option_name);
	d.value      = from_UTF8(value);
	g_cmdQueue.push_back(QueueEntry::makeDirect(std::move(d)));
	return true;
}

NYS_API bool nys_begin_keymap(const char* keyword, const char* name,
	const char* window_class, const char* window_title,
	const char* op, const char* parent_name,
	int default_keyseq_idx)
{
	if (!checkInLoadSetting("nys_begin_keymap")) return false;
	if (!keyword || !*keyword) return setError("nys_begin_keymap: keyword is empty");
	if (!name    || !*name)    return setError("nys_begin_keymap: name is empty");

	CmdArgsBeginKeymap d;
	d.keyword           = from_UTF8(keyword);
	d.name              = from_UTF8(name);
	d.defaultKeySeqIdx  = default_keyseq_idx;  // -1 = none

	if (window_class && *window_class) {
		d.windowClassName = from_UTF8(window_class);
		if (window_title && *window_title) {
			d.windowTitleName = from_UTF8(window_title);
			d.windowOp = (op && *op) ? from_UTF8(op) : wstringi(L"&&");
		}
	}
	if (parent_name && *parent_name)
		d.parentName = from_UTF8(parent_name);

	g_cmdQueue.push_back(QueueEntry::makeDirect(std::move(d)));
	return true;
}

NYS_API bool nys_assign_key(const NYsStrs* lhs_mod_keys, int rhs_keyseq_idx)
{
	if (!checkInLoadSetting("nys_assign_key")) return false;
	if (!lhs_mod_keys || nys_strs_length(lhs_mod_keys) == 0) return setError("nys_assign_key: lhs_mod_keys is empty");

	CmdArgsAssignKey d;
	d.rhsKeySeqIdx = static_cast<uint32_t>(rhs_keyseq_idx);
	int n = nys_strs_length(lhs_mod_keys);
	for (int i = 0; i < n; ++i) {
		const char* p = nullptr; size_t len = 0;
		nys_strs_get(lhs_mod_keys, i, &p, &len);
		CmdModifiedKey mk;
		if (!parseModifiedKey("nys_assign_key", p, len, mk)) return false;
		d.lhsKeys.push_back(std::move(mk));
	}
	g_cmdQueue.push_back(QueueEntry::makeDirect(std::move(d)));
	return true;
}

NYS_API bool nys_assign_event(const char* event_name, int rhs_keyseq_idx)
{
	if (!checkInLoadSetting("nys_assign_event")) return false;
	if (!event_name || !*event_name) return setError("nys_assign_event: event_name is empty");

	CmdArgsAssignEvent d;
	d.eventName     = from_UTF8(event_name);
	d.rhsKeySeqIdx  = static_cast<uint32_t>(rhs_keyseq_idx);
	g_cmdQueue.push_back(QueueEntry::makeDirect(std::move(d)));
	return true;
}

NYS_API bool nys_assign_mod(const NYsStrs* prefixes, const char* modifier_name,
	const char* op, const NYsStrs* keys)
{
	if (!checkInLoadSetting("nys_assign_mod")) return false;
	if (!modifier_name || !*modifier_name) return setError("nys_assign_mod: modifier_name is empty");
	if (!op || !*op)                        return setError("nys_assign_mod: op is empty");
	if (!keys || nys_strs_length(keys) == 0) return setError("nys_assign_mod: keys is empty");

	CmdArgsAssignMod d;
	d.mainModifierName = from_UTF8(modifier_name);
	d.op               = from_UTF8(op);

	if (prefixes) {
		int n = nys_strs_length(prefixes);
		for (int i = 0; i < n; ++i) {
			const char* p = nullptr; size_t len = 0;
			nys_strs_get(prefixes, i, &p, &len);
			auto [mode, mname] = splitAssignEntry(p, len);
			CmdArgsAssignMod::PrefixMod pm;
			pm.assignMode   = mode;
			pm.modifierName = mname;
			d.prefixes.push_back(pm);
		}
	}
	int nk = nys_strs_length(keys);
	for (int i = 0; i < nk; ++i) {
		const char* p = nullptr; size_t len = 0;
		nys_strs_get(keys, i, &p, &len);
		auto [mode, kname] = splitAssignEntry(p, len);
		CmdArgsAssignMod::KeyEntry ke;
		ke.assignMode = mode;
		ke.keyName    = kname;
		d.keys.push_back(ke);
	}
	g_cmdQueue.push_back(QueueEntry::makeDirect(std::move(d)));
	return true;
}

NYS_API bool nys_reg_user_func(const char* func_name, nys_on_exec_user_func on_exec_user_func)
{
	if (!checkInLoadSetting("nys_reg_user_func")) return false;
	if (!func_name || !*func_name) return setError("nys_reg_user_func: func_name is empty");
	if (!on_exec_user_func) return setError("nys_reg_user_func: handler is null");

	g_userFuncs[func_name] = on_exec_user_func;
	return true;
}

NYS_API bool nys_define_symbol(const char* name)
{
	if (!checkInLoadSetting("nys_define_symbol")) return false;
	if (!name || !*name) return setError("nys_define_symbol: name is empty");
	// Mirror the .mayu compiler's `define`: insert into the symbol set so that
	// later nys_has_symbol() sees it and flushQueue() emits a DefSymbol for it.
	g_symbols.insert(wstringi(from_UTF8(name)));
	return true;
}

NYS_API bool nys_has_symbol(const char* name)
{
	if (!checkInLoadSetting("nys_has_symbol")) return false;
	if (!name || !*name) return false;
	return g_symbols.count(wstringi(from_UTF8(name))) > 0;
}

NYS_API bool nys_reset_setting(void)
{
	if (!checkInLoadSetting("nys_reset_setting")) return false;
	resetQueue();
	return true;
}

NYS_API bool nys_load_mayu(void)
{
	if (!checkInLoadSetting("nys_load_mayu")) return false;

	ConfigFiles cf;
	wstringi path;
	if (!cf.getFilename(L"", &path, nullptr))
		return setError("nys_load_mayu: could not find config file");

	g_cmdQueue.push_back(QueueEntry::makeInclude(std::wstring(path)));
	return true;
}

NYS_API bool nys_include_mayu(const char* path)
{
	if (!checkInLoadSetting("nys_include_mayu")) return false;
	if (!path || !*path) return setError("nys_include_mayu: path is empty");

	ConfigFiles cf;
	wstringi resolved;
	if (!cf.getFilename(from_UTF8(path), &resolved, nullptr))
		return setError(std::string("nys_include_mayu: file not found: ") + path);

	g_cmdQueue.push_back(QueueEntry::makeInclude(std::wstring(resolved)));
	return true;
}


//=============================================================================
// Scan-code query API  (valid only from within on_load_setting)
//=============================================================================

// Parse a raw registry "Scancode Map" blob (see header for layout).
NYS_API bool parseScancodeMapBlob(const unsigned char* data, size_t len,
	std::vector<std::pair<uint16_t, uint16_t>>& out)
{
	out.clear();
	if (!data || len < 12) return false;

	auto readDword = [data](size_t off) -> uint32_t {
		return  static_cast<uint32_t>(data[off])
		     | (static_cast<uint32_t>(data[off + 1]) << 8)
		     | (static_cast<uint32_t>(data[off + 2]) << 16)
		     | (static_cast<uint32_t>(data[off + 3]) << 24);
	};

	uint32_t count = readDword(8);       // number of entries incl. null terminator
	if (count < 1) return false;          // must hold at least the terminator
	// header (12) + count DWORDs must fit in the blob.
	if (len < static_cast<size_t>(12) + static_cast<size_t>(count) * 4)
		return false;

	size_t mappings = count - 1;          // last entry is the null terminator
	out.reserve(mappings);
	for (size_t i = 0; i < mappings; ++i) {
		uint32_t e = readDword(12 + i * 4);
		uint16_t from = static_cast<uint16_t>(e >> 16);   // HIWORD = original
		uint16_t to   = static_cast<uint16_t>(e & 0xFFFF); // LOWORD = remapped
		out.emplace_back(from, to);
	}
	return true;
}

#ifdef NYAMY_TEST_HOOKS
// Test override: NYAMY_SCANCODE_MAP holds the raw registry blob as a hex
// string, letting a test pin the map instead of depending on whatever the
// machine happens to have configured.  Set but empty means "no Scancode Map".
// Returns false when the variable is absent, so the registry is read instead.
//
// Compiled only when NYAMY_TEST_HOOKS is defined (the Debug configuration of
// nyamy-scripter-dll, which is what the tests link).  A shipped build reads
// the registry and nothing else, so no environment variable can redirect it.
static bool loadScancodeMapFromEnv()
{
	std::wstring value;
	{
		const DWORD kStackChars = 512;
		SetLastError(ERROR_SUCCESS);
		wchar_t stack[kStackChars];
		DWORD n = GetEnvironmentVariableW(L"NYAMY_SCANCODE_MAP",
			stack, kStackChars);
		if (n == 0 && GetLastError() == ERROR_ENVVAR_NOT_FOUND)
			return false;
		if (n >= kStackChars) {               // longer than the stack buffer
			std::vector<wchar_t> heap(n);
			n = GetEnvironmentVariableW(L"NYAMY_SCANCODE_MAP",
				heap.data(), n);
			value.assign(heap.data(), n);
		} else {
			value.assign(stack, n);
		}
	}

	// Hex digits only; anything else (spaces, commas) is a separator.
	std::vector<unsigned char> bytes;
	int hi = -1;
	for (wchar_t c : value) {
		int d;
		if (c >= L'0' && c <= L'9')      d = c - L'0';
		else if (c >= L'a' && c <= L'f') d = c - L'a' + 10;
		else if (c >= L'A' && c <= L'F') d = c - L'A' + 10;
		else continue;
		if (hi < 0) {
			hi = d;
		} else {
			bytes.push_back(static_cast<unsigned char>((hi << 4) | d));
			hi = -1;
		}
	}
	if (!bytes.empty())
		parseScancodeMapBlob(bytes.data(), bytes.size(), g_scancodeMap);
	return true;
}
#endif // NYAMY_TEST_HOOKS

// Read (and cache) the registry Scancode Map on first access.
static void loadScancodeMapOnce()
{
	if (g_scancodeMapLoaded) return;
	g_scancodeMapLoaded = true;
	g_scancodeMap.clear();

#ifdef NYAMY_TEST_HOOKS
	if (loadScancodeMapFromEnv()) return;
#endif

	DWORD size = 0;
	LSTATUS st = RegGetValueW(HKEY_LOCAL_MACHINE,
		L"SYSTEM\\CurrentControlSet\\Control\\Keyboard Layout",
		L"Scancode Map", RRF_RT_REG_BINARY, nullptr, nullptr, &size);
	if (st != ERROR_SUCCESS || size == 0) return;

	std::vector<unsigned char> buf(size);
	st = RegGetValueW(HKEY_LOCAL_MACHINE,
		L"SYSTEM\\CurrentControlSet\\Control\\Keyboard Layout",
		L"Scancode Map", RRF_RT_REG_BINARY, nullptr, buf.data(), &size);
	if (st != ERROR_SUCCESS) return;

	parseScancodeMapBlob(buf.data(), buf.size(), g_scancodeMap);
}

// Keys whose presence in the registry Scancode Map suppresses the matching
// remapping in the configuration files.  Key names cannot be used here: the
// name table is filled by nys_def_key, which runs long after the symbol set
// has to be final, so these are the fixed set-1 scan codes.  0xE01D is
// RightControl and is deliberately not part of SCM-REMAP-LCTRL.
static const struct {
	const wchar_t* symbol;
	uint16_t       scan;
} kScancodeMapSymbols[] = {
	{ L"SCM-REMAP-ESC",   0x01 },
	{ L"SCM-REMAP-LCTRL", 0x1D },
};

// Define SCM-REMAP-* for every key the registry Scancode Map touches, as
// either the original or the remapped code.  A configuration file that sees
// one of these defined leaves that key alone instead of remapping it twice.
static void defineScancodeMapSymbols()
{
	loadScancodeMapOnce();
	for (const auto& e : kScancodeMapSymbols) {
		for (const auto& m : g_scancodeMap) {
			if (m.first == e.scan || m.second == e.scan) {
				g_symbols.insert(wstringi(e.symbol));
				break;
			}
		}
	}
}

NYS_API int nys_sc_resolve(const char* str)
{
	if (!str || !*str) return -1;
	uint16_t word = 0;
	if (!resolveScanWord(str, &word)) return -1;
	return word;
}

// Return true if word (as returned by nys_sc_resolve) is registered by the
// most recent "def option nls-keys".
NYS_API bool nys_is_nls_key_word(int word)
{
	if (word < 0) return false;
	return g_nlsKeys.count(static_cast<uint16_t>(word)) != 0;
}

NYS_API int nys_scancode_map_length(void)
{
	loadScancodeMapOnce();
	return static_cast<int>(g_scancodeMap.size());
}

NYS_API bool nys_scancode_map_entry(int idx, unsigned* from_word, unsigned* to_word)
{
	loadScancodeMapOnce();
	if (idx < 0 || idx >= static_cast<int>(g_scancodeMap.size()))
		return false;
	if (from_word) *from_word = g_scancodeMap[idx].first;
	if (to_word)   *to_word   = g_scancodeMap[idx].second;
	return true;
}


//=============================================================================
// Path resolution API  (valid from on_load_setting and on_exec_user_func)
//=============================================================================

NYS_API bool nys_resolve_config_path(const char*  name,
                                    const char** out_path)
{
	if (!checkInCallback("nys_resolve_config_path")) return false;
	ConfigFiles cf;
	wstringi resolved;

	if (name && *name) {
		// named file: absolute as is, relative through the search path
		if (!cf.getFilename(from_UTF8(name), &resolved))
			return setError(std::string("nys_resolve_config_path: not found: ") + name);
	} else if (!g_configPath.empty()) {
		// default: use path received from Engine, resolved the same way
		if (!cf.getFilename(g_configPath, &resolved))
			return setError("nys_resolve_config_path: path from Engine is not readable: "
			                + to_UTF8(std::wstring(g_configPath)));
	} else {
		// default: search the config search path for .mayu
		if (!cf.getFilename(L"", &resolved))
			return setError("nys_resolve_config_path: .mayu not found in the config search path");
	}

	NYsStrs* buf = g_sessionAlloc->newStrs();
	buf->strs.push_back(to_UTF8(std::wstring(resolved)));
	if (out_path) *out_path = buf->strs[0].c_str();
	return true;
}


//=============================================================================
// Runtime API
//=============================================================================

NYS_API bool nys_exec_keyseq(const char* actions)
{
	if (g_callbackState == CallbackState::LoadSetting)
		return setError("nys_exec_keyseq: must not be called from on_load_setting");
	if (g_callbackState != CallbackState::ExecUserFunc)
		return setError("nys_exec_keyseq: must be called from on_exec_user_func");
	if (!actions || !*actions)
		return setError("nys_exec_keyseq: actions is empty");
	if (!g_dataWriter)
		return setError("nys_exec_keyseq: not in an active session");

	// Reject actions containing &ExecUserFunc or @FuncName to prevent infinite recursion.
	{
		std::string s(actions);
		if (s.find("&ExecUserFunc") != std::string::npos ||
		    s.find('@') != std::string::npos)
			return setError("nys_exec_keyseq: actions must not contain &ExecUserFunc or @FuncName");
	}

	// Parse the action string directly (no synthetic wrapper needed).
	std::wstring wactions = from_UTF8(actions);
	MayuParser parser;
	auto seq = parser.parseActions(wactions.c_str(), wactions.size());
	if (parser.hasErrors())
		return setError("nys_exec_keyseq: failed to parse actions");

	// Compile to CmdActions without stream I/O.
	// A throwaway stream is passed to satisfy the MayuCompiler constructor;
	// it is only written to if the action string contains inline key sequence
	// literals as function arguments, which is unsupported in this context.
	std::ostringstream nullSink;
	CmdStreamWriter nullWriter(nullSink);
	ConfigFiles cf;
	MayuCompiler compiler(nullWriter, g_symbols, cf, nullptr, nullptr);
	std::vector<CmdAction> cmdActions =
		compiler.compileActions(*seq, ModifierContext::KeySeq);
	if (compiler.hasErrors())
		return setError("nys_exec_keyseq: failed to compile actions");

	// g_currentTrigger is always non-null while g_callbackState == ExecUserFunc.
	g_dataWriter->writeExecKeySeq(cmdActions, *g_currentTrigger);
	g_dataStream->flush();
	return true;
}


//=============================================================================
// Error reporting
//=============================================================================

NYS_API const char* nys_last_error(void)
{
	return g_lastError.empty() ? nullptr : g_lastError.c_str();
}


//=============================================================================
// Logging
//=============================================================================

NYS_API void nys_log(NYsLogLevel level, const char* msg)
{
	nysLogUtf8(logLevelFromByte(static_cast<uint8_t>(level)), msg);
}


NYS_API NYsLogLevel nys_log_level(void)
{
	return static_cast<NYsLogLevel>(nysEffectiveLogLevel());
}


NYS_API void nys_set_log_level(NYsLogLevel level)
{
	nysSetLogLevelFromScript(logLevelFromByte(static_cast<uint8_t>(level)));
}


NYS_API bool nys_would_log(NYsLogLevel level)
{
	return nysWouldLog(logLevelFromByte(static_cast<uint8_t>(level)));
}
