//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// cli_options.h
//
// The command line of nyamy-scripter.exe.
//
// Kept out of main() so that the option set stays one thing: a standalone mode
// added later (syntax check, stream dump) parses the same command line through
// the same code, and a test can call the parser directly.


#ifndef _CLI_OPTIONS_H
#  define _CLI_OPTIONS_H


#  include <string>
#  include <vector>


/// What the scripter was asked to do.  Only Pipe exists today; the enum is here
/// so that adding a mode does not reshape the command line.
enum class ScripterMode {
	Pipe,		///< speak the scripter protocol over the inherited pipes
};


struct CliOptions {
	ScripterMode mode = ScripterMode::Pipe;

	/// -I directories in the order given, already normalized (backslashes, no
	/// trailing separator).  All absolute: a relative -I is a usage error,
	/// since nothing in the scripter resolves against the current directory.
	std::vector<std::string> includeDirs;

	/// -D symbols in the order given.  Added to the symbol set of every setting
	/// load, on top of whatever the Start command carries.
	std::vector<std::string> symbols;

	/// argv index of <script>, or 0 when none was given
	int scriptArgIndex = 0;

	bool showHelp    = false;
	bool showVersion = false;
};


/// Parse argv into *o_options.  Returns false on a usage error, with the reason
/// in *o_error.  A missing <script> is not an error here: -h and --version do
/// not need one, so the caller decides.
bool parseCliOptions(int argc, const char *const *argv,
                     CliOptions *o_options, std::string *o_error);

/// Split a ";"-separated directory list, as -I and NYAMY_LOAD_PATH accept.
/// Empty elements are dropped.
std::vector<std::string> splitDirectoryList(const std::string &i_list);

/// Backslashes, no trailing separator (except on a drive root).
std::string normalizeDirectory(const std::string &i_dir);

/// True for "C:\...", "C:/..." and UNC paths only.  Deliberately stricter than
/// "starts with a separator": a drive-relative "\Lib" depends on the current
/// drive, which is the kind of dependence the scripter has none of elsewhere.
bool isAbsoluteDirectory(const std::string &i_dir);

void printCliUsage(const char *i_argv0);
void printCliVersion(void);


#endif // !_CLI_OPTIONS_H
