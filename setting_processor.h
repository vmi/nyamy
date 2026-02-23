//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// setting_processor.h


#ifndef _SETTING_PROCESSOR_H
#  define _SETTING_PROCESSOR_H


#  include "symbols.h"
#  include "multithread.h"
#  include "setting.h"
#  include <iostream>
#  include <memory>


/// Interprets a command stream (binary) and produces a Setting
class SettingProcessor
{
public:
	SettingProcessor(SyncObject *i_soLog, tostream *i_log);

	/// Stream-only: interpret a pre-compiled command stream
	std::unique_ptr<Setting> process(std::istream &in,
									 const Symbols &i_initialSymbols);

private:
	SyncObject *m_soLog;
	tostream *m_log;
};


#endif // !_SETTING_PROCESSOR_H
