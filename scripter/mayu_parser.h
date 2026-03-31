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
	using Prefixes = std::vector<wstringi>;

private:
	Lexer *m_lexer;				///< current lexer
	Lexer::Tokens m_tokens;			///< tokens for current line
	Lexer::Tokens::iterator m_ti;		///< current token iterator

	wstringi m_filename;			///< current filename
	size_t m_lineNumber;			///< current line number

	bool m_hasErrors;			///< any errors occurred?
	std::vector<std::wstring> m_messages;	///< error/warning messages

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
	void error(const std::wstring &msg);
	void warning(const std::wstring &msg);

	// Prefix initialization
	static void initPrefixes();

	// Modifier parsing helpers
	bool isModifierToken(const Token *t) const;
	bool isModifierTokenName(const wstringi &name) const;
	bool isAssignModifierToken(const Token *t) const;
	std::vector<AstModifierSpec> parseModifierSpecs();
	std::vector<AstModifierSpec> tokensToModifierSpecs(
		const std::vector<wstringi> &tokens) const;

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
		const wstringi &keyword);

	// Key and event assignments
	AstNodePtr parseKeyAssign();
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
		const wchar_t *buffer, size_t length,
		const wstringi &filename);

	/// Parse from a file (reads via ConfigFiles)
	std::unique_ptr<AstFile> parseFile(
		const wstringi &filename,
		ConfigFiles &configFiles);

	/// Parse a bare action sequence string (e.g. "S-A B @Func") as a key sequence.
	/// No enclosing keyseq statement is required.
	std::unique_ptr<AstKeySequence> parseActions(
		const wchar_t *buffer, size_t length,
		const wstringi &filename = L"<actions>");

	/// Check if there were any errors
	bool hasErrors() const { return m_hasErrors; }

	/// Get collected error/warning messages
	const std::vector<std::wstring> &getMessages() const {
		return m_messages;
	}

	/// Parse a modifier-key string (e.g. "C-A", "*-LButton", "S-C-Return")
	/// into its modifier specifiers and bare key name.
	/// Returns false if str is empty, contains more than one key action,
	/// or is not a simple key action.
	static bool parseModifiedKey(const wstringi &str,
		std::vector<AstModifierSpec> &mods, wstringi &keyName);

	/// Parse a single scan-code string (e.g. "0x1c", "E0-0x1c", "28")
	/// into an AstScanCode.
	/// Returns false if str is empty or not a valid scan code.
	static bool parseScanCode(const wstringi &str, AstScanCode &out);
};


#endif // !_MAYU_PARSER_H
