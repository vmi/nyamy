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

	/// When non-null, inline key-sequence literals (parenthesised sub-sequences
	/// used as function arguments) are collected here instead of being written
	/// to m_writer.  compileKeySequence() then returns the sub-sequence's index
	/// within this collector (post-order).  Used by nys_reg_keyseq() so the caller
	/// can register each sub-sequence as a separate keyseq and fix up the indices.
	std::vector<std::vector<CmdAction>> *m_subSeqCollector = nullptr;

	/// Modifier default state (inherited through includes)
	ModifierSpec m_defaultAssignModifier;
	ModifierSpec m_defaultKeySeqModifier;

private:
	// Compilation helpers
	uint32_t compileKeySequence(const AstKeySequence &seq);
	CmdAction compileAction(const AstAction &action);
	FuncArg compileArgument(const AstArgument &arg);

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

	/// Compile an AST file, writing commands to the stream.
	/// initialKeySeqIdx: starting index for RegKeySeq commands (default 0).
	/// writeSymbols: if false, the initial DefSymbol commands for m_symbols
	///              are suppressed (they have already been written by the caller).
	void compile(const AstFile &file,
				 uint32_t initialKeySeqIdx = 0,
				 bool writeSymbols = true);

	/// Compile a key sequence directly to a list of CmdActions without stream I/O.
	/// KeySeqLiteral arguments within function calls are written to the
	/// compiler's associated stream as side effects (same as compile()).
	std::vector<CmdAction> compileActions(const AstKeySequence &seq);

	/// Enable/disable sub-sequence collection mode (see m_subSeqCollector).
	/// Pass nullptr to restore normal stream-writing behaviour.
	void setSubSeqCollector(std::vector<std::vector<CmdAction>> *collector) {
		m_subSeqCollector = collector;
	}

	/// Check if there were any errors
	bool hasErrors() const { return m_hasErrors; }

	/// Return the next keyseq index that would be assigned.
	/// After compile(), this equals initialKeySeqIdx + number of keyseqs emitted.
	uint32_t nextKeySeqIdx() const { return m_nextKeySeqIdx; }

	/// Resolve modifier specifiers to a bitmask.
	/// Pure function: depends only on the static modifier-name table.
	static ModifierSpec compileModifierSpecs(
		const std::vector<AstModifierSpec> &specs);

	/// Convert a parsed scan code to its binary representation.
	/// Pure function: no member state required.
	static CmdScanCode compileScanCode(const AstScanCode &sc);
};


#endif // !_MAYU_COMPILER_H

