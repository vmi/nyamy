//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// cmd_processor.h


#ifndef _CMD_PROCESSOR_H
#  define _CMD_PROCESSOR_H

#  include "adhoc_keyseq.h"
#  include "setting_builder.h"
#  include "cmd_stream_reader.h"
#  include "multithread.h"
#  include <functional>
#  include <memory>
#  include <utility>
#  include <vector>


class CmdProcessor
{
public:
	// onCommit: passes std::shared_ptr<Setting>
	using CommitCallback = std::function<void(std::shared_ptr<Setting>)>;
	void onCommit(CommitCallback cb);

	// onExecKeySeq: passes AdHocKeySeq
	using ExecKeySeqCallback = std::function<void(AdHocKeySeq)>;
	void onExecKeySeq(ExecKeySeqCallback cb);

	CmdProcessor(SyncObject *soLog, std::wostream *log);

	/// Read commands from cr in a loop until EOF.
	void process(CmdStreamReader &cr);

	// Visitor operators (public: required by std::visit)
	void operator()(CmdArgsReset);
	void operator()(CmdArgsRegKeySeq &);
	void operator()(CmdArgsExecKeySeq &);
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
	void warning(const std::wstring &msg);
	void beginSetting();
	void applyDefSubst(const CmdArgsDefSubst &data);
	static bool lookupModifierType(const wstringi &name, Modifier::Type *o_mt);
	static Keymap::AssignMode parseAssignMode(const wstringi &s);

	SyncObject *m_soLog;
	std::wostream *m_log;

	// Commands whose resolution is deferred until Commit: keyseq actions may
	// reference keys / keymaps / keyseqs defined later in the stream, and
	// substitutes read the contents of a materialized keyseq.
	std::vector<std::pair<KeySeq *, CmdArgsRegKeySeq>> m_pendingKeySeqs;
	std::vector<CmdArgsDefSubst>                       m_pendingSubsts;

	std::shared_ptr<Setting>        m_setting;   ///< Setting being built (Reset .. Commit)
	std::shared_ptr<Setting>        m_committed; ///< last committed Setting (used by ExecKeySeq)
	std::unique_ptr<SettingBuilder> m_builder;   ///< valid between Reset and Commit

	CommitCallback     m_commitCallback;
	ExecKeySeqCallback m_execKeySeqCallback;
};


#endif // !_CMD_PROCESSOR_H
