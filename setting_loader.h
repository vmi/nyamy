//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// setting_loader.h


#ifndef _SETTING_LOADER_H
#  define _SETTING_LOADER_H


#  include "config_files.h"
#  include "lexer.h"
#  include "multithread.h"
#  include "setting_builder.h"
#  include <memory>


class Setting;


///
class SettingLoader
{
#  define FUNCTION_FRIEND
#  include "functions.h"
#  undef FUNCTION_FRIEND

public:
	///
	class FunctionCreator
	{
	public:
		const _TCHAR *m_name;			///
		FunctionData *m_creator;			///
	};

private:
	using Tokens = std::vector<Token>;		///
	using Prefixes = std::vector<tstringi>;	///
	using CanReadStack = std::vector<bool>;	///

private:
	SettingBuilder *m_builder;			/// setting builder
	bool m_isThereAnyError;			/// is there any error ?

	SyncObject *m_soLog;				/// guard log output stream
	tostream *m_log;				/// log output stream

	ConfigFiles m_configFiles;			/// file system operations

	tstringi m_currentFilename;			/// current filename

	Tokens m_tokens;				/// tokens for current line
	Tokens::iterator m_ti;			/// current processing token

	static std::shared_ptr<Prefixes> m_prefixes;	/// prefix terminal symbol

	Keymap *m_currentKeymap;			/// current keymap

	CanReadStack m_canReadStack;			/// for <COND_SYMBOL>

	Modifier m_defaultAssignModifier;		/** default
                                                    <ASSIGN_MODIFIER> */
	Modifier m_defaultKeySeqModifier;		/** default
                                                    <KEYSEQ_MODIFIER> */

private:
	bool isEOL();					/// is there no more tokens ?
	Token *getToken();				/// get next token
	Token *lookToken();				/// look next token
	bool getOpenParen(bool i_doesThrow, const _TCHAR *i_name); /// argument "("
	bool getCloseParen(bool i_doesThrow, const _TCHAR *i_name); /// argument ")"
	bool getComma(bool i_doesThrow, const _TCHAR *i_name); /// argument ","

	void load_LINE();				/// <LINE>
	void load_DEFINE();				/// <DEFINE>
	void load_IF();				/// <IF>
	void load_ELSE(bool i_isElseIf, const tstringi &i_token);
	/// <ELSE> <ELSEIF>
	bool load_ENDIF(const tstringi &i_token);	/// <ENDIF>
	void load_INCLUDE();				/// <INCLUDE>
	void load_SCAN_CODES(Key *o_key);		/// <SCAN_CODES>
	void load_DEFINE_KEY();			/// <DEFINE_KEY>
	void load_DEFINE_MODIFIER();			/// <DEFINE_MODIFIER>
	void load_DEFINE_SYNC_KEY();			/// <DEFINE_SYNC_KEY>
	void load_DEFINE_ALIAS();			/// <DEFINE_ALIAS>
	void load_DEFINE_SUBSTITUTE();		/// <DEFINE_SUBSTITUTE>
	void load_DEFINE_OPTION();			/// <DEFINE_OPTION>
	void load_KEYBOARD_DEFINITION();		/// <KEYBOARD_DEFINITION>
	Modifier load_MODIFIER(Modifier::Type i_mode, Modifier i_modifier,
						   Modifier::Type *o_mode = NULL);
	/// <..._MODIFIER>
	Key *load_KEY_NAME();				/// <KEY_NAME>
	void load_KEYMAP_DEFINITION(const Token *i_which);
	/// <KEYMAP_DEFINITION>
	void load_ARGUMENT(bool *o_arg);		/// <ARGUMENT>
	void load_ARGUMENT(int *o_arg);		/// <ARGUMENT>
	void load_ARGUMENT(unsigned int *o_arg);	/// <ARGUMENT>
	void load_ARGUMENT(long *o_arg);		/// <ARGUMENT>
	void load_ARGUMENT(unsigned __int64 *o_arg);	/// <ARGUMENT>
	void load_ARGUMENT(__int64 *o_arg);		/// <ARGUMENT>
	void load_ARGUMENT(tstringq *o_arg);		/// <ARGUMENT>
	void load_ARGUMENT(std::list<tstringq> *o_arg); /// <ARGUMENT>
	void load_ARGUMENT(tregex *o_arg);		/// <ARGUMENT>
	void load_ARGUMENT(VKey *o_arg);		/// <ARGUMENT_VK>
	void load_ARGUMENT(ToWindowType *o_arg);	/// <ARGUMENT_WINDOW>
	void load_ARGUMENT(GravityType *o_arg);	/// <ARGUMENT>
	void load_ARGUMENT(MouseHookType *o_arg);	/// <ARGUMENT>
	void load_ARGUMENT(MayuDialogType *o_arg);	/// <ARGUMENT>
	void load_ARGUMENT(ModifierLockType *o_arg);	/// <ARGUMENT_LOCK>
	void load_ARGUMENT(ToggleType *o_arg);	/// <ARGUMENT>
	void load_ARGUMENT(ShowCommandType *o_arg);	///<ARGUMENT_SHOW_WINDOW>
	void load_ARGUMENT(TargetWindowType *o_arg);
	/// <ARGUMENT_TARGET_WINDOW_TYPE>
	void load_ARGUMENT(BooleanType *o_arg);	/// <bool>
	void load_ARGUMENT(LogicalOperatorType *o_arg);/// <ARGUMENT>
	void load_ARGUMENT(Modifier *o_arg);		/// <ARGUMENT>
	void load_ARGUMENT(const Keymap **o_arg);	/// <ARGUMENT>
	void load_ARGUMENT(const KeySeq **o_arg);	/// <ARGUMENT>
	void load_ARGUMENT(StrExprArg *o_arg);	/// <ARGUMENT>
	void load_ARGUMENT(WindowMonitorFromType *o_arg);	/// <ARGUMENT>
	KeySeq *load_KEY_SEQUENCE(
		const tstringi &i_name = _T(""), bool i_isInParen = false,
		Modifier::Type i_mode = Modifier::Type_KEYSEQ); /// <KEY_SEQUENCE>
	void load_KEY_ASSIGN();			/// <KEY_ASSIGN>
	void load_EVENT_ASSIGN();			/// <EVENT_ASSIGN>
	void load_MODIFIER_ASSIGNMENT();		/// <MODIFIER_ASSIGN>
	void load_LOCK_ASSIGNMENT();			/// <LOCK_ASSIGN>
	void load_KEYSEQ_DEFINITION();		/// <KEYSEQ_DEFINITION>

	/// load
	void load(const tstringi &i_filename);

public:
	///
	SettingLoader(SyncObject *i_soLog, tostream *i_log);

	/// load setting (internal, called from load_INCLUDE)
	bool load(SettingBuilder *i_builder, const tstringi &i_filename);

	/// load setting
	std::unique_ptr<Setting> load(const tstringi &i_filename,
								  const Symbols &i_initialSymbols);

	/// load setting from a command stream (binary)
	std::unique_ptr<Setting> loadFromStream(std::istream &in,
											const Symbols &i_initialSymbols);
};


#endif // !_SETTING_LOADER_H
