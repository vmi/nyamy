//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// nyamy_paths.cpp


#include "misc.h"

#include "nyamy_paths.h"

#include <mutex>


namespace {

std::wstring g_root;
std::wstring g_home;
std::wstring g_config;
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
	g_root = getEnv(L"NYAMY_ROOT");
	if (g_root.empty())
		g_root = moduleDirectory();

	g_home = getEnv(L"NYAMY_HOME");
	if (g_home.empty()) {
		std::wstring localAppData = getEnv(L"LOCALAPPDATA");
		// No LOCALAPPDATA should not happen; fall back to the installation so
		// that nyamy.ini is at least read from somewhere sensible.
		g_home = localAppData.empty() ? g_root : localAppData + L"\\NYamy";
	}

	g_config = getEnv(L"NYAMY_CONFIG");
	if (g_config.empty())
		g_config = (g_home == g_root) ? g_root : g_home + L"\\Config";

	stripTrailingSeparator(&g_root);
	stripTrailingSeparator(&g_home);
	stripTrailingSeparator(&g_config);

	// CreateDirectory only makes the last component, so walk down from home.
	if (g_home != g_root)
		CreateDirectoryW(g_home.c_str(), NULL);
	if (g_config != g_root)
		CreateDirectoryW(g_config.c_str(), NULL);

	SetEnvironmentVariableW(L"NYAMY_ROOT",   g_root.c_str());
	SetEnvironmentVariableW(L"NYAMY_HOME",   g_home.c_str());
	SetEnvironmentVariableW(L"NYAMY_CONFIG", g_config.c_str());
}

} // namespace


void NYamyPaths::init()
{
	std::call_once(g_initOnce, initOnce);
}


const std::wstring &NYamyPaths::root()
{
	init();
	return g_root;
}


const std::wstring &NYamyPaths::home()
{
	init();
	return g_home;
}


const std::wstring &NYamyPaths::config()
{
	init();
	return g_config;
}
