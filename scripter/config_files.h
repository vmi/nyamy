//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// config_files.h


#ifndef _CONFIG_FILES_H
#  define _CONFIG_FILES_H


#  include "../multithread.h"
#  include "../stringtool.h"
#  include "../symbols.h"
#  include <list>
#  include <functional>


/// home directory path list
using HomeDirectories = std::list<tstringi>;


/// file system operations for configuration files
class ConfigFiles
{
public:
	/// callback invoked when no config file is found. return true to retry
	using RetryCallback = std::function<bool()>;

public:
	ConfigFiles(SyncObject *i_soLog = nullptr, tostream *i_log = nullptr);

	/// get home directory path
	void getHomeDirectories(HomeDirectories *o_path) const;

	/// get mayu filename from registry
	bool getFilenameFromRegistry(tstringi *o_name, tstringi *o_filename,
								 Symbols *o_symbols) const;

	/// read file contents
	bool readFile(tstring *o_data, const tstringi &i_filename) const;

	/// is the filename readable ?
	bool isReadable(const tstringi &i_filename, int i_debugLevel = 1) const;

	/// get filename
	bool getFilename(const tstringi &i_name, tstringi *o_path,
					 Symbols *o_symbols,
					 RetryCallback i_retry = nullptr,
					 int i_debugLevel = 1) const;

private:
	SyncObject *m_soLog;
	tostream *m_log;
};


#endif // !_CONFIG_FILES_H
