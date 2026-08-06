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

#include <windows.h>
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
	// by the build.  Deliberately move the cwd away from it: relative .rb
	// loads must resolve via $LOAD_PATH (script directory + home
	// directories), never via the current directory.
	wchar_t exePath[MAX_PATH] = {};
	GetModuleFileNameW(nullptr, exePath, MAX_PATH);
	std::wstring exeDir(exePath);
	size_t slash = exeDir.find_last_of(L"\\/");
	if (slash != std::wstring::npos) exeDir.resize(slash);
	size_t parentSlash = exeDir.find_last_of(L"\\/");
	if (parentSlash != std::wstring::npos)
		SetCurrentDirectoryW(exeDir.substr(0, parentSlash).c_str());

	std::string exeDirU8 = wideToUtf8(exeDir);
	std::string rbScript    = exeDirU8 + "\\dot.mayu.rb";
	std::string viaMayuPath = exeDirU8 + "\\__via_mayu__.rb";
	std::string wkRbPath    = exeDirU8 + "\\__wk_via_rb__.rb";
	std::string wkMayuPath  = exeDirU8 + "\\__wk_via_mayu__.rb";

	// Generate the loader scripts.  workaround.* is not included from dot.*,
	// so chain it behind the dot configuration.  The .mayu reference uses a
	// wrapper .mayu (single compile) because `define`d symbols do not
	// propagate between two separate `load "*.mayu"` calls.
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

	// Default-script probe: with no script argument the scripter searches
	// the home directories for ".mayu.rb".  Point USERPROFILE / LOCALAPPDATA
	// at scratch directories so the probe is hermetic, and verify that the
	// probed script's own directory ends up in $LOAD_PATH.
	{
		printf("[%d] .mayu.rb home directory probe ... ", idx + 3);
		fflush(stdout);

		std::wstring homeDir = exeDir + L"\\__home__";
		std::wstring cfgDir  = homeDir + L"\\.config\\nyamy";
		CreateDirectoryW(homeDir.c_str(), nullptr);
		CreateDirectoryW((homeDir + L"\\.config").c_str(), nullptr);
		CreateDirectoryW(cfgDir.c_str(), nullptr);
		writeUtf8File(cfgDir + L"\\__probe_lib__.rb", L"$probe_loaded = true\n");
		writeUtf8File(cfgDir + L"\\.mayu.rb",
			L"load \"__probe_lib__.rb\"\n"
			L"raise \"script-dir load failed\" unless $probe_loaded\n");

		wchar_t oldProf[MAX_PATH] = {}, oldLocal[MAX_PATH] = {};
		GetEnvironmentVariableW(L"USERPROFILE", oldProf, MAX_PATH);
		GetEnvironmentVariableW(L"LOCALAPPDATA", oldLocal, MAX_PATH);
		SetEnvironmentVariableW(L"USERPROFILE", homeDir.c_str());
		SetEnvironmentVariableW(L"LOCALAPPDATA", (homeDir + L"\\AppData\\Local").c_str());

		Symbols syms;
		std::shared_ptr<Setting> s = buildSetting(std::string(), syms);

		SetEnvironmentVariableW(L"USERPROFILE", oldProf);
		SetEnvironmentVariableW(L"LOCALAPPDATA", oldLocal);

		if (s) {
			printf("OK\n");
		} else {
			printf("FAIL (probe failed or no commit)\n");
			++failures;
		}
	}

	// parseScancodeMapBlob: exercise the registry-blob parser directly with
	// hand-built blobs (independent of the machine's actual Scancode Map).
	{
		printf("[%d] parseScancodeMapBlob ... ", idx + 4);
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
		printf("[%d] sc() / ScancodeMap DSL ... ", idx + 5);
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
			L"raise \"range err\"     unless expect_arg_error { sc(0x10000) }\n"
			L"raise \"unknown name\"  unless expect_arg_error { sc(\"NoSuchKeyXYZ\") }\n"
			L"raise \"to is array\"   unless ScancodeMap.to(0x1d).is_a?(Array)\n"
			L"m = ScancodeMap[0x3a]\n"
			L"unless m.nil?\n"
			L"  raise \"map/to consistency\" unless ScancodeMap.to(m).include?(0x3a)\n"
			L"end\n");

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
		printf("[%d] modifier defaults ... ", idx + 6);
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

	// `key ⟨MODIFIER⟩ = ⟨MODIFIER⟩' changed the default modifiers of every line
	// below it.  The statement is gone and must be rejected, not read as an
	// assignment to a key named "=".
	{
		printf("[%d] default modifier statement rejected ... ", idx + 7);
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
		printf("[%d] out-of-context modifier rejected ... ", idx + 8);
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

	int total = (int)(sizeof(combos) / sizeof(combos[0])) + 8;
	printf("\n%s (%d/%d passed)\n",
	       failures == 0 ? "ALL PASSED" : "FAILURES",
	       total - failures, total);
	return failures == 0 ? 0 : 1;
}
