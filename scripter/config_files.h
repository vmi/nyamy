//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// config_files.h


#ifndef _CONFIG_FILES_H
#  define _CONFIG_FILES_H


#  include "../multithread.h"
#  include "../stringtool.h"
#  include "../symbols.h"
#  include <list>
#  include <functional>


/// config file search path
using SearchDirectories = std::list<wstringi>;


/// file system operations for configuration files
class ConfigFiles
{
public:
	/// callback invoked when no config file is found. return true to retry
	using RetryCallback = std::function<bool()>;

public:
	ConfigFiles(SyncObject *i_soLog = nullptr, std::wostream *i_log = nullptr);

	/// get the config file search path: NYAMY_CONFIG, then NYAMY_ROOT.
	/// The current directory is deliberately not part of it, and neither is
	/// "<NYAMY_HOME>\Lib", which carries .rb libraries for $LOAD_PATH only.
	void getSearchDirectories(SearchDirectories *o_path) const;

	/// read file contents
	bool readFile(std::wstring *o_data, const wstringi &i_filename) const;

	/// is the filename readable ?
	bool isReadable(const wstringi &i_filename,
					LogLevel i_level = LogLevel::Debug) const;

	/// get filename.  An absolute i_name is only checked for readability;
	/// a relative one is searched in the search path
	bool getFilename(const wstringi &i_name, wstringi *o_path,
					 RetryCallback i_retry = nullptr,
					 LogLevel i_level = LogLevel::Debug) const;

private:
	SyncObject *m_soLog;
	std::wostream *m_log;
};


#endif // !_CONFIG_FILES_H
