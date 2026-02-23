//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// mayu_parser.h
//
// Recursive descent parser for .mayu configuration files.
// Produces an AST (see ast.h) from the token stream produced by Lexer.


#ifndef _MAYU_PARSER_H
#  define _MAYU_PARSER_H

#  include "ast.h"
#  include "lexer.h"
#  include <memory>
#  include <vector>


class ConfigFiles;


///
class MayuParser
{
public:
	using Prefixes = std::vector<tstringi>;

private:
	Lexer *m_lexer;				///< current lexer
	Lexer::Tokens m_tokens;			///< tokens for current line
	Lexer::Tokens::iterator m_ti;		///< current token iterator

	tstringi m_filename;			///< current filename
	size_t m_lineNumber;			///< current line number

	bool m_hasErrors;			///< any errors occurred?
	std::vector<tstring> m_messages;	///< error/warning messages

	static std::shared_ptr<Prefixes> s_prefixes;	///< shared prefix table

private:
	// Token access
	bool nextLine();
	bool isEOL() const;
	Token *getToken();
	Token *lookToken();

	// Location
	AstSourceLoc currentLoc() const;

	// Error reporting
	void error(const tstring &msg);
	void warning(const tstring &msg);

	// Prefix initialization
	static void initPrefixes();

	// Modifier parsing helpers
	bool isModifierToken(const Token *t) const;
	bool isModifierTokenName(const tstringi &name) const;
	bool isAssignModifierToken(const Token *t) const;
	std::vector<AstModifierSpec> parseModifierSpecs();
	std::vector<AstModifierSpec> tokensToModifierSpecs(
		const std::vector<tstringi> &tokens) const;

	// Scan code parsing
	std::vector<AstScanCode> parseScanCodes();

	// Recursive descent parsing methods
	AstNodePtr parseLine();

	// Conditional blocks
	std::unique_ptr<AstConditional> parseConditional(bool isAnd);

	// Top-level directives
	std::unique_ptr<AstDefineSymbol> parseDefine();
	std::unique_ptr<AstInclude> parseInclude();

	// Keyboard definitions (def ...)
	AstNodePtr parseKeyboardDefinition();
	std::unique_ptr<AstDefKey> parseDefKey();
	std::unique_ptr<AstDefModifier> parseDefModifier();
	std::unique_ptr<AstDefSync> parseDefSync();
	std::unique_ptr<AstDefAlias> parseDefAlias();
	std::unique_ptr<AstDefSubstitute> parseDefSubstitute();
	std::unique_ptr<AstDefOption> parseDefOption();

	// Keymap definition
	std::unique_ptr<AstKeymapDef> parseKeymapDefinition(
		const tstringi &keyword);

	// Key and event assignments
	AstNodePtr parseKeyAssignOrDefaultModifier();
	std::unique_ptr<AstEventAssign> parseEventAssign();

	// Modifier assignment
	std::unique_ptr<AstModifierAssign> parseModifierAssign();

	// Keyseq definition
	std::unique_ptr<AstKeySeqDef> parseKeySeqDefinition();

	// Key sequence (shared sub-structure)
	std::unique_ptr<AstKeySequence> parseKeySequence(
		bool inParen = false);

	// Function arguments
	std::unique_ptr<AstArgument> parseArgument();
	std::vector<std::unique_ptr<AstArgument>> parseArguments();

public:
	MayuParser();

	/// Parse from a buffer
	std::unique_ptr<AstFile> parseBuffer(
		const _TCHAR *buffer, size_t length,
		const tstringi &filename);

	/// Parse from a file (reads via ConfigFiles)
	std::unique_ptr<AstFile> parseFile(
		const tstringi &filename,
		ConfigFiles &configFiles);

	/// Check if there were any errors
	bool hasErrors() const { return m_hasErrors; }

	/// Get collected error/warning messages
	const std::vector<tstring> &getMessages() const {
		return m_messages;
	}
};


#endif // !_MAYU_PARSER_H
