//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// test_main.cpp
//
// Verifies that loading dot.mayu (native .mayu compile pipeline) and
// dot.mayu.rb (mruby DSL pipeline) produce identical Settings for several
// USE* symbol combinations.
//
// The .mayu reference path reuses the mruby runtime via a tiny generated
// loader script (__via_mayu__.rb -> `load "dot.mayu"`), so both paths run
// through the same nys_start() / flushQueue() / CmdProcessor plumbing and
// differ only in the script that is executed.

#include "misc.h"

#include "test_harness.h"
#include "setting_dump.h"
#include "setting.h"
#include "keymap.h"
#include "keyboard.h"
#include "symbols.h"
#include "nyamy_scripter.h"   // parseScancodeMapBlob (exported test helper)
#include "cli_options.h"

#include <windows.h>
#include <io.h>
#include <cstdio>
#include <cstdint>
#include <fstream>
#include <string>
#include <utility>
#include <vector>


namespace {

std::string wideToUtf8(const std::wstring &i_w)
{
	if (i_w.empty()) return std::string();
	int n = WideCharToMultiByte(CP_UTF8, 0, i_w.c_str(), (int)i_w.size(),
	                            nullptr, 0, nullptr, nullptr);
	std::string s(n, '\0');
	WideCharToMultiByte(CP_UTF8, 0, i_w.c_str(), (int)i_w.size(),
	                    &s[0], n, nullptr, nullptr);
	return s;
}

void writeUtf8File(const std::wstring &i_path, const std::wstring &i_text)
{
	std::ofstream f(i_path.c_str(), std::ios::binary);
	std::string u8 = wideToUtf8(i_text);
	f.write(u8.data(), (std::streamsize)u8.size());
}

// Report the 1-based line number and content of the first difference.
void reportFirstDiff(const std::wstring &a, const std::wstring &b)
{
	std::wstring la, lb;
	size_t ia = 0, ib = 0;
	int line = 1;
	while (ia < a.size() || ib < b.size()) {
		la.clear(); lb.clear();
		while (ia < a.size() && a[ia] != L'\n') la += a[ia++];
		if (ia < a.size()) ++ia;
		while (ib < b.size() && b[ib] != L'\n') lb += b[ib++];
		if (ib < b.size()) ++ib;
		if (la != lb) {
			printf("  first diff at line %d:\n", line);
			printf("    .mayu : %s\n", wideToUtf8(la).c_str());
			printf("    .rb   : %s\n", wideToUtf8(lb).c_str());
			return;
		}
		++line;
	}
}

struct Combo {
	const char *name;
	bool workaround;   // additionally load workaround.mayu / workaround.mayu.rb
	std::vector<const wchar_t *> symbols;
};


// The keymap of the given name, or null.
const Keymap *findKeymap(const Setting &i_setting, const wchar_t *i_name)
{
	for (const Keymap &km : i_setting.m_keymaps)
		if (std::wstring(km.getName().c_str()) == i_name)
			return &km;
	return nullptr;
}

// A ModifiedKey as the engine builds one for a physical key event: every
// modifier is cared for, U- / D- reflect the direction of the event.
// i_locks presses the lock and window-state modifiers; R- and M0- ... M9- stay
// released, as they are not lock state.
struct Held {
	bool ctrl = false, shift = false, alt = false, win = false, locks = false;
};

ModifiedKey physicalKey(Key *i_key, bool i_isPressed, Held i_held = Held())
{
	ModifiedKey mkey;
	mkey.m_key = i_key;
	Modifier &mod = mkey.m_modifier;
	mod.press(Modifier::Type_Shift, i_held.shift);
	mod.press(Modifier::Type_Alt, i_held.alt);
	mod.press(Modifier::Type_Control, i_held.ctrl);
	mod.press(Modifier::Type_Windows, i_held.win);
	mod.press(Modifier::Type_Up, !i_isPressed);
	mod.press(Modifier::Type_Down, i_isPressed);
	for (int t = Modifier::Type_Repeat; t < Modifier::Type_ASSIGN; ++t) {
		Modifier::Type type = static_cast<Modifier::Type>(t);
		bool isMod = (Modifier::Type_Mod0 <= type && type <= Modifier::Type_Mod9);
		mod.press(type, i_held.locks && !isMod
						&& type != Modifier::Type_Repeat);
	}
	return mkey;
}

// The modifier of the first action of the key sequence assigned to i_mkey,
// or null when there is no assignment or no action.
const Modifier *firstActionModifier(const Keymap &i_keymap,
									const ModifiedKey &i_mkey,
									Action::Type *o_type = nullptr)
{
	const Keymap::KeyAssignment *ka = i_keymap.searchAssignment(i_mkey);
	if (!ka) return nullptr;
	const KeySeq::Actions &actions = ka->m_keySeq->getActions();
	if (actions.empty()) return nullptr;
	const Action *action = actions[0].get();
	if (o_type) *o_type = action->getType();
	if (action->getType() == Action::Type_key)
		return &static_cast<const ActionKey *>(action)->m_modifiedKey.m_modifier;
	if (action->getType() == Action::Type_function)
		return &static_cast<const ActionFunction *>(action)->m_modifier;
	return nullptr;
}

} // namespace


