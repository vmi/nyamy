//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// test_main.cpp
//
// Verifies that loading dot.mayu (native .mayu compile pipeline) and
// dot.mayu.rb (mruby DSL pipeline) produce identical Settings for several
// USE* symbol combinations.
//
// The .mayu reference path reuses the mruby runtime via a tiny generated
// loader script (__via_mayu__.rb -> `load "dot.mayu"`), so both paths run
// through the same ys_start() / flushQueue() / CmdProcessor plumbing and
// differ only in the script that is executed.

#include "misc.h"

#include "test_harness.h"
#include "setting_dump.h"
#include "setting.h"
#include "symbols.h"

#include <windows.h>
#include <cstdio>
#include <fstream>
#include <string>
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
	// Resolve our own directory; config / script files are copied next to us by
	// the build, and .rb `load` resolves relative to the current directory.
	wchar_t exePath[MAX_PATH] = {};
	GetModuleFileNameW(nullptr, exePath, MAX_PATH);
	std::wstring exeDir(exePath);
	size_t slash = exeDir.find_last_of(L"\\/");
	if (slash != std::wstring::npos) exeDir.resize(slash);
	SetCurrentDirectoryW(exeDir.c_str());

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

	printf("\n%s (%d/%d passed)\n",
	       failures == 0 ? "ALL PASSED" : "FAILURES",
	       (int)(sizeof(combos) / sizeof(combos[0])) - failures,
	       (int)(sizeof(combos) / sizeof(combos[0])));
	return failures == 0 ? 0 : 1;
}
