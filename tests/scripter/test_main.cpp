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
	std::vector<const wchar_t *> symbols;
};

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

	// Generate the .mayu loader script.
	writeUtf8File(exeDir + L"\\__via_mayu__.rb", L"load \"dot.mayu\"\n");

	const Combo combos[] = {
		{ "USE104",                                   { L"USE104" } },
		{ "USE104 + USE109on104",                     { L"USE104", L"USE109on104" } },
		{ "USE109",                                   { L"USE109" } },
		{ "USE109 + USE104on109",                     { L"USE109", L"USE104on109" } },
		{ "USE109 + USE104on109 + USEdefault",        { L"USE109", L"USE104on109", L"USEdefault" } },
	};

	int failures = 0;
	int idx = 0;
	for (const Combo &combo : combos) {
		++idx;
		Symbols syms;
		for (const wchar_t *s : combo.symbols) syms.insert(wstringi(s));

		printf("[%d] %s ... ", idx, combo.name);
		fflush(stdout);

		std::shared_ptr<Setting> a = buildSetting(viaMayuPath, syms); // .mayu
		std::shared_ptr<Setting> b = buildSetting(rbScript, syms);    // .mayu.rb

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

	int total = (int)(sizeof(combos) / sizeof(combos[0])) + 5;
	printf("\n%s (%d/%d passed)\n",
	       failures == 0 ? "ALL PASSED" : "FAILURES",
	       total - failures, total);
	return failures == 0 ? 0 : 1;
}
