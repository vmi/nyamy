//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// nyamy_paths.h


#ifndef _NYAMY_PATHS_H
#  define _NYAMY_PATHS_H


#  include <string>


/// Directory layout of NYamy.
///
///   NYAMY_ROOT    the directory holding nyamy.exe
///   NYAMY_HOME    %LOCALAPPDATA%\NYamy
///   NYAMY_CONFIG  %LOCALAPPDATA%\NYamy\Config
///
/// The three are published to this process' environment, so every child
/// process (the scripter above all) inherits them; the scripter side reads
/// them back through nys_paths_root() / _home() / _config().
///
/// A value already present in the environment is adopted as is, which is what
/// lets a second instance be pointed at a different tree from outside without
/// touching the installation.  Overriding NYAMY_HOME alone moves NYAMY_CONFIG
/// with it, since that one is derived from it.
///
/// Values never carry a trailing separator, so "${NYAMY_ROOT}\file" and
/// root() + L"\\file" are both correct.
class NYamyPaths
{
public:
	/// Resolve the three directories, create NYAMY_HOME and NYAMY_CONFIG, and
	/// publish all three to the environment.  Idempotent; the accessors call
	/// it when it has not run yet, but wWinMain does so explicitly to keep the
	/// directory creation out of whatever happens to look first.
	static void init();

	/// directory of nyamy.exe (NYAMY_ROOT)
	static const std::wstring &root();
	/// per-user data directory (NYAMY_HOME)
	static const std::wstring &home();
	/// per-user configuration directory (NYAMY_CONFIG)
	static const std::wstring &config();
};


#endif // !_NYAMY_PATHS_H
