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
using HomeDirectories = std::list<wstringi>;


/// file system operations for configuration files
class ConfigFiles
{
public:
	/// callback invoked when no config file is found. return true to retry
	using RetryCallback = std::function<bool()>;

public:
	ConfigFiles(SyncObject *i_soLog = nullptr, std::wostream *i_log = nullptr);

	/// get home directory path
	void getHomeDirectories(HomeDirectories *o_path) const;

	/// get mayu filename from registry
	bool getFilenameFromRegistry(wstringi *o_name, wstringi *o_filename,
								 Symbols *o_symbols) const;

	/// read file contents
	bool readFile(std::wstring *o_data, const wstringi &i_filename) const;

	/// is the filename readable ?
	bool isReadable(const wstringi &i_filename, int i_debugLevel = 1) const;

	/// get filename
	bool getFilename(const wstringi &i_name, wstringi *o_path,
					 Symbols *o_symbols,
					 RetryCallback i_retry = nullptr,
					 int i_debugLevel = 1) const;

private:
	SyncObject *m_soLog;
	std::wostream *m_log;
};


#endif // !_CONFIG_FILES_H
