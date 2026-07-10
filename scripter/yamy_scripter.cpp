//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// yamy_scripter.cpp - yamy-scripter DLL implementation
//
// Implements the C API centred on ys_start().
//
// Reads CtrlStream commands from an inherited pipe handle (YS_CTRL env var).
// Writes compiled setting commands to a CmdStream pipe handle (YS_CMD env var).
// stdout / stderr are binary UTF-8 log channels (one message per line).
//
// Design -- command queueing:
//   ys_* API calls made during on_load_setting push commands into a typed
//   command queue (g_cmdQueue).  When on_load_setting returns true the queue
//   is flushed to the Engine pipe followed by CmdCommit.
//   If on_load_setting returns false the queue is discarded.
//
//   Queue layout:
//     1. g_keyseqEntries -- one entry per ys_reg_keyseq call, stored as
//        (name, compiled CmdActions).  Actions are parsed and compiled at
//        registration time, not at flush time, to amortise the parse cost.
//        Anonymous keyseqs get generated names "__ks{n}__".
//        g_keyseqEntries and g_keyseqByName survive a successful CmdCommit
//        so that ys_get_keyseq_idx() remains valid during on_exec_user_func.
//
//     2. g_cmdQueue -- sequential list of QueueEntry items, each of which is
//        either a direct CmdArgs struct (written verbatim to the stream) or
//        an Include directive (compiled at flush time with initialKeySeqIdx
//        set to the pre-registered keyseq count).
//
//   Modifier-prefix bit resolution and scan-code parsing are done at API
//   call time using the public static helpers MayuCompiler::compileModifierSpecs
//   and MayuParser::parseModifiedKey / parseScanCode.


#define _YAMY_SCRIPTER_IMPL
#include "yamy_scripter.h"  // must come first so YS_API = dllexport before ys_types.h re-includes it

#include "misc.h"
#include "ys_types.h"

#include "ctrl_stream_reader.h"
#include "cmd_stream_writer.h"
#include "config_files.h"
#include "mayu_parser.h"
#include "mayu_compiler.h"

#include "pipe_streambuf.h"

#include <fcntl.h>
#include <io.h>
#include <sstream>
#include <unordered_map>
#include <vector>
#include <string>


//=============================================================================
// Logging helpers
//=============================================================================

static void logLine(const std::wstring& msg)
{
	std::string utf8 = to_UTF8(msg);
	utf8 += '\n';
	fwrite(utf8.c_str(), 1, utf8.size(), stderr);
}

class Utf8LineWStreambuf : public std::wstreambuf
{
public:
	~Utf8LineWStreambuf() { flush(); }
	void flush() {
		if (!m_buf.empty()) { logLine(m_buf); m_buf.clear(); }
	}
protected:
	int_type overflow(int_type c) override {
		if (c == traits_type::eof()) { flush(); return traits_type::eof(); }
		wchar_t wc = static_cast<wchar_t>(c);
		if (wc == L'\n') { logLine(m_buf); m_buf.clear(); }
		else              m_buf += wc;
		return c;
	}
	std::streamsize xsputn(const wchar_t* s, std::streamsize n) override {
		for (std::streamsize i = 0; i < n; ++i) overflow(s[i]);
		return n;
	}
private:
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
// name -> virtual index (persists across CmdCommit for ys_get_keyseq_idx)
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

// Current trigger context (used by ys_exec_keyseq)
static const TriggerInfo* g_currentTrigger = nullptr;

// Active CmdStreamWriter / ostream (valid while ys_start is running)
static CmdStreamWriter* g_dataWriter = nullptr;
static std::ostream*    g_dataStream = nullptr;

// Symbol set received from the most recent Start command
static Symbols   g_symbols;
static wstringi  g_configName;
static wstringi  g_configPath;

// User-function registry: funcName -> handler
static std::unordered_map<std::string, ys_on_exec_user_func> g_userFuncs;

// Last error string (UTF-8)
static std::string g_lastError;

// RAII allocator for YsFuncArgs / YsStrs created during a callback session.
// A stack instance is live during on_load_setting and on_exec_user_func;
// g_sessionAlloc points to it so ys_*_new can register allocations.
struct SessionAllocator {
	std::vector<std::unique_ptr<YsFuncArgs>> funcArgs;
	std::vector<std::unique_ptr<YsStrs>>     strs;

