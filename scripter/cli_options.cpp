//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// cli_options.cpp


#include "cli_options.h"
#include "nyamy_scripter.h"

#include <cctype>
#include <cstdio>
#include <cstring>


//=============================================================================
// Path helpers
//=============================================================================

std::vector<std::string> splitDirectoryList(const std::string &i_list)
{
	std::vector<std::string> out;
	size_t begin = 0;
	for (;;) {
		size_t sep = i_list.find(';', begin);
		std::string part = (sep == std::string::npos)
			? i_list.substr(begin)
			: i_list.substr(begin, sep - begin);
		if (!part.empty())
			out.push_back(part);
		if (sep == std::string::npos)
			break;
		begin = sep + 1;
	}
	return out;
}


std::string normalizeDirectory(const std::string &i_dir)
{
	std::string s = i_dir;
	for (char &c : s)
		if (c == '/') c = '\\';
	// A drive root has to keep its separator: "C:" alone means "wherever the
	// current directory on C: happens to be", which is not what was written.
	while (1 < s.size() && s.back() == '\\' &&
	       s[s.size() - 2] != ':')
		s.pop_back();
	return s;
}


bool isAbsoluteDirectory(const std::string &i_dir)
{
	// "C:\..." or "C:/..."
	if (3 <= i_dir.size() && isalpha((unsigned char)i_dir[0]) &&
	    i_dir[1] == ':' && (i_dir[2] == '\\' || i_dir[2] == '/'))
		return true;
	// UNC: "\\server\share"
	if (2 <= i_dir.size() &&
	    (i_dir[0] == '\\' || i_dir[0] == '/') &&
	    (i_dir[1] == '\\' || i_dir[1] == '/'))
		return true;
	return false;
}


//=============================================================================
// Usage
//=============================================================================

void printCliUsage(const char *i_argv0)
{
	fprintf(stderr,
		"usage: %s [options] <script> [args...]\n"
		"\n"
		"  script       configuration script (.rb, or .mayu to compile).  A\n"
		"               relative path is searched in NYAMY_CONFIG then\n"
		"               NYAMY_ROOT; the current directory is not searched.\n"
		"  args         passed to the script as ARGV\n"
		"\n"
		"  -I <dir>     add <dir> to $LOAD_PATH.  Repeatable, and several\n"
		"               directories may be given at once separated by ';'.\n"
		"               The path must be absolute.\n"
		"  -D <symbol>  define <symbol>, as if the configuration had run\n"
		"               `define <symbol>'.  Repeatable.\n"
		"  --           stop reading options; the next argument is the script\n"
		"  -h, --help   show this message\n"
		"  --version    show the version\n"
		"\n"
		"NYAMY_LOAD_PATH adds to $LOAD_PATH as well (';'-separated, absolute).\n"
		"\n"
		"NYAMY_ROOT=%s\n"
		"NYAMY_HOME=%s\n"
		"NYAMY_CONFIG=%s\n",
		i_argv0, nys_paths_root(), nys_paths_home(), nys_paths_config());
}


void printCliVersion(void)
{
	fprintf(stderr, "nyamy-scripter %s\n", VERSION);
}


//=============================================================================
// Parser
//=============================================================================

// Value of an option that takes one: either glued to the flag ("-Idir") or the
// next argument ("-I dir").  Advances *io_i past a consumed argument.
static bool optionValue(int argc, const char *const *argv, int *io_i,
                        size_t i_flagLen, std::string *o_value,
                        std::string *o_error)
{
	const char *arg = argv[*io_i];
	if (arg[i_flagLen] != '\0') {
		*o_value = arg + i_flagLen;
		return true;
	}
	if (argc <= *io_i + 1) {
		*o_error = std::string("option ") + arg + " requires an argument";
		return false;
	}
	++*io_i;
	*o_value = argv[*io_i];
	return true;
}


bool parseCliOptions(int argc, const char *const *argv,
                     CliOptions *o_options, std::string *o_error)
{
	CliOptions opts;
	std::string error;

	int i = 1;
	for (; i < argc; ++i) {
		const char *arg = argv[i];

		if (std::strcmp(arg, "--") == 0) {
			++i;
			break;
		}
		// Everything that is not an option ends the option list.  A lone "-"
		// is a file name as far as we are concerned, not a flag.
		if (arg[0] != '-' || arg[1] == '\0')
			break;

		if (std::strcmp(arg, "-h") == 0 || std::strcmp(arg, "--help") == 0) {
			opts.showHelp = true;
			continue;
		}
		if (std::strcmp(arg, "--version") == 0) {
			opts.showVersion = true;
			continue;
		}
		if (arg[1] == 'I') {
			std::string value;
			if (!optionValue(argc, argv, &i, 2, &value, &error)) {
				if (o_error) *o_error = error;
				return false;
			}
			for (const auto &dir : splitDirectoryList(value)) {
				std::string norm = normalizeDirectory(dir);
				// A relative -I would have to be resolved against something,
				// and the only candidate is the current directory, which the
				// scripter never consults.  Refuse rather than invent a base.
				if (!isAbsoluteDirectory(norm)) {
					if (o_error)
						*o_error = "-I requires an absolute path: " + dir;
					return false;
				}
				opts.includeDirs.push_back(norm);
			}
			continue;
		}
		if (arg[1] == 'D') {
			std::string value;
			if (!optionValue(argc, argv, &i, 2, &value, &error)) {
				if (o_error) *o_error = error;
				return false;
			}
			if (value.empty()) {
				if (o_error) *o_error = "-D requires a symbol name";
				return false;
			}
			opts.symbols.push_back(value);
			continue;
		}

		if (o_error) *o_error = std::string("unknown option: ") + arg;
		return false;
	}

	if (i < argc)
		opts.scriptArgIndex = i;

	*o_options = std::move(opts);
	return true;
}
