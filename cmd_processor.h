//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// cmd_processor.h


#ifndef _CMD_PROCESSOR_H
#  define _CMD_PROCESSOR_H

#  include "setting_builder.h"
#  include "cmd_stream_reader.h"
#  include "multithread.h"
#  include <functional>
#  include <memory>


class CmdProcessor
{
public:
	using CommitCallback = std::function<void(std::unique_ptr<Setting>)>;

	CmdProcessor(SyncObject *soLog, std::wostream *log);

	/// Register a callback invoked each time CmdArgsCommit is received.
	void onCommit(CommitCallback cb);

	/// Read commands from cr in a loop until EOF.
	void process(CmdStreamReader &cr);

	// Visitor operators (public: required by std::visit)
	void operator()(CmdArgsRegKeySeq &);
	void operator()(CmdArgsDefKey &);
	void operator()(CmdArgsDefMod &);
	void operator()(CmdArgsDefSync &);
	void operator()(CmdArgsDefAlias &);
	void operator()(CmdArgsDefSubst &);
	void operator()(CmdArgsDefOption &);
	void operator()(CmdArgsDefSymbol &);
	void operator()(CmdArgsBeginKeymap &);
	void operator()(CmdArgsAssignKey &);
	void operator()(CmdArgsAssignEvent &);
	void operator()(CmdArgsAssignMod &);
	void operator()(CmdArgsCommit);

private:
	void error(const std::wstring &msg);
	static bool lookupModifierType(const wstringi &name, Modifier::Type *o_mt);
	static Keymap::AssignMode parseAssignMode(const wstringi &s);

	SyncObject *m_soLog;
	std::wostream *m_log;
	CommitCallback m_commitCallback;
	std::unique_ptr<SettingBuilder> m_builder;
};


#endif // !_CMD_PROCESSOR_H