int main()
{
	// Resolve our own directory; config / script files are copied next to us
	// by the build.
	wchar_t exePath[MAX_PATH] = {};
	GetModuleFileNameW(nullptr, exePath, MAX_PATH);
	std::wstring exeDir(exePath);
	size_t slash = exeDir.find_last_of(L"\\/");
	if (slash != std::wstring::npos) exeDir.resize(slash);

	// Pin the directory layout to scratch directories under it, so the tests
	// neither read nor write the real per-user tree.  NYAMY_ROOT is our own
	// directory because that is where the distributed .mayu / .rb files sit.
	// The scripter reads these back through nys_paths_*, which caches on first
	// use, so they have to be set before the first buildSetting().
	std::wstring homeDir = exeDir + L"\\__home__";
	std::wstring cfgDir  = homeDir + L"\\Config";
	std::wstring libDir  = homeDir + L"\\Lib";
	std::wstring cwdDir  = exeDir + L"\\__cwd__";
	CreateDirectoryW(homeDir.c_str(), nullptr);
	CreateDirectoryW(cfgDir.c_str(), nullptr);
	CreateDirectoryW(libDir.c_str(), nullptr);
	CreateDirectoryW(cwdDir.c_str(), nullptr);
	SetEnvironmentVariableW(L"NYAMY_ROOT",   exeDir.c_str());
	SetEnvironmentVariableW(L"NYAMY_HOME",   homeDir.c_str());
	SetEnvironmentVariableW(L"NYAMY_CONFIG", cfgDir.c_str());

	// Move the cwd to a directory holding nothing but decoys of the file names
	// the tests load: no path resolution may go through the current directory.
	writeUtf8File(cwdDir + L"\\.mayu.rb",
		L"raise \"resolved through the current directory\"\n");
	writeUtf8File(cwdDir + L"\\__lp_load_lib__.rb",
		L"raise \"resolved through the current directory\"\n");
	SetCurrentDirectoryW(cwdDir.c_str());

	std::string exeDirU8 = wideToUtf8(exeDir);
	std::string rbScript    = exeDirU8 + "\\dot.mayu.rb";
	std::string viaMayuPath = exeDirU8 + "\\__via_mayu__.rb";
	std::string wkRbPath    = exeDirU8 + "\\__wk_via_rb__.rb";
	std::string wkMayuPath  = exeDirU8 + "\\__wk_via_mayu__.rb";

	// Generate the loader scripts.  workaround.* is not included from dot.*,
	// so chain it behind the dot configuration.  The .mayu reference goes
	// through a wrapper .mayu, which is the spelling the .rb side mirrors:
	// one `load` of a file that includes both.
	writeUtf8File(exeDir + L"\\__via_mayu__.rb", L"load \"dot.mayu\"\n");
	writeUtf8File(exeDir + L"\\__wk_all__.mayu",
	              L"include \"dot.mayu\"\ninclude \"workaround.mayu\"\n");
	writeUtf8File(exeDir + L"\\__wk_via_mayu__.rb",
	              L"load \"__wk_all__.mayu\"\n");
	writeUtf8File(exeDir + L"\\__wk_via_rb__.rb",
	              L"load \"dot.mayu.rb\"\nload \"workaround.mayu.rb\"\n");

	const Combo combos[] = {
		{ "USE104",                                   false,
		  { L"USE104" } },
		{ "USE104 + USE109on104",                     false,
		  { L"USE104", L"USE109on104" } },
		{ "USE109",                                   false,
		  { L"USE109" } },
		{ "USE109 + USE104on109",                     false,
		  { L"USE109", L"USE104on109" } },
		{ "USE109 + USE104on109 + USEdefault",        false,
		  { L"USE109", L"USE104on109", L"USEdefault" } },
		{ "USE104 + workaround",                      true,
		  { L"USE104" } },
		{ "USE109 + USEdefault + MAP-ESCAPE-TO-META + workaround", true,
		  { L"USE109", L"USEdefault", L"MAP-ESCAPE-TO-META" } },
		{ "USE109 + USE104on109 + USEdefault + workaround", true,
		  { L"USE109", L"USE104on109", L"USEdefault" } },
		// The remaining branch symbols.  Each one guards a block that has to be
		// spelled out twice, once per configuration language, so each needs a
		// combination that actually enters it.
		{ "USE109 + USEdefault + ZXCV",               false,
		  { L"USE109", L"USEdefault", L"ZXCV" } },
		{ "USE109 + USEdefault + EmacsMove/ShiftSelection", false,
		  { L"USE109", L"USEdefault", L"EmacsMove/ShiftSelection" } },
		{ "USE109 + USE104on109 + SunType4",          false,
		  { L"USE109", L"USE104on109", L"SunType4" } },
		{ "USE104 + USE109on104 + USEdefault + MAP-ESCAPE-TO-META", false,
		  { L"USE104", L"USE109on104", L"USEdefault", L"MAP-ESCAPE-TO-META" } },
	};

	int failures = 0;
	int idx = 0;
	for (const Combo &combo : combos) {
		++idx;
		Symbols syms;
		for (const wchar_t *s : combo.symbols) syms.insert(wstringi(s));

		printf("[%d] %s ... ", idx, combo.name);
		fflush(stdout);

		std::shared_ptr<Setting> a = buildSetting(
			combo.workaround ? wkMayuPath : viaMayuPath, syms);  // .mayu
		std::shared_ptr<Setting> b = buildSetting(
			combo.workaround ? wkRbPath : rbScript, syms);       // .mayu.rb

		if (!a || !b) {
			printf("FAIL (load failed: %s%s)\n",
			       a ? "" : ".mayu ", b ? "" : ".mayu.rb");
			++failures;
			continue;
		}

		std::wstring da = dumpSetting(*a);
		std::wstring db = dumpSetting(*b);

		if (da == db) {
			printf("OK\n");
		} else {
			printf("FAIL (settings differ)\n");
			reportFirstDiff(da, db);
			wchar_t suffix[16];
			_itow_s(idx, suffix, 16, 10);
			writeUtf8File(exeDir + L"\\fail_" + suffix + L"_mayu.txt", da);
			writeUtf8File(exeDir + L"\\fail_" + suffix + L"_rb.txt", db);
			++failures;
		}
	}

	// Reload: send Start twice over the same pipe and verify the Setting of
	// the second Commit matches a single load.
	{
		Symbols syms;
		for (const wchar_t *s : combos[0].symbols) syms.insert(wstringi(s));

		printf("[%d] reload (%s x2) ... ", idx + 1, combos[0].name);
		fflush(stdout);

		std::shared_ptr<Setting> once  = buildSetting(rbScript, syms);
		std::shared_ptr<Setting> twice = buildSetting(rbScript, syms, 2);

		if (!once || !twice) {
			printf("FAIL (load failed: %s%s)\n",
			       once ? "" : "x1 ", twice ? "" : "x2");
			++failures;
		} else {
			std::wstring da = dumpSetting(*once);
			std::wstring db = dumpSetting(*twice);
			if (da == db) {
				printf("OK\n");
			} else {
				printf("FAIL (settings differ)\n");
				reportFirstDiff(da, db);
				++failures;
			}
		}
	}

	// $LOAD_PATH / require semantics:
	//  - a relative .rb load resolves via $LOAD_PATH (script directory)
	//  - require evaluates a feature once and returns true / false
	//  - directories pushed onto $LOAD_PATH by the script are honored
	{
		printf("[%d] load path / require ... ", idx + 2);
		fflush(stdout);

		std::wstring subDir = exeDir + L"\\__lp_sub__";
		CreateDirectoryW(subDir.c_str(), nullptr);
		writeUtf8File(exeDir + L"\\__lp_load_lib__.rb",
			L"$load_lib_count = ($load_lib_count || 0) + 1\n");
		writeUtf8File(exeDir + L"\\__lp_req_lib__.rb",
			L"$req_lib_count = ($req_lib_count || 0) + 1\n");
		writeUtf8File(subDir + L"\\__lp_sub_lib__.rb", L"$sub_loaded = true\n");

		// Escape backslashes for use in a Ruby string literal.
		std::wstring subDirRb;
		for (wchar_t c : subDir) {
			subDirRb += c;
			if (c == L'\\') subDirRb += L'\\';
		}

		writeUtf8File(exeDir + L"\\__lp_main__.rb",
			L"load \"__lp_load_lib__.rb\"\n"
			L"raise \"load via $LOAD_PATH failed\" unless $load_lib_count == 1\n"
			L"r1 = require \"__lp_req_lib__\"\n"
			L"r2 = require \"__lp_req_lib__\"\n"
			L"raise \"require should return true then false, got \" +\n"
			L"      [r1, r2].inspect unless r1 == true && r2 == false\n"
			L"raise \"require evaluated the file twice\" unless $req_lib_count == 1\n"
			L"$LOAD_PATH.push \"" + subDirRb + L"\"\n"
			L"load \"__lp_sub_lib__.rb\"\n"
			L"raise \"load via pushed $LOAD_PATH entry failed\" unless $sub_loaded\n");

		Symbols syms;
		std::shared_ptr<Setting> s =
			buildSetting(exeDirU8 + "\\__lp_main__.rb", syms);
		if (s) {
			printf("OK\n");
		} else {
			printf("FAIL (script raised or no commit)\n");
			++failures;
		}
	}

	// A relative script name resolves through the config search path, not the
	// current directory: the cwd holds a ".mayu.rb" that raises when reached.
	// The script loads a library next to itself, so its own directory has to
	// be on $LOAD_PATH as well.
	{
		printf("[%d] relative script resolution ... ", idx + 3);
		fflush(stdout);

		writeUtf8File(cfgDir + L"\\__cfg_lib__.rb", L"$cfg_loaded = true\n");
		writeUtf8File(cfgDir + L"\\.mayu.rb",
			L"load \"__cfg_lib__.rb\"\n"
			L"raise \"script-dir load failed\" unless $cfg_loaded\n");

		Symbols syms;
		std::shared_ptr<Setting> s = buildSetting(".mayu.rb", syms);
		if (s) {
			printf("OK\n");
		} else {
			printf("FAIL (script not resolved, or it raised)\n");
			++failures;
		}
	}

	// "<NYAMY_HOME>\Lib" carries .rb libraries: it is on $LOAD_PATH, but not
	// on the config search path, so a .mayu sitting there stays invisible to
	// `load` / `include`.
	{
		printf("[%d] NYAMY_HOME\\Lib ... ", idx + 4);
		fflush(stdout);

		writeUtf8File(libDir + L"\\__home_lib__.rb", L"$home_lib_loaded = true\n");
		writeUtf8File(libDir + L"\\__lib_only__.mayu", L"# lib only\n");
		writeUtf8File(exeDir + L"\\__lib_ok__.rb",
			L"require \"__home_lib__\"\n"
			L"raise \"require from lib failed\" unless $home_lib_loaded\n");
		writeUtf8File(exeDir + L"\\__lib_ng__.rb",
			L"load \"__lib_only__.mayu\"\n");

		Symbols syms;
		std::shared_ptr<Setting> ok = buildSetting(exeDirU8 + "\\__lib_ok__.rb", syms);
		std::shared_ptr<Setting> ng = buildSetting(exeDirU8 + "\\__lib_ng__.rb", syms);
		if (ok && !ng) {
			printf("OK\n");
		} else if (!ok) {
			printf("FAIL (require from lib failed)\n");
			++failures;
		} else {
			printf("FAIL (.mayu in lib was reachable from include)\n");
			++failures;
		}
	}

	// parseScancodeMapBlob: exercise the registry-blob parser directly with
	// hand-built blobs (independent of the machine's actual Scancode Map).
	{
		printf("[%d] parseScancodeMapBlob ... ", idx + 5);
		fflush(stdout);

		auto makeBlob = [](std::vector<uint32_t> entries) {
			// header1, header2, count(=entries incl. null terminator)
			std::vector<uint32_t> dwords = { 0, 0,
				(uint32_t)(entries.size() + 1) };
			for (uint32_t e : entries) dwords.push_back(e);
			dwords.push_back(0); // null terminator entry
			std::vector<unsigned char> bytes;
			for (uint32_t d : dwords) {
				bytes.push_back((unsigned char)(d & 0xFF));
				bytes.push_back((unsigned char)((d >> 8) & 0xFF));
				bytes.push_back((unsigned char)((d >> 16) & 0xFF));
				bytes.push_back((unsigned char)((d >> 24) & 0xFF));
			}
			return bytes;
		};

		bool ok = true;
		std::vector<std::pair<uint16_t, uint16_t>> out;

		// CapsLock(0x3A) -> LeftCtrl(0x1D): entry HIWORD=from, LOWORD=to.
		auto b1 = makeBlob({ 0x003A001Du });
		ok = ok && parseScancodeMapBlob(b1.data(), b1.size(), out)
		        && out.size() == 1
		        && out[0].first == 0x3A && out[0].second == 0x1D;

		// Extended source RightCtrl(E0-0x1D) and a disabled key (to == 0).
		auto b2 = makeBlob({ 0xE01D001Du, 0x003A0000u });
		ok = ok && parseScancodeMapBlob(b2.data(), b2.size(), out)
		        && out.size() == 2
		        && out[0].first == 0xE01D && out[0].second == 0x1D
		        && out[1].first == 0x3A   && out[1].second == 0x0000;

		// Malformed: too short, and a count that overruns the buffer.
		unsigned char tooShort[8] = { 0 };
		ok = ok && !parseScancodeMapBlob(tooShort, sizeof(tooShort), out)
		        && out.empty();
		unsigned char badCount[12] = { 0,0,0,0, 0,0,0,0, 0xFF,0xFF,0,0 };
		ok = ok && !parseScancodeMapBlob(badCount, sizeof(badCount), out);

		if (ok) {
			printf("OK\n");
		} else {
			printf("FAIL\n");
			++failures;
		}
	}

	// sc() / ScancodeMap DSL surface: a script that asserts the contract and
	// raises on any mismatch.  buildSetting returns null if the script raised.
	{
		printf("[%d] sc() / ScancodeMap DSL ... ", idx + 6);
		fflush(stdout);

		writeUtf8File(exeDir + L"\\__sc_test__.rb",
			L"defkey \"ScTestA\", \"ScAliasA\", scan: \"0x1e\"\n"
			L"defkey \"ScTestExt\", scan: \"E0-0x1d\"\n"
			L"raise \"sc int\"        unless sc(0x1c) == 0x1c\n"
			L"raise \"sc int ext\"    unless sc(0xE10F) == 0xE10F\n"
			L"raise \"sc hex str\"    unless sc(\"0x1c\") == 0x1c\n"
			L"raise \"sc E1 str\"     unless sc(\"E1-0x0f\") == 0xE10F\n"
			L"raise \"sc dec str\"    unless sc(\"28\") == 0x1c\n"
			L"raise \"sc name\"       unless sc(\"ScTestA\") == 0x1e\n"
			L"raise \"sc alias\"      unless sc(\"ScAliasA\") == 0x1e\n"
			L"raise \"sc icase\"      unless sc(\"sctesta\") == 0x1e\n"
			L"raise \"sc ext name\"   unless sc(\"ScTestExt\") == 0xe01d\n"
			L"def expect_arg_error\n"
			L"  yield\n"
			L"  false\n"
			L"rescue ArgumentError\n"
			L"  true\n"
			L"end\n"
			L"def expect_no_method\n"
			L"  yield\n"
			L"  false\n"
			L"rescue NoMethodError\n"
			L"  true\n"
			L"end\n"
			L"raise \"range err\"     unless expect_arg_error { sc(0x10000) }\n"
			L"raise \"unknown name\"  unless expect_arg_error { sc(\"NoSuchKeyXYZ\") }\n"
			L"raise \"to is array\"   unless ScancodeMap.to[0x1d].is_a?(Array)\n"
			L"raise \"to takes no arg\" unless expect_arg_error { ScancodeMap.to(0x1d) }\n"
			L"raise \"from withdrawn\"  unless expect_no_method { ScancodeMap.from(0x1d) }\n"
			L"m = ScancodeMap[0x3a]\n"
			L"unless m.nil?\n"
			L"  raise \"map/to consistency\" unless ScancodeMap.to[m].include?(0x3a)\n"
			L"end\n"
			// SCM-REMAP-*: defined exactly when the registry map touches that
			// key in either direction.  Expected values come from the map
			// itself, so this holds whatever the test machine has configured.
			L"esc = !ScancodeMap[0x01].nil? || !ScancodeMap.to[0x01].empty?\n"
			L"lc  = !ScancodeMap[0x1d].nil? || !ScancodeMap.to[0x1d].empty?\n"
			L"raise \"SCM-REMAP-ESC\"   unless symbol_defined?(\"SCM-REMAP-ESC\")   == esc\n"
			L"raise \"SCM-REMAP-LCTRL\" unless symbol_defined?(\"SCM-REMAP-LCTRL\") == lc\n");

		Symbols syms;
		std::shared_ptr<Setting> s =
			buildSetting(exeDirU8 + "\\__sc_test__.rb", syms);
		if (s) {
			printf("OK\n");
		} else {
			printf("FAIL (script raised or no commit)\n");
			++failures;
		}
	}

	// Modifier defaults: what a key or an action says nothing about must stay
	// dontcare (U- / D-, the locks, the window state), the way the old text
	// loader did it.  Without that, only rules written with an explicit `*'
	// match or emit anything.
	{
		printf("[%d] modifier defaults ... ", idx + 7);
		fflush(stdout);

		writeUtf8File(exeDir + L"\\__mod_defaults__.mayu",
			L"include \"109.mayu\"\n"
			L"keymap Global\n"
			L"key A = B\n"				// nothing specified on either side
			L"key C-D = E\n"			// basic modifier on the left side
			L"key U-*F = G\n"			// U- only: key release
			L"key *H = D-I\n"			// D- only: press without release
			L"key *J = &Sync\n"			// function without a modifier
			L"key *K = C-&Sync\n"		// function with a modifier
			L"key *L = *&Sync\n");		// function ignoring the modifiers
		// the same through the Ruby DSL, which compiles the two sides via
		// separate entry points
		writeUtf8File(exeDir + L"\\__mod_defaults__.rb",
			L"load \"__mod_defaults__.mayu\"\n"
			L"keymap \"Global\" do\n"
			L"  key[\"M\"] = \"N\"\n"
			L"  key[\"C-S-Z\"] = \"&WindowMaximize\"\n"
			L"end\n");

		Symbols syms;
		std::shared_ptr<Setting> s =
			buildSetting(exeDirU8 + "\\__mod_defaults__.rb", syms);

		int bad = 0;
		auto check = [&](bool cond, const char *what) {
			if (!cond) { printf("\n  %s: FAILED", what); ++bad; }
		};
		const Keymap *km = s ? findKeymap(*s, L"Global") : nullptr;
		check(km != nullptr, "keymap Global exists");
		if (km) {
			Keyboard &kb = const_cast<Keyboard &>(s->m_keyboard);
			auto key = [&](const wchar_t *name) {
				return kb.searchKey(wstringi(name));
			};
			Held locksHeld; locksHeld.locks = true;
			Held ctrlHeld;  ctrlHeld.ctrl = true;

			// `key A = B': matches press and release, whatever the locks say,
			// and emits B on both.
			Action::Type type = Action::Type_key;
			const Modifier *mod =
				firstActionModifier(*km, physicalKey(key(L"A"), true), &type);
			check(mod != nullptr, "bare key matches a press");
			check(firstActionModifier(*km, physicalKey(key(L"A"), false)) != nullptr,
				  "bare key matches a release");
			check(firstActionModifier(*km,
					physicalKey(key(L"A"), true, locksHeld)) != nullptr,
				  "bare key ignores the lock modifiers");
			if (mod) {
				check(type == Action::Type_key, "bare rhs is a key action");
				check(mod->isDontcare(Modifier::Type_Up) &&
					  mod->isDontcare(Modifier::Type_Down),
					  "bare rhs key emits both press and release");
			}

			// `key C-D = E': Control must be held.
			check(firstActionModifier(*km,
					physicalKey(key(L"D"), true, ctrlHeld)) != nullptr,
				  "C- key matches with Control held");
			check(firstActionModifier(*km,
					physicalKey(key(L"D"), true)) == nullptr,
				  "C- key does not match without Control");

			// `key U-*F = G' / `key *H = D-I': U- and D- select the direction.
			check(firstActionModifier(*km, physicalKey(key(L"F"), false)) != nullptr,
				  "U- key matches a release");
			check(firstActionModifier(*km, physicalKey(key(L"F"), true)) == nullptr,
				  "U- key does not match a press");
			mod = firstActionModifier(*km, physicalKey(key(L"H"), true));
			check(mod != nullptr, "rhs D- key matches");
			if (mod)
				check(!mod->isDontcare(Modifier::Type_Down) &&
					  mod->isOn(Modifier::Type_Down) &&
					  !mod->isDontcare(Modifier::Type_Up) &&
					  !mod->isOn(Modifier::Type_Up),
					  "rhs D- key emits the press only");

			// `&Sync' without a modifier runs on the press and releases every
			// basic modifier; with `C-' it holds Control; with `*' it leaves
			// the modifiers alone.
			mod = firstActionModifier(*km, physicalKey(key(L"J"), true), &type);
			check(mod != nullptr && type == Action::Type_function,
				  "bare function is a function action");
			if (mod) {
				check(mod->isDontcare(Modifier::Type_Up) &&
					  mod->isDontcare(Modifier::Type_Down),
					  "bare function runs on press and release");
				check(!mod->isDontcare(Modifier::Type_Control) &&
					  !mod->isOn(Modifier::Type_Control) &&
					  !mod->isDontcare(Modifier::Type_Shift) &&
					  !mod->isOn(Modifier::Type_Shift),
					  "bare function releases the basic modifiers");
			}
			mod = firstActionModifier(*km, physicalKey(key(L"K"), true));
			check(mod != nullptr, "C-&Sync matches");
			if (mod) {
				check(mod->isDontcare(Modifier::Type_Down) ||
					  mod->isOn(Modifier::Type_Down),
					  "C-&Sync runs on the press");
				check(!mod->isDontcare(Modifier::Type_Control) &&
					  mod->isOn(Modifier::Type_Control),
					  "C-&Sync holds Control");
				check(!mod->isDontcare(Modifier::Type_Alt) &&
					  !mod->isOn(Modifier::Type_Alt),
					  "C-&Sync releases Alt");
			}
			mod = firstActionModifier(*km, physicalKey(key(L"L"), true));
			check(mod != nullptr, "*&Sync matches");
			if (mod)
				check(mod->isDontcare(Modifier::Type_Control) &&
					  mod->isDontcare(Modifier::Type_Shift),
					  "*&Sync leaves the modifiers alone");

			// The Ruby DSL must arrive at the same defaults.
			mod = firstActionModifier(*km, physicalKey(key(L"M"), true), &type);
			check(mod != nullptr, "DSL: bare key matches a press");
			if (mod)
				check(type == Action::Type_key &&
					  mod->isDontcare(Modifier::Type_Up) &&
					  mod->isDontcare(Modifier::Type_Down),
					  "DSL: bare rhs key emits both press and release");
			Held ctrlShift; ctrlShift.ctrl = true; ctrlShift.shift = true;
			mod = firstActionModifier(*km,
					physicalKey(key(L"Z"), true, ctrlShift), &type);
			check(mod != nullptr, "DSL: C-S- key matches");
			if (mod)
				check(type == Action::Type_function &&
					  mod->isDontcare(Modifier::Type_Down),
					  "DSL: bare function runs on the press");
		}

		if (bad == 0) {
			printf("OK\n");
		} else {
			printf("\n  FAIL (%d check(s))\n", bad);
			++failures;
		}
	}

	// `key <MODIFIER> = <MODIFIER>' changed the default modifiers of every line
	// below it.  The statement is gone and must be rejected, not read as an
	// assignment to a key named "=".
	{
		printf("[%d] default modifier statement rejected ... ", idx + 8);
		fflush(stdout);

		writeUtf8File(exeDir + L"\\__mod_stmt__.mayu",
			L"include \"109.mayu\"\n"
			L"keymap Global\n"
			L"key ~NL- =\n");
		writeUtf8File(exeDir + L"\\__mod_stmt__.rb",
			L"load \"__mod_stmt__.mayu\"\n");

		Symbols syms;
		std::shared_ptr<Setting> s =
			buildSetting(exeDirU8 + "\\__mod_stmt__.rb", syms);
		if (!s) {
			printf("OK\n");
		} else {
			printf("FAIL (the load should have failed)\n");
			++failures;
		}
	}

	// A modifier that the context does not allow is an error, as it was in the
	// old text loader: R- belongs to the left side, not to an action.
	{
		printf("[%d] out-of-context modifier rejected ... ", idx + 9);
		fflush(stdout);

		writeUtf8File(exeDir + L"\\__mod_bad__.mayu",
			L"include \"109.mayu\"\n"
			L"keymap Global\n"
			L"key *A = R-B\n");
		writeUtf8File(exeDir + L"\\__mod_bad__.rb",
			L"load \"__mod_bad__.mayu\"\n");

		Symbols syms;
		std::shared_ptr<Setting> s =
			buildSetting(exeDirU8 + "\\__mod_bad__.rb", syms);
		if (!s) {
			printf("OK\n");
		} else {
			printf("FAIL (the load should have failed)\n");
			++failures;
		}
	}

	// Every keymap must end up knowing which keys are modifiers: the keys named
	// by `defmod' plus whatever the parent keymap assigns, folded in on top of
	// the keymap's own `mod' statements.  The engine reads this list to decide
	// whether Control is held; when it stays empty every key looks unmodified
	// and a spurious modifier release is injected before it.
	{
		printf("[%d] keymap modifier assignments ... ", idx + 10);
		fflush(stdout);

		writeUtf8File(exeDir + L"\\__mod_assign__.mayu",
			L"include \"109.mayu\"\n"
			L"keymap Global\n"
			L"keymap ModChild : Global\n");
		writeUtf8File(exeDir + L"\\__mod_assign__.rb",
			L"load \"__mod_assign__.mayu\"\n");

		Symbols syms;
		std::shared_ptr<Setting> s =
			buildSetting(exeDirU8 + "\\__mod_assign__.rb", syms);

		int bad = 0;
		auto check = [&](bool cond, const char *what) {
			if (!cond) { printf("\n  %s: FAILED", what); ++bad; }
		};
		auto assigns = [&](const Keymap *km, Modifier::Type mt,
		                   const wchar_t *keyName) {
			if (!km) return false;
			Keyboard &kb = const_cast<Keyboard &>(s->m_keyboard);
			Key *key = kb.searchKey(wstringi(keyName));
			if (!key) return false;
			const Keymap::ModAssignments &ma = km->getModAssignments(mt);
			for (Keymap::ModAssignments::const_iterator i = ma.begin();
			     i != ma.end(); ++ i)
				if ((*i).m_key == key)
					return true;
			return false;
		};

		const Keymap *global = s ? findKeymap(*s, L"Global") : nullptr;
		const Keymap *child = s ? findKeymap(*s, L"ModChild") : nullptr;
		check(global != nullptr, "keymap Global exists");
		check(child != nullptr, "keymap ModChild exists");
		// from `defmod'
		check(assigns(global, Modifier::Type_Control, L"LeftControl"),
		      "Global: LeftControl is a Control modifier");
		check(assigns(global, Modifier::Type_Control, L"RightControl"),
		      "Global: RightControl is a Control modifier");
		check(assigns(global, Modifier::Type_Shift, L"LeftShift"),
		      "Global: LeftShift is a Shift modifier");
		check(assigns(global, Modifier::Type_Alt, L"LeftAlt"),
		      "Global: LeftAlt is an Alt modifier");
		check(assigns(global, Modifier::Type_Windows, L"LeftWindows"),
		      "Global: LeftWindows is a Windows modifier");
		// from 109.mayu's own `mod shift += E0RightShift'
		check(assigns(global, Modifier::Type_Shift, L"E0RightShift"),
		      "Global: E0RightShift is a Shift modifier");
		// inherited by a child keymap
		check(assigns(child, Modifier::Type_Control, L"LeftControl"),
		      "ModChild inherits the Control modifiers");

		if (bad == 0) {
			printf("OK\n");
		} else {
			printf("\n  FAIL (%d check(s))\n", bad);
			++failures;
		}
	}

	// `def option nls-keys' lists the scan codes whose break event never
	// arrives, so the engine can synthesize one.  E0/E1 prefixed codes are
	// stored as 0xE0nn / 0xE1nn.
	{
		printf("[%d] nls-keys option ... ", idx + 11);
		fflush(stdout);

		writeUtf8File(exeDir + L"\\__nls_keys__.mayu",
			L"include \"109.mayu\"\n"
			L"def option nls-keys = \"0x3a, E0-0x29, 112\"\n");
		writeUtf8File(exeDir + L"\\__nls_keys__.rb",
			L"load \"__nls_keys__.mayu\"\n");

		Symbols syms;
		std::shared_ptr<Setting> s =
			buildSetting(exeDirU8 + "\\__nls_keys__.rb", syms);

		int bad = 0;
		auto check = [&](bool cond, const char *what) {
			if (!cond) { printf("\n  %s: FAILED", what); ++bad; }
		};
		check(s != nullptr, "setting built");
		if (s) {
			check(s->m_nlsKeys.size() == 3, "three scan codes parsed");
			check(s->m_nlsKeys.count(0x3a) == 1, "hex scan code parsed");
			check(s->m_nlsKeys.count(0xe029) == 1, "E0- prefix parsed");
			check(s->m_nlsKeys.count(112) == 1, "decimal scan code parsed");

			// what keyboardDetour() asks on every key event
			check(s->isNlsKey(0x3a, 0), "0x3a is an NLS key");
			check(!s->isNlsKey(0x3a, KEYBOARD_INPUT_DATA::E0),
				  "E0-0x3a is not, the bare 0x3a is");
			check(s->isNlsKey(0x29, KEYBOARD_INPUT_DATA::E0),
				  "E0-0x29 is an NLS key");
			check(!s->isNlsKey(0x29, 0), "bare 0x29 is not");
			check(!s->isNlsKey(0x1d, 0), "an unlisted scan code is not");
		}

		// Key names defined by defkey stand in for scan codes.  109.mayu names
		// these keys in Japanese, spelled here with \u escapes to keep this
		// file ASCII:
		//   Eisuu     (0x3a)    -> U+82F1 U+6570
		//   E0Zenkaku (E0-0x29) -> "E0" U+534A U+89D2 '/' U+5168 U+89D2
		//   E0Eisuu   (E0-0x3a) -> "E0" U+82F1 U+6570
		// The last one is both a valid key name and something that looks like
		// an E0 prefix, so it pins down the disambiguation rule as well.
		writeUtf8File(exeDir + L"\\__nls_names__.rb",
			L"load \"109.mayu\"\n"
			L"defoption \"nls-keys\", value: \""
			L"\u82F1\u6570, "
			L"E0\u534A\u89D2/\u5168\u89D2, "
			L"E0\u82F1\u6570\"\n");
		Symbols symsN;
		std::shared_ptr<Setting> sn =
			buildSetting(exeDirU8 + "\\__nls_names__.rb", symsN);
		check(sn != nullptr, "names: setting built");
		if (sn) {
			check(sn->m_nlsKeys.count(0x3a) == 1, "names: Eisuu -> 0x3a");
			check(sn->m_nlsKeys.count(0xe029) == 1,
				  "names: E0Zenkaku -> E0-0x29");
			check(sn->m_nlsKeys.count(0xe03a) == 1,
				  "names: E0Eisuu is a name, not an E0 prefix");
			check(sn->m_nlsKeys.size() == 3, "names: three scan codes");
		}

		// an unknown name is an error, and leaves nothing behind
		writeUtf8File(exeDir + L"\\__nls_bad__.rb",
			L"load \"109.mayu\"\n"
			L"defoption \"nls-keys\", value: \"0x3a, NoSuchKeyXYZ\"\n");
		Symbols symsB;
		std::shared_ptr<Setting> sb =
			buildSetting(exeDirU8 + "\\__nls_bad__.rb", symsB);
		if (sb)
			check(sb->m_nlsKeys.empty(), "an unknown name discards the list");

		// Pause spans two scan codes, so it cannot take a synthesized break
		writeUtf8File(exeDir + L"\\__nls_multi__.rb",
			L"load \"109.mayu\"\n"
			L"defoption \"nls-keys\", value: \"Pause\"\n");
		Symbols symsM;
		std::shared_ptr<Setting> sm =
			buildSetting(exeDirU8 + "\\__nls_multi__.rb", symsM);
		if (sm)
			check(sm->m_nlsKeys.empty(), "a multi-scan-code key is rejected");

		// the Ruby DSL has to reach the same place
		writeUtf8File(exeDir + L"\\__nls_dsl__.rb",
			L"load \"109.mayu\"\n"
			L"defoption \"nls-keys\", value: \"0x3a, E0-0x29, 112\"\n");
		Symbols syms3;
		std::shared_ptr<Setting> s3 =
			buildSetting(exeDirU8 + "\\__nls_dsl__.rb", syms3);
		check(s3 != nullptr, "DSL: setting built");
		if (s3)
			check(s3->m_nlsKeys == s->m_nlsKeys,
				  "DSL: same scan codes as the .mayu form");

		// no option at all means no synthesized breaks anywhere.  This cannot
		// lean on 109.mayu: it names its own NLS keys, so the configuration has
		// to be one that says nothing about them.
		writeUtf8File(exeDir + L"\\__nls_none__.rb",
			L"defkey \"ScNoneA\", scan: \"0x1e\"\n");
		Symbols syms2;
		std::shared_ptr<Setting> s2 =
			buildSetting(exeDirU8 + "\\__nls_none__.rb", syms2);
		check(s2 != nullptr, "setting without the option built");
		if (s2) {
			check(s2->m_nlsKeys.empty(), "default is an empty set");
			check(!s2->isNlsKey(0x3a, 0), "default synthesizes no break");
		}

		if (bad == 0) {
			printf("OK\n");
		} else {
			printf("\n  FAIL (%d check(s))\n", bad);
			++failures;
		}
	}

	// The SCM-REMAP-* symbols are derived from the registry Scancode Map before
	// on_load_setting runs, so both configuration languages must see them: that
	// is the whole point of deriving them in the scripter rather than in the
	// Ruby DSL.  Whether they are actually defined depends on the machine, so
	// this compares the two trees against each other instead of a fixed answer.
	{
		printf("[%d] SCM-REMAP-* symbols ... ", idx + 12);
		fflush(stdout);

		writeUtf8File(exeDir + L"\\__scm_sym__.mayu",
			L"include \"109.mayu\"\n"
			L"keymap Global\n"
			L" if ( SCM-REMAP-ESC )\n"
			L"   key A\t= B\n"
			L" else\n"
			L"   key A\t= C\n"
			L" endif\n"
			L" if ( SCM-REMAP-LCTRL )\n"
			L"   key D\t= E\n"
			L" else\n"
			L"   key D\t= F\n"
			L" endif\n");
		writeUtf8File(exeDir + L"\\__scm_via_mayu__.rb",
			L"load \"__scm_sym__.mayu\"\n");
		writeUtf8File(exeDir + L"\\__scm_via_rb__.rb",
			L"load \"109.mayu\"\n"
			L"keymap \"Global\" do\n"
			L"  key[\"A\"] = symbol_defined?(\"SCM-REMAP-ESC\")   ? \"B\" : \"C\"\n"
			L"  key[\"D\"] = symbol_defined?(\"SCM-REMAP-LCTRL\") ? \"E\" : \"F\"\n"
			L"end\n");

		Symbols syms;
		std::shared_ptr<Setting> a =
			buildSetting(exeDirU8 + "\\__scm_via_mayu__.rb", syms);
		std::shared_ptr<Setting> b =
			buildSetting(exeDirU8 + "\\__scm_via_rb__.rb", syms);

		if (!a || !b) {
			printf("FAIL (load failed: %s%s)\n",
			       a ? "" : ".mayu ", b ? "" : ".mayu.rb");
			++failures;
		} else {
			std::wstring da = dumpSetting(*a);
			std::wstring db = dumpSetting(*b);
			if (da == db) {
				printf("OK\n");
			} else {
				printf("FAIL (settings differ)\n");
				reportFirstDiff(da, db);
				++failures;
			}
		}
	}

	// Both SCM-REMAP-* branches.  Which way they go depends on the machine's
	// registry, so the map is pinned through NYAMY_SCANCODE_MAP: without that
	// the "not remapped" side of every guarded block goes untested on a machine
	// that has a Scancode Map (and vice versa).  The variable is only honoured
	// by a scripter DLL built with NYAMY_TEST_HOOKS, so a build without it can
	// only skip this.
	{
		printf("[%d] SCM-REMAP-* branches ... ", idx + 13);
		fflush(stdout);
#ifndef NYAMY_TEST_HOOKS
		printf("SKIPPED (built without NYAMY_TEST_HOOKS)\n");
#else

		// Raw registry blob as hex: header1, header2, count (including the
		// null terminator), then one DWORD per mapping (HIWORD=from, LOWORD=to).
		auto blobHex = [](std::vector<uint32_t> entries) {
			std::vector<uint32_t> dwords = { 0, 0,
				(uint32_t)(entries.size() + 1) };
			for (uint32_t e : entries) dwords.push_back(e);
			dwords.push_back(0);
			std::wstring hex;
			for (uint32_t d : dwords)
				for (int b = 0; b < 4; ++b) {
					wchar_t buf[3];
					_snwprintf_s(buf, 3, _TRUNCATE, L"%02x",
						(unsigned)((d >> (b * 8)) & 0xFF));
					hex += buf;
				}
			return hex;
		};

		// 0x29 is Zenkaku/Hankaku, the key default.mayu swaps with Esc.
		const uint32_t kZenkakuToEsc = 0x00290001u;  // 0x29 -> Esc
		const uint32_t kEscToZenkaku = 0x00010029u;  // Esc -> 0x29
		const uint32_t kCapsToLCtrl  = 0x003A001Du;  // CapsLock -> LControl
		const uint32_t kLCtrlToCaps  = 0x001D003Au;  // LControl -> CapsLock

		const struct {
			const char *name;
			std::vector<uint32_t> entries;
			bool esc;
			bool lctrl;
		} maps[] = {
			{ "empty map",        {},                          false, false },
			{ "Esc swapped",      { kEscToZenkaku, kZenkakuToEsc }, true, false },
			{ "Esc as target",    { kZenkakuToEsc },            true,  false },
			{ "LControl as target", { kCapsToLCtrl },           false, true  },
			{ "LControl as source", { kLCtrlToCaps },           false, true  },
			{ "both swapped",     { kEscToZenkaku, kZenkakuToEsc,
			                        kCapsToLCtrl, kLCtrlToCaps }, true, true },
		};

		// Symbol sets that between them enter every SCM-guarded block:
		// default/emacsedit need USEdefault + MAP-ESCAPE-TO-META, 104on109
		// needs SunType4.
		const std::vector<const wchar_t *> symbolSets[] = {
			{ L"USE109", L"USEdefault", L"MAP-ESCAPE-TO-META" },
			{ L"USE109", L"USE104on109", L"SunType4" },
		};

		int bad = 0;
		for (const auto &m : maps) {
			SetEnvironmentVariableW(L"NYAMY_SCANCODE_MAP",
				blobHex(m.entries).c_str());

			// the symbols follow the pinned map
			std::wstring probe =
				std::wstring(L"raise \"esc\"   unless symbol_defined?(\"SCM-REMAP-ESC\")   == ") +
				(m.esc ? L"true\n" : L"false\n") +
				L"raise \"lctrl\" unless symbol_defined?(\"SCM-REMAP-LCTRL\") == " +
				(m.lctrl ? L"true\n" : L"false\n");
			writeUtf8File(exeDir + L"\\__scm_probe__.rb", probe);
			Symbols none;
			if (!buildSetting(exeDirU8 + "\\__scm_probe__.rb", none)) {
				printf("\n  %s: symbols do not match the map", m.name);
				++bad;
			}

			// and both configuration trees still agree under it
			for (const auto &set : symbolSets) {
				Symbols syms;
				for (const wchar_t *s : set) syms.insert(wstringi(s));
				std::shared_ptr<Setting> a = buildSetting(viaMayuPath, syms);
				std::shared_ptr<Setting> b = buildSetting(rbScript, syms);
				if (!a || !b) {
					printf("\n  %s: load failed", m.name);
					++bad;
					continue;
				}
				std::wstring da = dumpSetting(*a);
				std::wstring db = dumpSetting(*b);
				if (da != db) {
					printf("\n  %s: settings differ", m.name);
					reportFirstDiff(da, db);
					++bad;
				}
			}
		}
		SetEnvironmentVariableW(L"NYAMY_SCANCODE_MAP", nullptr);

		if (bad == 0) {
			printf("OK\n");
		} else {
			printf("\n  FAIL (%d check(s))\n", bad);
			++failures;
		}
#endif // NYAMY_TEST_HOOKS
	}

	// Regexp: mruby parses /.../ but has no Regexp class, so the binding
	// supplies one on top of std::wregex.  The script asserts the contract and
	// raises on any mismatch; buildSetting returns null if it did.
	{
		printf("[%d] Regexp / MatchData ... ", idx + 14);
		fflush(stdout);

		writeUtf8File(exeDir + L"\\__re_test__.rb",
			// the real class lives under NYamy; ::Regexp is an alias
			L"raise \"class name\" unless /a/.class.to_s == \"NYamy::Regexp\"\n"
			L"raise \"alias\"      unless Regexp.equal?(NYamy::Regexp)\n"
			L"raise \"md alias\"   unless MatchData.equal?(NYamy::MatchData)\n"
			// source / pattern / options / inspect / to_s
			L"raise \"source\"  unless /a\\d+/.source == \"a\\\\d+\"\n"
			L"raise \"pattern\" unless /ab/.pattern == \"ab\"\n"
			L"raise \"options\" unless /a/im.options == 5\n"
			L"raise \"opt x\"   unless /a/x.options == Regexp::EXTENDED\n"
			L"raise \"opt n\"   unless /a/n.options == 32\n"
			L"raise \"inspect\" unless /a/xim.inspect == \"/a/mix\"\n"
			L"raise \"to_s\"    unless /a/i.to_s == \"(?i-mx:a)\"\n"
			// interpolation, and '/' as division where it is not a literal
			L"x = \"b\"\n"
			L"raise \"interp\" unless /a#{x}c/.source == \"abc\"\n"
			L"y = 4\n"
			L"raise \"div\"  unless 6/2 == 3\n"
			L"raise \"div2\" unless y / 2 == 2\n"
			// matching
			L"raise \"match?\" unless /b+/.match?(\"abbbc\")\n"
			L"raise \"=~\"     unless (/b+/ =~ \"abbbc\") == 1\n"
			L"raise \"===\"    unless /b+/ === \"abbbc\"\n"
			L"m = /a(b+)(z)?c/.match(\"xxabbbc\")\n"
			L"raise \"md\"       if m.nil?\n"
			L"raise \"md[0]\"    unless m[0] == \"abbbc\"\n"
			L"raise \"md[1]\"    unless m[1] == \"bbb\"\n"
			L"raise \"md[2]\"    unless m[2].nil?\n"
			L"raise \"md size\"  unless m.size == 3\n"
			L"raise \"captures\" unless m.captures == [\"bbb\", nil]\n"
			L"raise \"pre\"      unless m.pre_match == \"xx\"\n"
			L"raise \"post\"     unless m.post_match == \"\"\n"
			L"raise \"begin\"    unless m.begin(0) == 2\n"
			L"raise \"end\"      unless m.end(1) == 6\n"
			// $~ / $1..$9 are plain globals in mruby
			L"raise \"$~\"       unless $~[1] == \"bbb\"\n"
			L"raise \"$1\"       unless $1 == \"bbb\"\n"
			L"raise \"no match\" unless /zzz/.match(\"abc\").nil?\n"
			L"raise \"$~ nil\"   unless $~.nil?\n"
			L"/l+/ =~ \"hello\"\n"
			L"raise \"last_match\" unless Regexp.last_match(0) == \"ll\"\n"
			// String side
			L"raise \"s =~\"      unless (\"hello\" =~ /l+/) == 2\n"
			L"raise \"s match\"   unless \"hello\".match(/l+/)[0] == \"ll\"\n"
			L"raise \"s match?\"  unless \"hello\".match?(/^he/)\n"
			L"raise \"to_regexp\" unless \"a+\".to_regexp.source == \"a+\"\n"
			L"raise \"escape\"    unless Regexp.escape(\"a.b\") == \"a\\\\.b\"\n"
			// 'm' is ECMAScript multiline, not Ruby's dot-matches-newline
			L"raise \"m anchors\"  unless (/^b$/m =~ \"a\\nb\") == 2\n"
			L"raise \"no m\"       unless (/^b$/ =~ \"a\\nb\").nil?\n"
			L"raise \"not dotall\" unless (/a.b/m =~ \"a\\nb\").nil?\n"
			// 'x' rewrites the pattern; source keeps what was written
			// a real tab, not the \t escape: /x drops literal whitespace, while
			// an escape sequence stays, exactly as in Ruby
			L"raise \"x ws\"      unless /a b\tc/x.pattern == \"abc\"\n"
			L"raise \"x esc t\"   unless /a\\tb/x.pattern == \"a\\\\tb\"\n"
			L"raise \"x source\"  unless /a b/x.source == \"a b\"\n"
			L"raise \"x esc\"     unless /a\\ b/x.pattern == \"a\\\\ b\"\n"
			L"raise \"x class\"   unless /[a b]/x.pattern == \"[a b]\"\n"
			L"raise \"x comment\" unless /a # drop me\nb/x.pattern == \"ab\"\n"
			L"raise \"x esc #\"   unless /a\\#b/x.pattern == \"a\\\\#b\"\n"
			L"raise \"x cls #\"   unless /a[#]b/x.pattern == \"a[#]b\"\n"
			L"raise \"no x\"      unless /a b/.pattern == \"a b\"\n"
			// integer options, and the flag decoder as the one validation point
			L"raise \"int opts\" unless Regexp.new(\"a\", Regexp::IGNORECASE) == /a/i\n"
			L"def expect_err(cls)\n"
			L"  yield\n"
			L"  false\n"
			L"rescue => e\n"
			L"  e.is_a?(cls)\n"
			L"end\n"
			L"raise \"bad flag\" unless expect_err(ArgumentError) { Regexp.compile(\"a\", \"zz\") }\n"
			L"raise \"bad pat\"  unless expect_err(RegexpError) { Regexp.compile(\"(\") }\n"
			// Onigmo-only syntax the manual lists as unusable really is rejected
			// rather than quietly meaning something else
			// Onigmo-only syntax.  Group constructs are rejected outright, but
			// the letter escapes are read as identity escapes and quietly match
			// the bare letter - the manual has to warn about exactly this.
			L"['(?<n>a)', '(?i)a', '(?<=a)b', '\\p{L}'].each do |p|\n"
			L"  raise \"expected RegexpError: #{p}\" unless expect_err(RegexpError) { Regexp.compile(p) }\n"
			L"end\n"
			L"raise \"\\\\A is literal A\" unless (/\\A/ =~ \"xA\") == 1\n"
			L"raise \"\\\\z is literal z\" unless (/\\z/ =~ \"xz\") == 1\n"
			L"raise \"\\\\h is literal h\" unless (/\\h/ =~ \"xh\") == 1\n"
			// the two examples the manual shows in the "Regexp limitations" section
			L"host = \"desktop-01.example.jp\"\n"
			L"raise \"doc =~\" unless (host =~ /^([^.]+)\\./) == 0\n"
			L"raise \"doc $1\" unless $1.upcase == \"DESKTOP-01\"\n"
			L"doc_re = /\n"
			L"    EXPLORER\\.EXE :       # comment\n"
			L"    .* SysListView32 $    # end\n"
			L"  /x\n"
			L"raise \"doc x\" unless doc_re.pattern == \"EXPLORER\\\\.EXE:.*SysListView32$\"\n");

		Symbols syms;
		std::shared_ptr<Setting> s =
			buildSetting(exeDirU8 + "\\__re_test__.rb", syms);
		if (s) {
			printf("OK\n");
		} else {
			printf("FAIL (script raised or no commit)\n");
			++failures;
		}
	}

	// What a Regexp contributes to class: / title: is the compiled pattern,
	// not its source: under /x the two differ, and it is the compiled one the
	// engine has to see.  Proven by dumping both spellings and comparing.
	{
		printf("[%d] Regexp in class: / title: ... ", idx + 15);
		fflush(stdout);

		writeUtf8File(exeDir + L"\\__re_win_a__.rb",
			L"keymap \"Global\"\n"
			L"window \"W1\", class: /^Foo:Bar$/, parent: \"Global\"\n"
			L"window \"W2\", class: /a b/x, title: /t x/x, parent: \"Global\"\n"
			L"window \"W3\", class: /^Baz$/i, parent: \"Global\"\n");
		writeUtf8File(exeDir + L"\\__re_win_b__.rb",
			L"keymap \"Global\"\n"
			L"window \"W1\", class: '^Foo:Bar$', parent: \"Global\"\n"
			L"window \"W2\", class: 'ab', title: 'tx', parent: \"Global\"\n"
			L"window \"W3\", class: '^Baz$', parent: \"Global\"\n");

		Symbols syms;
		std::shared_ptr<Setting> a =
			buildSetting(exeDirU8 + "\\__re_win_a__.rb", syms);
		std::shared_ptr<Setting> b =
			buildSetting(exeDirU8 + "\\__re_win_b__.rb", syms);
		if (!a || !b) {
			printf("FAIL (load failed)\n");
			++failures;
		} else {
			std::wstring da = dumpSetting(*a);
			std::wstring db = dumpSetting(*b);
			if (da == db) {
				printf("OK\n");
			} else {
				printf("\n  settings differ\n");
				reportFirstDiff(da, db);
				++failures;
			}
		}
	}

	// The two notices: 'm' is reported once per pattern because it does not
	// mean what Ruby's /m means, and flags that cannot travel to the engine
	// are reported where they are used.  'x' travels (it is folded into the
	// pattern), so it must stay quiet.
	{
		printf("[%d] Regexp log output ... ", idx + 16);
		fflush(stdout);

		writeUtf8File(exeDir + L"\\__re_log__.rb",
			L"keymap \"Global\"\n"
			L"window \"WarnMe\",  class: /^a$/i, parent: \"Global\"\n"
			L"window \"QuietX\",  class: /a b/x, parent: \"Global\"\n"
			L"window \"QuietNo\", class: /^plain$/, parent: \"Global\"\n"
			L"10.times { /^a$/m }\n");

		// Capture stderr, where the scripter writes its log, by swapping the
		// file descriptor rather than reopening the console afterwards: the
		// test may well be running without one.
		std::wstring logPath = exeDir + L"\\__re_log__.txt";
		fflush(stderr);
		int savedFd = _dup(_fileno(stderr));
		FILE *redirected = nullptr;
		_wfreopen_s(&redirected, logPath.c_str(), L"w", stderr);

		Symbols syms;
		std::shared_ptr<Setting> s =
			buildSetting(exeDirU8 + "\\__re_log__.rb", syms);

		fflush(stderr);
		_dup2(savedFd, _fileno(stderr));
		_close(savedFd);
		clearerr(stderr);

		std::vector<std::string> lines;
		{
			std::ifstream in(logPath.c_str());
			std::string line;
			while (std::getline(in, line)) {
				if (!line.empty() && line.back() == '\r')
					line.pop_back();
				lines.push_back(line);
			}
		}
		auto count = [&](const char *tag, const char *needle) {
			int n = 0;
			for (const std::string &l : lines)
				if (l.compare(0, 2, tag) == 0 &&
					l.find(needle) != std::string::npos)
					++n;
			return n;
		};

		int bad = 0;
		auto check = [&](bool cond, const char *what) {
			if (!cond) { printf("\n  %s: FAILED", what); ++bad; }
		};
		check(s != nullptr, "script loaded");
		check(count("W|", "\"WarnMe\"") == 1, "/i on class: warns");
		check(count("W|", "\"QuietX\"") == 0, "/x on class: stays quiet");
		check(count("W|", "\"QuietNo\"") == 0, "no flags stays quiet");
		check(count("I|", "not dotall") == 1, "/m notice is emitted once");

		if (bad == 0) {
			printf("OK\n");
		} else {
			printf("\n  FAIL (%d check(s))\n", bad);
			++failures;
		}
	}

	// In the Ruby DSL a keymap statement declares the keymap; where the
	// assignments that follow it land is the block's business.  With a block,
	// what follows the block belongs to the keymap that was in effect before
	// it; without one, nothing moves at all and the assignments stay at the
	// top level (Global).  .mayu keeps its own rule - see the test after this.
	{
		printf("[%d] keymap block scope ... ", idx + 17);
		fflush(stdout);

		writeUtf8File(exeDir + L"\\__scope__.rb",
			L"load \"109.mayu.rb\"\n"
			L"keymap \"Global\" do\n"
			L"  key[\"C-A\"] = \"Home\"\n"
			L"end\n"
			L"window \"W1\", class: 'w1', parent: \"Global\" do\n"
			L"  key[\"C-B\"] = \"End\"\n"
			L"end\n"
			L"key[\"C-C\"] = \"Delete\"\n"
			L"keymap \"K2\", parent: \"Global\" do\n"
			L"  window \"W2\", class: 'w2', parent: \"Global\" do\n"
			L"    key[\"C-D\"] = \"Left\"\n"
			L"  end\n"
			L"  key[\"C-E\"] = \"Right\"\n"
			L"end\n"
			L"key[\"C-F\"] = \"Up\"\n"
			L"window \"W3\", class: 'w3', parent: \"Global\"\n"
			L"key[\"C-G\"] = \"Down\"\n"
			// declaring first and filling in later has to keep the parent
			L"keymap \"K4\", parent: \"K2\"\n"
			L"keymap \"K4\" do\n"
			L"  key[\"C-H\"] = \"Left\"\n"
			L"end\n");

		Symbols syms;
		std::shared_ptr<Setting> s =
			buildSetting(exeDirU8 + "\\__scope__.rb", syms);

		int bad = 0;
		auto check = [&](bool cond, const char *what) {
			if (!cond) { printf("\n  %s: FAILED", what); ++bad; }
		};
		// Whether the keymap itself carries the assignment, without looking at
		// its parents: that is exactly the question the scope answers.
		auto assigned = [&](const wchar_t *keymapName, const wchar_t *keyName) {
			if (!s) return false;
			const Keymap *km = findKeymap(*s, keymapName);
			if (!km) return false;
			Keyboard &kb = const_cast<Keyboard &>(s->m_keyboard);
			Key *key = kb.searchKey(wstringi(keyName));
			if (!key) return false;
			Held held;
			held.ctrl = true;
			return km->searchAssignment(physicalKey(key, true, held)) != nullptr;
		};

		check(s != nullptr, "script loaded");
		check(assigned(L"Global", L"A"), "C-A lands in Global");
		check(assigned(L"W1", L"B"),     "C-B lands in the window block");
		check(assigned(L"Global", L"C"), "C-C after the block returns to Global");
		check(!assigned(L"W1", L"C"),    "C-C does not leak into the window");
		check(assigned(L"W2", L"D"),     "C-D lands in the inner block");
		check(assigned(L"K2", L"E"),     "C-E returns to the enclosing block");
		check(!assigned(L"W2", L"E"),    "C-E does not leak out of the inner block");
		check(assigned(L"Global", L"F"), "C-F returns to Global from a nested block");
		check(assigned(L"Global", L"G"), "C-G stays at the top level");
		check(!assigned(L"W3", L"G"),    "a blockless window takes nothing");
		check(assigned(L"K4", L"H"),     "C-H lands in the block that fills K4 in");
		const Keymap *k4 = s ? findKeymap(*s, L"K4") : nullptr;
		check(k4 && k4->getParentKeymap() &&
		      std::wstring(k4->getParentKeymap()->getName().c_str()) == L"K2",
		      "a blockless declaration keeps its parent");

		if (bad == 0) {
			printf("OK\n");
		} else {
			printf("\n  FAIL (%d check(s))\n", bad);
			++failures;
		}
	}

	// Where a `load' leaves the keymap context.  A .mayu keymap statement stays
	// in effect until the next one - inside that file - but the rule stops at
	// the file: what the script writes after the load belongs to the script's
	// own top level.  The same goes for a .rb that ends on a blockless keymap.
	{
		printf("[%d] keymap context across load ... ", idx + 18);
		fflush(stdout);

		writeUtf8File(exeDir + L"\\__scope_lib__.rb",
			L"window \"L1\", class: 'l1', parent: \"Global\"\n");
		writeUtf8File(exeDir + L"\\__scope_leg__.mayu",
			L"include \"109.mayu\"\n"
			L"keymap Global\n"
			L"keymap KLeg : Global\n"
			L"key C-B\t= End\n");
		writeUtf8File(exeDir + L"\\__scope_load__.rb",
			L"load \"__scope_leg__.mayu\"\n"
			L"key[\"C-C\"] = \"Delete\"\n"
			L"load \"__scope_lib__.rb\"\n"
			L"key[\"C-A\"] = \"Home\"\n");

		Symbols syms;
		std::shared_ptr<Setting> s =
			buildSetting(exeDirU8 + "\\__scope_load__.rb", syms);

		int bad = 0;
		auto check = [&](bool cond, const char *what) {
			if (!cond) { printf("\n  %s: FAILED", what); ++bad; }
		};
		auto assigned = [&](const wchar_t *keymapName, const wchar_t *keyName) {
			if (!s) return false;
			const Keymap *km = findKeymap(*s, keymapName);
			if (!km) return false;
			Keyboard &kb = const_cast<Keyboard &>(s->m_keyboard);
			Key *key = kb.searchKey(wstringi(keyName));
			if (!key) return false;
			Held held;
			held.ctrl = true;
			return km->searchAssignment(physicalKey(key, true, held)) != nullptr;
		};

		check(s != nullptr, "script loaded");
		check(assigned(L"KLeg", L"B"),   ".mayu keeps its own keymap context");
		check(assigned(L"Global", L"C"), "the .mayu context does not escape");
		check(!assigned(L"KLeg", L"C"),  "C-C does not land in the .mayu keymap");
		check(assigned(L"Global", L"A"), "a blockless window in a .rb does not escape");
		check(!assigned(L"L1", L"A"),    "C-A does not land in the loaded window");

		if (bad == 0) {
			printf("OK\n");
		} else {
			printf("\n  FAIL (%d check(s))\n", bad);
			++failures;
		}
	}

	// `define' in one included .mayu is visible to the `if' of the next one.
	// Each include is compiled on its own, so the symbols have to be carried
	// across; nesting an include has always worked, and these have to match.
	{
		printf("[%d] symbols across sibling includes ... ", idx + 19);
		fflush(stdout);

		writeUtf8File(exeDir + L"\\__sib_a__.mayu", L"define SIB-A\n");
		writeUtf8File(exeDir + L"\\__sib_b__.mayu",
			L"if ( SIB-A )\n"
			L"  define SIB-B-SAW-A\n"
			L"endif\n");
		writeUtf8File(exeDir + L"\\__sib_main__.rb",
			L"load \"__sib_a__.mayu\"\n"
			L"load \"__sib_b__.mayu\"\n");

		Symbols syms;
		std::shared_ptr<Setting> s =
			buildSetting(exeDirU8 + "\\__sib_main__.rb", syms);

		bool saw = s && s->m_symbols.count(wstringi(L"SIB-B-SAW-A")) > 0;
		if (saw) {
			printf("OK\n");
		} else {
			printf("FAIL (%s)\n",
			       s ? "the second include did not see the first define"
			         : "load failed");
			++failures;
		}
	}

	// ENV: a read-only view of the process environment.
	{
		printf("[%d] ENV ... ", idx + 20);
		fflush(stdout);

		SetEnvironmentVariableW(L"NYAMY_TEST_ENV", L"hello");
		SetEnvironmentVariableW(L"NYAMY_TEST_ENV_MISSING", nullptr);

		writeUtf8File(exeDir + L"\\__env__.rb",
			L"raise \"[] failed\" unless ENV[\"NYAMY_TEST_ENV\"] == \"hello\"\n"
			L"raise \"[] on unset\" unless ENV[\"NYAMY_TEST_ENV_MISSING\"].nil?\n"
			L"raise \"symbol key\" unless ENV[:NYAMY_TEST_ENV] == \"hello\"\n"
			L"raise \"key? true\"  unless ENV.key?(\"NYAMY_TEST_ENV\")\n"
			L"raise \"key? false\" if ENV.key?(\"NYAMY_TEST_ENV_MISSING\")\n"
			L"raise \"fetch\" unless ENV.fetch(\"NYAMY_TEST_ENV\") == \"hello\"\n"
			L"raise \"fetch default\" unless\n"
			L"  ENV.fetch(\"NYAMY_TEST_ENV_MISSING\", \"d\") == \"d\"\n"
			L"raised = false\n"
			L"begin\n"
			L"  ENV.fetch(\"NYAMY_TEST_ENV_MISSING\")\n"
			L"rescue KeyError\n"
			L"  raised = true\n"
			L"end\n"
			L"raise \"fetch should raise KeyError\" unless raised\n"
			L"raise \"keys\" unless ENV.keys.include?(\"NYAMY_TEST_ENV\")\n"
			L"raise \"to_h\" unless ENV.to_h[\"NYAMY_TEST_ENV\"] == \"hello\"\n"
			L"n = 0\n"
			L"ENV.each { |k, v| n += 1 if k == \"NYAMY_TEST_ENV\" && v == \"hello\" }\n"
			L"raise \"each\" unless n == 1\n");

		Symbols syms;
		std::shared_ptr<Setting> s =
			buildSetting(exeDirU8 + "\\__env__.rb", syms);

		SetEnvironmentVariableW(L"NYAMY_TEST_ENV", nullptr);
		if (s) {
			printf("OK\n");
		} else {
			printf("FAIL (script raised or no commit)\n");
			++failures;
		}
	}

	// NYAMY_LOAD_PATH adds to $LOAD_PATH.  A relative element is dropped with a
	// warning instead of failing the load: a stray environment variable should
	// not be able to stop nyamy from starting.
	{
		printf("[%d] NYAMY_LOAD_PATH ... ", idx + 21);
		fflush(stdout);

		std::wstring lpDir = exeDir + L"\\__nlp__";
		CreateDirectoryW(lpDir.c_str(), nullptr);
		writeUtf8File(lpDir + L"\\__nlp_lib__.rb", L"$nlp_loaded = true\n");
		writeUtf8File(exeDir + L"\\__nlp_main__.rb",
			L"load \"__nlp_lib__.rb\"\n"
			L"raise \"NYAMY_LOAD_PATH entry not searched\" unless $nlp_loaded\n");

		SetEnvironmentVariableW(L"NYAMY_LOAD_PATH",
		                        (L"relative\\path;" + lpDir).c_str());

		Symbols syms;
		std::shared_ptr<Setting> s =
			buildSetting(exeDirU8 + "\\__nlp_main__.rb", syms);

		SetEnvironmentVariableW(L"NYAMY_LOAD_PATH", nullptr);
		if (s) {
			printf("OK\n");
		} else {
			printf("FAIL (script raised or no commit)\n");
			++failures;
		}
	}

	// The command line parser, which main() is the only production caller of.
	{
		printf("[%d] command line options ... ", idx + 22);
		fflush(stdout);

		int bad = 0;
		auto check = [&](bool cond, const char *what) {
			if (!cond) { printf("\n  %s: FAILED", what); ++bad; }
		};

		{
			const char *argv[] = { "nyamy-scripter", "-I", "C:\\a;D:/b\\",
			                       "-DSYM1", "-D", "SYM2", "--",
			                       "-script.rb", "arg1" };
			CliOptions o;
			std::string err;
			bool ok = parseCliOptions((int)(sizeof(argv) / sizeof(argv[0])),
			                          argv, &o, &err);
			check(ok, "full command line parses");
			check(o.includeDirs.size() == 2, "two -I directories");
			check(o.includeDirs.size() == 2 && o.includeDirs[0] == "C:\\a" &&
			      o.includeDirs[1] == "D:\\b",
			      "-I is split on ';', slashes and trailing separator normalized");
			check(o.symbols.size() == 2 && o.symbols[0] == "SYM1" &&
			      o.symbols[1] == "SYM2", "-D glued and separated both work");
			check(o.scriptArgIndex == 7, "-- makes the next argument the script");
		}
		{
			const char *argv[] = { "nyamy-scripter", "script.rb" };
			CliOptions o;
			std::string err;
			check(parseCliOptions(2, argv, &o, &err) && o.scriptArgIndex == 1,
			      "a bare script is the script");
		}
		{
			const char *argv[] = { "nyamy-scripter", "--help" };
			CliOptions o;
			std::string err;
			check(parseCliOptions(2, argv, &o, &err) && o.showHelp &&
			      o.scriptArgIndex == 0, "--help needs no script");
		}
		{
			const char *argv[] = { "nyamy-scripter", "-I", "lib", "s.rb" };
			CliOptions o;
			std::string err;
			check(!parseCliOptions(4, argv, &o, &err), "relative -I is rejected");
		}
		{
			const char *argv[] = { "nyamy-scripter", "-I", "\\lib", "s.rb" };
			CliOptions o;
			std::string err;
			check(!parseCliOptions(4, argv, &o, &err),
			      "drive-relative -I is rejected");
		}
		{
			const char *argv[] = { "nyamy-scripter", "-D" };
			CliOptions o;
			std::string err;
			check(!parseCliOptions(2, argv, &o, &err), "-D without a value fails");
		}
		{
			const char *argv[] = { "nyamy-scripter", "-Z", "s.rb" };
			CliOptions o;
			std::string err;
			check(!parseCliOptions(3, argv, &o, &err), "an unknown option fails");
		}

		if (bad == 0) {
			printf("OK\n");
		} else {
			printf("\n  FAIL (%d check(s))\n", bad);
			++failures;
		}
	}

	// Every argument type has to survive the trip to a user function: the ctrl
	// stream used to carry only strings and numbers, and an argument it could
	// not write vanished while its count stayed - which desynchronized the
	// reader and killed every request that followed.  Hence the second call.
	{
		printf("[%d] ExecUserFunc arguments ... ", idx + 23);
		fflush(stdout);

		writeUtf8File(exeDir + L"\\__uf_args__.rb",
			L"load \"109.mayu.rb\"\n"
			L"deffunc \"T13Args\" do |s, n, re, ks, mod, toks|\n"
			L"  log.info \"T13 s=#{s} n=#{n} re=#{re.source}\" +\n"
			L"           \" ks=#{ks.class} mod=#{mod.class} toks=#{toks.inspect}\"\n"
			L"end\n");

		ExecUserFuncRequest req;
		req.name = wstringi(L"T13Args");
		req.args.emplace_back(std::in_place_type<FuncArgString>, L"hello");
		req.args.emplace_back(std::in_place_type<FuncArgNumber>, -42);
		req.args.emplace_back(std::in_place_type<FuncArgRegexp>, L"a+b");
		req.args.emplace_back(std::in_place_type<FuncArgKeySeqIdx>, 0u);
		ModifierSpec ms;
		ms.modifiers = 0x12;
		ms.dontcares = 0x34;
		req.args.emplace_back(std::in_place_type<FuncArgModifierSpec>, ms);
		req.args.emplace_back(std::in_place_type<FuncArgTokenSeq>,
			FuncArgTokenSeq{ wstringi(L"A"), wstringi(L"B") });
		req.context.scanCode = 0x1e;
		req.context.windowClass = L"cls";
		req.context.windowTitle = L"ttl";
		std::vector<ExecUserFuncRequest> execs = { req, req };

		std::wstring logPath = exeDir + L"\\__uf_args__.txt";
		fflush(stderr);
		int savedFd = _dup(_fileno(stderr));
		FILE *redirected = nullptr;
		_wfreopen_s(&redirected, logPath.c_str(), L"w", stderr);

		Symbols syms;
		std::shared_ptr<Setting> s =
			buildSetting(exeDirU8 + "\\__uf_args__.rb", syms, 1, &execs);

		fflush(stderr);
		_dup2(savedFd, _fileno(stderr));
		_close(savedFd);
		clearerr(stderr);

		int hits = 0, calls = 0;
		{
			std::ifstream in(logPath.c_str());
			std::string line;
			const char *want =
				"T13 s=hello n=-42 re=a+b ks=NYamy::KeySeq"
				" mod=NYamy::Modifier toks=[\"A\", \"B\"]";
			while (std::getline(in, line)) {
				if (line.find("T13 ") != std::string::npos) ++calls;
				if (line.find(want) != std::string::npos) ++hits;
			}
		}

		int bad = 0;
		auto check = [&](bool cond, const char *what) {
			if (!cond) { printf("\n  %s: FAILED", what); ++bad; }
		};
		check(s != nullptr, "script loaded");
		check(calls == 2, "the handler ran once per request");
		check(hits == 2,  "every argument type arrived intact, twice");

		if (bad == 0) {
			printf("OK\n");
		} else {
			printf("\n  FAIL (%d check(s))\n", bad);
			++failures;
		}
	}

	// A pattern is dumped as a regexp literal, so the delimiter and anything
	// that cannot be shown have to be escaped - and nothing else, since the
	// text is the pattern's own source.
	{
		printf("[%d] regexp literal in dumps ... ", idx + 24);
		fflush(stdout);

		// a/b\/c<TAB>d<e-acute>: a bare delimiter, one that is already escaped,
		// a control character, and a letter no ASCII locale calls printable.
		writeUtf8File(exeDir + L"\\__re_dump__.rb",
			L"load \"109.mayu.rb\"\n"
			L"window \"T13Re\", class: \"a/b\\\\/c\\td\\u00e9\","
			L" parent: \"Global\"\n");

		Symbols syms;
		std::shared_ptr<Setting> s =
			buildSetting(exeDirU8 + "\\__re_dump__.rb", syms);

		std::wstring dump = s ? dumpSetting(*s) : std::wstring();
		const std::wstring want = L"/a\\/b\\/c\\td\u00e9/";

		if (s && dump.find(want) != std::wstring::npos) {
			printf("OK\n");
		} else if (!s) {
			printf("FAIL (load failed)\n");
			++failures;
		} else {
			printf("FAIL (expected literal not in the dump)\n");
			++failures;
		}
	}

	// $NAME in an argument means one of two things depending on the parameter
	// it lands on, and the parser cannot tell which - it does not know the
	// signatures.  So it travels as FuncArgDollarName and the generated
	// loadFromCmd() decides.  Before that it was compiled to a plain string,
	// which made $Clipboard the literal text "Clipboard" and made a keyseq
	// argument throw std::bad_variant_access on a thread with no handler.
	{
		printf("[%d] $NAME resolves by parameter type ... ", idx + 25);
		fflush(stdout);

		writeUtf8File(exeDir + L"\\__dollar__.rb",
			L"load \"109.mayu.rb\"\n"
			L"keyseq \"$Foo\", \"A\"\n"
			// a substitution, a literal that merely looks like one, and a
			// keyseq reference - all three spelled into arguments
			L"key[\"C-A-S-O\"] = "
			L"'&ShellExecute(\"open\", $Clipboard,,, ShowNormal)'\n"
			L"key[\"C-A-S-W\"] = '&ClipboardCopy($WindowClassName)'\n"
			L"key[\"C-A-S-L\"] = '&ClipboardCopy(\"Clipboard\")'\n"
			L"key[\"C-A-S-P\"] = '&Repeat($Foo, 3)'\n");

		Symbols syms;
		std::shared_ptr<Setting> s =
			buildSetting(exeDirU8 + "\\__dollar__.rb", syms);
		std::wstring dump = s ? dumpSetting(*s) : std::wstring();

		struct { const wchar_t *want; const char *why; } wants[] = {
			{ L"&ShellExecute(\"open\", $Clipboard,",	"$Clipboard" },
			{ L"&ClipboardCopy($WindowClassName)",		"$WindowClassName" },
			{ L"&ClipboardCopy(\"Clipboard\")",		"quoted literal" },
			{ L"&Repeat(",					"keyseq argument" },
		};
		const char *missing = NULL;
		for (const auto &w : wants)
			if (!missing && dump.find(w.want) == std::wstring::npos)
				missing = w.why;

		if (s && !missing) {
			printf("OK\n");
		} else if (!s) {
			printf("FAIL (load failed)\n");
			++failures;
		} else {
			printf("FAIL (%s not as expected)\n", missing);
			++failures;
		}
	}

	// An unknown $NAME used to leave StrExprArg::m_expr null, so the first
	// eval() dereferenced it.  It has to be reported instead, and the rest of
	// the setting has to survive.
	{
		printf("[%d] unknown $NAME is reported ... ", idx + 26);
		fflush(stdout);

		writeUtf8File(exeDir + L"\\__dollar_bad__.rb",
			L"load \"109.mayu.rb\"\n"
			L"key[\"C-A-S-O\"] = "
			L"'&ShellExecute(\"open\", $Clipbord,,, ShowNormal)'\n"
			L"key[\"C-A-S-P\"] = '&Repeat($NoSuchKeySeq, 3)'\n"
			L"key[\"C-A-S-B\"] = \"&DescribeBindings\"\n");

		Symbols syms;
		std::shared_ptr<Setting> s =
			buildSetting(exeDirU8 + "\\__dollar_bad__.rb", syms);
		std::wstring dump = s ? dumpSetting(*s) : std::wstring();

		// the good assignment survives, the two bad ones are dropped
		if (s && dump.find(L"&DescribeBindings") != std::wstring::npos
				&& dump.find(L"Clipbord") == std::wstring::npos
				&& dump.find(L"NoSuchKeySeq") == std::wstring::npos) {
			printf("OK\n");
		} else if (!s) {
			printf("FAIL (load failed)\n");
			++failures;
		} else {
			printf("FAIL (bad names not dropped, or good one lost)\n");
			++failures;
		}
	}

	int total = (int)(sizeof(combos) / sizeof(combos[0])) + 26;
	printf("\n%s (%d/%d passed)\n",
	       failures == 0 ? "ALL PASSED" : "FAILURES",
	       total - failures, total);
	return failures == 0 ? 0 : 1;
}