	YsFuncArgs* newFuncArgs() {
		auto p = std::make_unique<YsFuncArgs>();
		auto* r = p.get();
		funcArgs.push_back(std::move(p));
		return r;
	}
	YsStrs* newStrs() {
		auto p = std::make_unique<YsStrs>();
		auto* r = p.get();
		strs.push_back(std::move(p));
		return r;
	}
};
static SessionAllocator* g_sessionAlloc = nullptr;

} // namespace


//=============================================================================
// Internal utilities
//=============================================================================

static bool setError(const std::string& msg)
{
	g_lastError = msg;
	logLine(L"[ys] error: " + from_UTF8(msg));
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
}

// Release queue buffers after a successful CmdCommit.
// g_keyseqEntries.name and g_keyseqByName persist so that
// ys_get_keyseq_idx() remains valid during on_exec_user_func calls.
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
	out.modifier = MayuCompiler::compileModifierSpecs(mods);
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

// True when the action list contains a function call or keyseq reference
// (directly or inside a sub-sequence).  Such actions bind to keymaps or
// other keyseqs when the consumer materializes them, so they must be
// re-emitted after all keymaps have been defined.
static bool hasLateBinding(const std::vector<CmdAction>& actions)
{
	for (const auto& a : actions) {
		if (a.type == CmdAction::FuncCall || a.type == CmdAction::KeySeqRef)
			return true;
		if (a.type == CmdAction::SubSeq && hasLateBinding(a.subActions))
			return true;
	}
	return false;
}

