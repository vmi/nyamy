//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// nys_paths.cpp
//
// Directory layout as seen from the scripter side.  nyamy publishes
// NYAMY_ROOT / NYAMY_HOME / NYAMY_CONFIG to its environment, which every child
// process inherits; the values are read back here.
//
// The fallbacks matter when the scripter runs without nyamy above it (started
// from a shell, or the in-process test harness): the layout is then derived
// the same way nyamy would derive it.


#include "../misc.h"

#define _NYAMY_SCRIPTER_IMPL	// NYS_API = dllexport (we are inside the DLL)
#include "nyamy_scripter.h"
#include "../stringtool.h"

#include <mutex>
#include <string>


namespace {

std::string g_root;
std::string g_home;
std::string g_config;
std::once_flag g_initOnce;


// read an environment variable; returns an empty string when it is unset
std::wstring getEnv(const wchar_t *i_name)
{
	wchar_t buf[GANA_MAX_PATH];
	DWORD len = GetEnvironmentVariableW(i_name, buf, NUMBER_OF(buf));
	if (len == 0 || NUMBER_OF(buf) <= len)
		return std::wstring();
	return std::wstring(buf, len);
}


// drop trailing separators, except from a drive root ("C:\")
void stripTrailingSeparator(std::wstring *io_path)
{
	while (1 < io_path->size() &&
			(io_path->back() == L'\\' || io_path->back() == L'/') &&
			(*io_path)[io_path->size() - 2] != L':')
		io_path->pop_back();
}


// directory part of this process' executable, without a trailing separator
std::wstring moduleDirectory()
{
	wchar_t buf[GANA_MAX_PATH];
	DWORD len = GetModuleFileNameW(NULL, buf, NUMBER_OF(buf));
	if (len == 0 || NUMBER_OF(buf) <= len)
		return std::wstring();
	std::wstring path(buf, len);
	size_t sep = path.find_last_of(L"\\/");
	if (sep == std::wstring::npos)
		return std::wstring();
	path.resize(sep);
	stripTrailingSeparator(&path);
	return path;
}


void initOnce()
{
	std::wstring root = getEnv(L"NYAMY_ROOT");
	if (root.empty())
		root = moduleDirectory();

	std::wstring home = getEnv(L"NYAMY_HOME");
	if (home.empty()) {
		std::wstring localAppData = getEnv(L"LOCALAPPDATA");
		home = localAppData.empty() ? root : localAppData + L"\\NYamy";
	}

	std::wstring config = getEnv(L"NYAMY_CONFIG");
	if (config.empty())
		config = (home == root) ? root : home + L"\\Config";

	stripTrailingSeparator(&root);
	stripTrailingSeparator(&home);
	stripTrailingSeparator(&config);

	g_root   = to_UTF8(root);
	g_home   = to_UTF8(home);
	g_config = to_UTF8(config);
}

} // namespace


NYS_API const char* nys_paths_root(void)
{
	std::call_once(g_initOnce, initOnce);
	return g_root.c_str();
}


NYS_API const char* nys_paths_home(void)
{
	std::call_once(g_initOnce, initOnce);
	return g_home.c_str();
}


NYS_API const char* nys_paths_config(void)
{
	std::call_once(g_initOnce, initOnce);
	return g_config.c_str();
}
