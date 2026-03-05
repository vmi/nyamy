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

	/// Register a callback invoked each time CmdCommit is received.
	void onCommit(CommitCallback cb);

	/// Read commands from cr in a loop until EOF.
	/// Calls initBuilder() on construction and after each CmdCommit.
	void process(CmdStreamReader &cr);

	// Visitor operators (public: required by std::visit)
	void operator()(CmdKeySequence &);
	void operator()(CmdDefKeyData &);
	void operator()(CmdDefModifierData &);
	void operator()(CmdDefSyncData &);
	void operator()(CmdDefAliasData &);
	void operator()(CmdDefSubstituteData &);
	void operator()(CmdDefOptionData &);
	void operator()(CmdDefSymbolData &);
	void operator()(CmdKeymapDefData &);
	void operator()(CmdKeyAssignData &);
	void operator()(CmdKeyDefaultModData &);
	void operator()(CmdEventAssignData &);
	void operator()(CmdModAssignData &);
	void operator()(CmdKeySeqDefData &);
	void operator()(CmdCommit);

private:
	void initBuilder();
	void error(const std::wstring &msg);
	static bool lookupModifierType(const wstringi &name, Modifier::Type *o_mt);
	static Keymap::AssignMode parseAssignMode(const wstringi &s);

	SyncObject *m_soLog;
	std::wostream *m_log;
	CommitCallback m_commitCallback;
	std::unique_ptr<SettingBuilder> m_builder;
};


#endif // !_CMD_PROCESSOR_H