// Flush all queued commands to the data stream and write CmdCommit.
// Returns false on error (stream not flushed).
static bool flushQueue()
{
	if (!g_dataWriter) return false;

	Utf8LineWStreambuf logBuf;
	std::wostream logStream(&logBuf);
	ConfigFiles cf;

	// Step 1: DefSymbol commands for the current symbol set.
	for (const auto& sym : g_symbols) {
		CmdArgsDefSymbol d;
		d.symbolName = sym;
		g_dataWriter->writeDefSymbol(d);
	}

	// Step 2: Key definition commands (def key / mod / sync / alias) from the
	// queue, in order.  These must precede RegKeySeq because the consumer
	// resolves key names when it materializes a key sequence, and drops
	// actions whose key is not defined yet.
	auto isKeyDef = [](const CmdArgs& c) {
		return std::holds_alternative<CmdArgsDefKey>(c)
		    || std::holds_alternative<CmdArgsDefMod>(c)
		    || std::holds_alternative<CmdArgsDefSync>(c)
		    || std::holds_alternative<CmdArgsDefAlias>(c);
	};
	for (const auto& entry : g_cmdQueue) {
		if (entry.kind != QueueEntry::Kind::Direct || !isKeyDef(entry.cmd))
			continue;
		if (auto* k = std::get_if<CmdArgsDefKey>(&entry.cmd))
			g_dataWriter->writeDefKey(*k);
		else if (auto* m = std::get_if<CmdArgsDefMod>(&entry.cmd))
			g_dataWriter->writeDefMod(*m);
		else if (auto* s = std::get_if<CmdArgsDefSync>(&entry.cmd))
			g_dataWriter->writeDefSync(*s);
		else if (auto* al = std::get_if<CmdArgsDefAlias>(&entry.cmd))
			g_dataWriter->writeDefAlias(*al);
	}

	// Step 3: Pre-registered keyseqs (indices 0 .. n-1).
	for (const auto& e : g_keyseqEntries) {
		CmdArgsRegKeySeq ks;
		ks.name    = wstringi(e.name);
		ks.mode    = 0;
		ks.actions = e.compiled;
		g_dataWriter->writeRegKeySeq(ks);
	}

	// Step 4: Remaining commands in queue order (key definitions were already
	// written in step 2).
	// Include entries are compiled with initialKeySeqIdx = current keyseq count.
	uint32_t nextKeySeqIdx = static_cast<uint32_t>(g_keyseqEntries.size());

	for (const auto& entry : g_cmdQueue) {
		if (entry.kind == QueueEntry::Kind::Direct) {
			if (isKeyDef(entry.cmd))
				continue;
			std::visit(overloaded{
				[](const CmdArgsRegKeySeq&)   {},  // not placed in cmdQueue
				[](const CmdArgsExecKeySeq&)  {},  // not placed in cmdQueue
				[](const CmdArgsCommit&)      {},  // not placed in cmdQueue
				[](const CmdArgsDefKey&)      {},  // written in step 2
				[](const CmdArgsDefMod&)      {},  // written in step 2
				[](const CmdArgsDefSync&)     {},  // written in step 2
				[](const CmdArgsDefAlias&)    {},  // written in step 2
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
					logLine(L"[ys] error: compile failed.");
					return false;
				}
				nextKeySeqIdx = compiler.nextKeySeqIdx();
			}
		}
	}

	// Step 5: Re-emit pre-registered keyseqs that reference keymaps or other
	// keyseqs.  Their first emission in step 3 preceded every BeginKeymap, so
	// the consumer materialized those references as unresolved.  Re-emitting
	// after step 4 lets the consumer resolve them; it replaces the contents
	// of a named keyseq in place, keeping pointers held by earlier
	// assignments valid.
	for (const auto& e : g_keyseqEntries) {
		if (!hasLateBinding(e.compiled))
			continue;
		CmdArgsRegKeySeq ks;
		ks.name    = wstringi(e.name);
		ks.mode    = 0;
		ks.actions = e.compiled;
		g_dataWriter->writeRegKeySeq(ks);
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



YS_API YsFuncArgs* ys_func_args_new(void) {
	if (!g_sessionAlloc) {
		setError("ys_func_args_new: must be called from a scripter callback");
		return nullptr;
	}
	return g_sessionAlloc->newFuncArgs();
}
YS_API YsStrs* ys_strs_new(void) {
	if (!g_sessionAlloc) {
		setError("ys_strs_new: must be called from a scripter callback");
		return nullptr;
	}
	return g_sessionAlloc->newStrs();
}


YS_API int ys_func_args_length(const YsFuncArgs* fas)
{
	if (!fas) return 0;
	return static_cast<int>(fas->entries.size());
}

YS_API int ys_strs_length(const YsStrs* ss)
{
	if (!ss) return 0;
	return static_cast<int>(ss->strs.size());
}

YS_API YsType ys_func_args_get(const YsFuncArgs* fas, int idx,
	int64_t* p_value, int64_t* p_length)
{
	if (!fas || idx < 0 || idx >= static_cast<int>(fas->entries.size()))
		return YsType_Error;
	const YsFuncArg& e = fas->entries[idx];
	switch (e.type) {
	case YsType_String:
	case YsType_Regexp:
		*p_value  = reinterpret_cast<int64_t>(e.str.c_str());
		*p_length = static_cast<int64_t>(e.str.size());
		return e.type;
	case YsType_Number:
	case YsType_KeySeqIdx:
		*p_value  = e.numval;
		*p_length = 0;
		return e.type;
	case YsType_ModifierSpec:
		*p_value  = e.numval;
		*p_length = e.numval2;
		return e.type;
	case YsType_TokenSeq:
		*p_value  = reinterpret_cast<int64_t>(e.strs);
		*p_length = e.strs ? static_cast<int64_t>(e.strs->strs.size()) : 0;
		return e.type;
	default:
		return YsType_Error;
	}
}

YS_API bool ys_strs_get(const YsStrs* ss, int idx,
	const char** p_value, size_t* p_length)
{
	if (!ss || idx < 0 || idx >= static_cast<int>(ss->strs.size())) return false;
	*p_value  = ss->strs[idx].c_str();
	*p_length = ss->strs[idx].size();
	return true;
}

YS_API bool ys_func_args_push(YsFuncArgs* fas, YsType type,
	int64_t value, int64_t length)
{
	if (!fas) return false;

	YsFuncArg e;
	e.type = type;
	switch (type) {
	case YsType_String:
	case YsType_Regexp: {
		const char* p = reinterpret_cast<const char*>(static_cast<uintptr_t>(value));
		e.str.assign(p, static_cast<size_t>(length));
		break;
	}
	case YsType_Number:
		e.numval = static_cast<int32_t>(value);
		break;
	case YsType_KeySeqIdx:
		e.numval = static_cast<uint32_t>(value);
		break;
	case YsType_ModifierSpec:
		e.numval  = value;
		e.numval2 = length;
		break;
	case YsType_TokenSeq: {
		const YsStrs* ss = reinterpret_cast<const YsStrs*>(static_cast<uintptr_t>(value));
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

YS_API bool ys_strs_push(YsStrs* ss, const char* value, size_t length)
{
	if (!ss || !value) return false;
	ss->strs.emplace_back(value, length);
	return true;
}




//=============================================================================
// ys_start -- main event loop
//=============================================================================

YS_API int ys_start(const YsCallbacks* callbacks, void* exeCtx)
{
	if (!callbacks || !callbacks->on_load_setting) return 1;

	// Retrieve pipe handles from environment variables set by the parent process.
	HANDLE hCtrlRead  = INVALID_HANDLE_VALUE;
	HANDLE hDataWrite = INVALID_HANDLE_VALUE;
	{
		wchar_t buf[32];
		if (GetEnvironmentVariableW(L"YS_CTRL", buf, 32) > 0)
			hCtrlRead  = reinterpret_cast<HANDLE>(
				static_cast<uintptr_t>(wcstoull(buf, nullptr, 10)));
		if (GetEnvironmentVariableW(L"YS_CMD",  buf, 32) > 0)
			hDataWrite = reinterpret_cast<HANDLE>(
				static_cast<uintptr_t>(wcstoull(buf, nullptr, 10)));
	}

	_setmode(_fileno(stdout), _O_BINARY);
	_setmode(_fileno(stderr), _O_BINARY);

	if (hCtrlRead == INVALID_HANDLE_VALUE || hDataWrite == INVALID_HANDLE_VALUE) {
		logLine(L"error: YS_CTRL and YS_CMD environment variables are required");
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

	for (;;) {
		CtrlId id;
		if (!ctrlReader.readNext(id))
			break;

		if (id == CtrlId::Quit) {
			if (callbacks->on_quit) callbacks->on_quit(exeCtx);
			break;
		} else if (id == CtrlId::Start) {
			auto startArgs = ctrlReader.readStart();
			g_configName = std::move(startArgs.configName);
			g_configPath = std::move(startArgs.configPath);
			g_symbols    = std::move(startArgs.symbols);

			resetQueue();
			g_callbackState = CallbackState::LoadSetting;
			g_lastError.clear();
			{
				SessionAllocator sa;
				g_sessionAlloc = &sa;
				bool ok = callbacks->on_load_setting(exeCtx);
				g_sessionAlloc = nullptr;
				// sa goes out of scope here, freeing all YsFuncArgs/YsStrs
				g_callbackState = CallbackState::None;
				if (ok) {
					if (!flushQueue())
						logLine(L"[ys] error: flushQueue failed; setting not applied.");
					else
						clearQueueBuffers();
				} else {
					resetQueue();
					logLine(L"[ys] on_load_setting returned false; setting discarded.");
				}
			}
		} else if (id == CtrlId::ExecUserFunc) {
			SessionAllocator sa;
			g_sessionAlloc = &sa;
			CtrlArgsExecUserFunc req = ctrlReader.readExecUserFunc();
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
			// sa goes out of scope here, freeing all YsFuncArgs/YsStrs
			// including YsFuncArg::strs entries inside req.args
		}
	}

	g_dataWriter = nullptr;
	g_dataStream = nullptr;

	CloseHandle(hCtrlRead);
	CloseHandle(hDataWrite);
	return 0;
}

YS_API uint32_t ys_version(void)
{
	return (0u << 16) | (1u << 8) | 0u;
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


YS_API int ys_reg_keyseq(const char* name, const char* actions)
{
	if (!checkInLoadSetting("ys_reg_keyseq")) return -1;
	if (!actions || !*actions) {
		setError("ys_reg_keyseq: actions must not be empty");
		return -1;
	}

	// Parse and compile the action string at registration time.
	std::wstring wactions = from_UTF8(actions);
	MayuParser parser;
	auto seq = parser.parseActions(wactions.c_str(), wactions.size());
	if (parser.hasErrors() || !seq) {
		setError("ys_reg_keyseq: failed to parse actions");
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
	std::vector<CmdAction> compiled = compiler.compileActions(*seq);
	compiler.setSubSeqCollector(nullptr);
	if (compiler.hasErrors()) {
		setError("ys_reg_keyseq: failed to compile actions");
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

YS_API int ys_get_keyseq_idx(const char* name)
{
	if (!name || !*name) return -1;
	auto it = g_keyseqByName.find(name);
	return (it != g_keyseqByName.end()) ? it->second : -1;
}

YS_API bool ys_def_key(const YsStrs* names, const YsStrs* scancodes)
{
	if (!checkInLoadSetting("ys_def_key")) return false;
	if (!names     || ys_strs_length(names)     == 0) return setError("ys_def_key: names is empty");
	if (!scancodes || ys_strs_length(scancodes) == 0) return setError("ys_def_key: scancodes is empty");

	CmdArgsDefKey d;
	int nn = ys_strs_length(names);
	for (int i = 0; i < nn; ++i) {
		const char* p = nullptr; size_t len = 0;
		ys_strs_get(names, i, &p, &len);
		d.names.push_back(wstringi(from_UTF8(std::string(p, len))));
	}
	int ns = ys_strs_length(scancodes);
	for (int i = 0; i < ns; ++i) {
		const char* p = nullptr; size_t len = 0;
		ys_strs_get(scancodes, i, &p, &len);
		CmdScanCode sc;
		if (!parseScanCode("ys_def_key", p, len, sc)) return false;
		d.scanCodes.push_back(sc);
	}
	g_cmdQueue.push_back(QueueEntry::makeDirect(std::move(d)));
	return true;
}

YS_API bool ys_def_mod(const char* modifier_name, const YsStrs* key_names)
{
	if (!checkInLoadSetting("ys_def_mod")) return false;
	if (!modifier_name || !*modifier_name) return setError("ys_def_mod: modifier_name is empty");
	if (!key_names || ys_strs_length(key_names) == 0) return setError("ys_def_mod: key_names is empty");

	CmdArgsDefMod d;
	d.modifierName = from_UTF8(modifier_name);
	int n = ys_strs_length(key_names);
	for (int i = 0; i < n; ++i) {
		const char* p = nullptr; size_t len = 0;
		ys_strs_get(key_names, i, &p, &len);
		d.keyNames.push_back(wstringi(from_UTF8(std::string(p, len))));
	}
	g_cmdQueue.push_back(QueueEntry::makeDirect(std::move(d)));
	return true;
}

YS_API bool ys_def_sync(const YsStrs* scan_codes)
{
	if (!checkInLoadSetting("ys_def_sync")) return false;
	if (!scan_codes || ys_strs_length(scan_codes) == 0) return setError("ys_def_sync: scan_codes is empty");

	CmdArgsDefSync d;
	int n = ys_strs_length(scan_codes);
	for (int i = 0; i < n; ++i) {
		const char* p = nullptr; size_t len = 0;
		ys_strs_get(scan_codes, i, &p, &len);
		CmdScanCode sc;
		if (!parseScanCode("ys_def_sync", p, len, sc)) return false;
		d.scanCodes.push_back(sc);
	}
	g_cmdQueue.push_back(QueueEntry::makeDirect(std::move(d)));
	return true;
}

YS_API bool ys_def_alias(const char* alias_name, const char* key_name)
{
	if (!checkInLoadSetting("ys_def_alias")) return false;
	if (!alias_name || !*alias_name) return setError("ys_def_alias: alias_name is empty");
	if (!key_name   || !*key_name)   return setError("ys_def_alias: key_name is empty");

	CmdArgsDefAlias d;
	d.aliasName = from_UTF8(alias_name);
	d.keyName   = from_UTF8(key_name);
	g_cmdQueue.push_back(QueueEntry::makeDirect(std::move(d)));
	return true;
}

YS_API bool ys_def_subst(const YsStrs* lhs_mod_keys, int rhs_keyseq_idx)
{
	if (!checkInLoadSetting("ys_def_subst")) return false;
	if (!lhs_mod_keys || ys_strs_length(lhs_mod_keys) == 0) return setError("ys_def_subst: lhs_mod_keys is empty");

	CmdArgsDefSubst d;
	d.rhsKeySeqIdx = static_cast<uint32_t>(rhs_keyseq_idx);
	int n = ys_strs_length(lhs_mod_keys);
	for (int i = 0; i < n; ++i) {
		const char* p = nullptr; size_t len = 0;
		ys_strs_get(lhs_mod_keys, i, &p, &len);
		CmdModifiedKey mk;
		if (!parseModifiedKey("ys_def_subst", p, len, mk)) return false;
		d.lhsKeys.push_back(std::move(mk));
	}
	g_cmdQueue.push_back(QueueEntry::makeDirect(std::move(d)));
	return true;
}

YS_API bool ys_def_option(const char* option_name, const char* value)
{
	if (!checkInLoadSetting("ys_def_option")) return false;
	if (!option_name || !*option_name) return setError("ys_def_option: option_name is empty");
	if (!value)                         return setError("ys_def_option: value is null");

	CmdArgsDefOption d;
	d.optionName = from_UTF8(option_name);
	d.value      = from_UTF8(value);
	g_cmdQueue.push_back(QueueEntry::makeDirect(std::move(d)));
	return true;
}

YS_API bool ys_begin_keymap(const char* keyword, const char* name,
	const char* window_class, const char* window_title,
	const char* op, const char* parent_name,
	int default_keyseq_idx)
{
	if (!checkInLoadSetting("ys_begin_keymap")) return false;
	if (!keyword || !*keyword) return setError("ys_begin_keymap: keyword is empty");
	if (!name    || !*name)    return setError("ys_begin_keymap: name is empty");

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

YS_API bool ys_assign_key(const YsStrs* lhs_mod_keys, int rhs_keyseq_idx)
{
	if (!checkInLoadSetting("ys_assign_key")) return false;
	if (!lhs_mod_keys || ys_strs_length(lhs_mod_keys) == 0) return setError("ys_assign_key: lhs_mod_keys is empty");

	CmdArgsAssignKey d;
	d.rhsKeySeqIdx = static_cast<uint32_t>(rhs_keyseq_idx);
	int n = ys_strs_length(lhs_mod_keys);
	for (int i = 0; i < n; ++i) {
		const char* p = nullptr; size_t len = 0;
		ys_strs_get(lhs_mod_keys, i, &p, &len);
		CmdModifiedKey mk;
		if (!parseModifiedKey("ys_assign_key", p, len, mk)) return false;
		d.lhsKeys.push_back(std::move(mk));
	}
	g_cmdQueue.push_back(QueueEntry::makeDirect(std::move(d)));
	return true;
}

YS_API bool ys_assign_event(const char* event_name, int rhs_keyseq_idx)
{
	if (!checkInLoadSetting("ys_assign_event")) return false;
	if (!event_name || !*event_name) return setError("ys_assign_event: event_name is empty");

	CmdArgsAssignEvent d;
	d.eventName     = from_UTF8(event_name);
	d.rhsKeySeqIdx  = static_cast<uint32_t>(rhs_keyseq_idx);
	g_cmdQueue.push_back(QueueEntry::makeDirect(std::move(d)));
	return true;
}

YS_API bool ys_assign_mod(const YsStrs* prefixes, const char* modifier_name,
	const char* op, const YsStrs* keys)
{
	if (!checkInLoadSetting("ys_assign_mod")) return false;
	if (!modifier_name || !*modifier_name) return setError("ys_assign_mod: modifier_name is empty");
	if (!op || !*op)                        return setError("ys_assign_mod: op is empty");
	if (!keys || ys_strs_length(keys) == 0) return setError("ys_assign_mod: keys is empty");

	CmdArgsAssignMod d;
	d.mainModifierName = from_UTF8(modifier_name);
	d.op               = from_UTF8(op);

	if (prefixes) {
		int n = ys_strs_length(prefixes);
		for (int i = 0; i < n; ++i) {
			const char* p = nullptr; size_t len = 0;
			ys_strs_get(prefixes, i, &p, &len);
			auto [mode, mname] = splitAssignEntry(p, len);
			CmdArgsAssignMod::PrefixMod pm;
			pm.assignMode   = mode;
			pm.modifierName = mname;
			d.prefixes.push_back(pm);
		}
	}
	int nk = ys_strs_length(keys);
	for (int i = 0; i < nk; ++i) {
		const char* p = nullptr; size_t len = 0;
		ys_strs_get(keys, i, &p, &len);
		auto [mode, kname] = splitAssignEntry(p, len);
		CmdArgsAssignMod::KeyEntry ke;
		ke.assignMode = mode;
		ke.keyName    = kname;
		d.keys.push_back(ke);
	}
	g_cmdQueue.push_back(QueueEntry::makeDirect(std::move(d)));
	return true;
}

YS_API bool ys_reg_user_func(const char* func_name, ys_on_exec_user_func on_exec_user_func)
{
	if (!checkInLoadSetting("ys_reg_user_func")) return false;
	if (!func_name || !*func_name) return setError("ys_reg_user_func: func_name is empty");
	if (!on_exec_user_func) return setError("ys_reg_user_func: handler is null");

	g_userFuncs[func_name] = on_exec_user_func;
	return true;
}

YS_API bool ys_define_symbol(const char* name)
{
	if (!checkInLoadSetting("ys_define_symbol")) return false;
	if (!name || !*name) return setError("ys_define_symbol: name is empty");
	// Mirror the .mayu compiler's `define`: insert into the symbol set so that
	// later ys_has_symbol() sees it and flushQueue() emits a DefSymbol for it.
	g_symbols.insert(wstringi(from_UTF8(name)));
	return true;
}

YS_API bool ys_has_symbol(const char* name)
{
	if (!checkInLoadSetting("ys_has_symbol")) return false;
	if (!name || !*name) return false;
	return g_symbols.count(wstringi(from_UTF8(name))) > 0;
}

YS_API bool ys_reset_setting(void)
{
	if (!checkInLoadSetting("ys_reset_setting")) return false;
	resetQueue();
	return true;
}

YS_API bool ys_load_mayu(void)
{
	if (!checkInLoadSetting("ys_load_mayu")) return false;

	ConfigFiles cf;
	wstringi path;
	if (!cf.getFilename(L"", &path, nullptr))
		return setError("ys_load_mayu: could not find config file");

	g_cmdQueue.push_back(QueueEntry::makeInclude(std::wstring(path)));
	return true;
}

YS_API bool ys_include_mayu(const char* path)
{
	if (!checkInLoadSetting("ys_include_mayu")) return false;
	if (!path || !*path) return setError("ys_include_mayu: path is empty");

	ConfigFiles cf;
	wstringi resolved;
	if (!cf.getFilename(from_UTF8(path), &resolved, nullptr))
		return setError(std::string("ys_include_mayu: file not found: ") + path);

	g_cmdQueue.push_back(QueueEntry::makeInclude(std::wstring(resolved)));
	return true;
}


//=============================================================================
// Path resolution API  (valid from on_load_setting and on_exec_user_func)
//=============================================================================

YS_API YsStrs* ys_get_home_directories(void)
{
	if (!checkInCallback("ys_get_home_directories")) return nullptr;
	ConfigFiles cf;
	HomeDirectories dirs;
	cf.getHomeDirectories(&dirs);
	YsStrs* result = ys_strs_new();
	if (!result) return nullptr;
	for (const auto& d : dirs) {
		std::string u = to_UTF8(std::wstring(d));
		ys_strs_push(result, u.c_str(), u.size());
	}
	return result;
}


YS_API bool ys_resolve_config_path(const char*  name,
                                    const char** out_path)
{
	if (!checkInCallback("ys_resolve_config_path")) return false;
	ConfigFiles cf;
	wstringi resolved;

	if (name && *name) {
		// named file: search home directories (no registry access)
		if (!cf.getFilename(from_UTF8(name), &resolved))
			return setError(std::string("ys_resolve_config_path: not found: ") + name);
	} else if (!g_configPath.empty()) {
		// default: use path received from Engine
		resolved = g_configPath;
		if (!cf.isReadable(resolved))
			return setError("ys_resolve_config_path: path from Engine is not readable: "
			                + to_UTF8(std::wstring(resolved)));
	} else {
		// default: no registry config; search home directories for .mayu
		HomeDirectories dirs;
		cf.getHomeDirectories(&dirs);
		bool found = false;
		for (const auto &dir : dirs) {
			resolved = dir + L"\\.mayu";
			if (cf.isReadable(resolved, 0)) { found = true; break; }
		}
		if (!found)
			return setError("ys_resolve_config_path: .mayu not found in home directories");
	}

	YsStrs* buf = g_sessionAlloc->newStrs();
	buf->strs.push_back(to_UTF8(std::wstring(resolved)));
	if (out_path) *out_path = buf->strs[0].c_str();
	return true;
}


//=============================================================================
// Runtime API
//=============================================================================

YS_API bool ys_exec_keyseq(const char* actions)
{
	if (g_callbackState == CallbackState::LoadSetting)
		return setError("ys_exec_keyseq: must not be called from on_load_setting");
	if (g_callbackState != CallbackState::ExecUserFunc)
		return setError("ys_exec_keyseq: must be called from on_exec_user_func");
	if (!actions || !*actions)
		return setError("ys_exec_keyseq: actions is empty");
	if (!g_dataWriter)
		return setError("ys_exec_keyseq: not in an active session");

	// Reject actions containing &ExecUserFunc or @FuncName to prevent infinite recursion.
	{
		std::string s(actions);
		if (s.find("&ExecUserFunc") != std::string::npos ||
		    s.find('@') != std::string::npos)
			return setError("ys_exec_keyseq: actions must not contain &ExecUserFunc or @FuncName");
	}

	// Parse the action string directly (no synthetic wrapper needed).
	std::wstring wactions = from_UTF8(actions);
	MayuParser parser;
	auto seq = parser.parseActions(wactions.c_str(), wactions.size());
	if (parser.hasErrors())
		return setError("ys_exec_keyseq: failed to parse actions");

	// Compile to CmdActions without stream I/O.
	// A throwaway stream is passed to satisfy the MayuCompiler constructor;
	// it is only written to if the action string contains inline key sequence
	// literals as function arguments, which is unsupported in this context.
	std::ostringstream nullSink;
	CmdStreamWriter nullWriter(nullSink);
	ConfigFiles cf;
	MayuCompiler compiler(nullWriter, g_symbols, cf, nullptr, nullptr);
	std::vector<CmdAction> cmdActions = compiler.compileActions(*seq);
	if (compiler.hasErrors())
		return setError("ys_exec_keyseq: failed to compile actions");

	// g_currentTrigger is always non-null while g_callbackState == ExecUserFunc.
	g_dataWriter->writeExecKeySeq(cmdActions, *g_currentTrigger);
	g_dataStream->flush();
	return true;
}


//=============================================================================
// Error reporting
//=============================================================================

YS_API const char* ys_last_error(void)
{
	return g_lastError.empty() ? nullptr : g_lastError.c_str();
}
