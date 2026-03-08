//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// mayu_compiler.h
//
// Compiler: AST -> command stream.
// Resolves conditional blocks, define symbols, and include directives
// at compile time. The output is written to a CmdStreamWriter.


#ifndef _MAYU_COMPILER_H
#  define _MAYU_COMPILER_H

#  include "ast_visitor.h"
#  include "cmd_stream.h"
#  include "cmd_stream_reader.h"
#  include "cmd_stream_writer.h"
#  include "multithread.h"
#  include <set>


class ConfigFiles;
class MayuParser;

using Symbols = std::set<wstringi>;


///
class MayuCompiler : public AstVisitor
{
private:
	Symbols m_symbols;			///< defined symbols
	ConfigFiles &m_configFiles;		///< for include resolution
	SyncObject *m_soLog;
	std::wostream *m_log;
	CmdStreamWriter &m_writer;
	bool m_hasErrors;
	uint32_t m_nextKeySeqIdx;		///< auto-incrementing keySeq index

	/// Modifier default state (inherited through includes)
	CmdModifier m_defaultAssignModifier;
	CmdModifier m_defaultKeySeqModifier;

private:
	// Compilation helpers
	uint32_t compileKeySequence(const AstKeySequence &seq);
	CmdAction compileAction(const AstAction &action);
	CmdModifier compileModifierSpecs(
		const std::vector<AstModifierSpec> &specs);
	CmdFuncArg compileArgument(const AstArgument &arg);
	CmdScanCode compileScanCode(const AstScanCode &sc);

	void error(const AstSourceLoc &loc, const std::wstring &msg);

	// AstVisitor implementation
	void visit(const AstFile &node) override;
	void visit(const AstConditional &node) override;
	void visit(const AstDefineSymbol &node) override;
	void visit(const AstInclude &node) override;
	void visit(const AstDefKey &node) override;
	void visit(const AstDefModifier &node) override;
	void visit(const AstDefSync &node) override;
	void visit(const AstDefAlias &node) override;
	void visit(const AstDefSubstitute &node) override;
	void visit(const AstDefOption &node) override;
	void visit(const AstKeymapDef &node) override;
	void visit(const AstKeyAssign &node) override;
	void visit(const AstEventAssign &node) override;
	void visit(const AstModifierAssign &node) override;
	void visit(const AstKeySeqDef &node) override;

	// Sub-structure visitors (not used directly, but required by interface)
	void visit(const AstKeySequence &node) override;
	void visit(const AstModifierSeq &node) override;
	void visit(const AstActionKey &node) override;
	void visit(const AstActionKeySeqRef &node) override;
	void visit(const AstActionFuncCall &node) override;
	void visit(const AstActionSubSeq &node) override;

public:
	MayuCompiler(CmdStreamWriter &writer,
				 const Symbols &initialSymbols,
				 ConfigFiles &configFiles,
				 SyncObject *soLog = NULL,
				 std::wostream *log = NULL);

	/// Compile an AST file, writing commands to the stream
	void compile(const AstFile &file);

	/// Check if there were any errors
	bool hasErrors() const { return m_hasErrors; }
};


#endif // !_MAYU_COMPILER_H

