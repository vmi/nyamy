//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ast.h
//
// AST (Abstract Syntax Tree) node types for .mayu configuration files.
// All types use the Ast prefix to avoid name collisions with existing
// domain types (Action, Modifier, ScanCode, Key, KeySeq, Keymap, etc.)


#ifndef _AST_H
#  define _AST_H

#  include "stringtool.h"
#  include <vector>
#  include <memory>


class AstVisitor;


/// Source location for error reporting
struct AstSourceLoc {
	wstringi filename;
	size_t line;

	AstSourceLoc() : line(0) {}
	AstSourceLoc(const wstringi &i_filename, size_t i_line)
		: filename(i_filename), line(i_line) {}
};


//=============================================================================
// Base node
//=============================================================================

class AstNode
{
public:
	AstSourceLoc m_loc;

	virtual ~AstNode() = default;
	virtual void accept(AstVisitor &v) const = 0;
};

using AstNodePtr = std::unique_ptr<AstNode>;


//=============================================================================
// Sub-structures (not top-level nodes, used as fields)
//=============================================================================

/// Modifier specifier within a modifier sequence
struct AstModifierSpec {
	enum class Flag { Press, Release, Dontcare };
	wstringi name;		///< "S-", "C-", "*", "~", etc.
	Flag flag;
};


/// Scan code with optional E0-/E1- extensions
struct AstScanCode {
	std::vector<wstringi> extensions;	///< "E0-", "E1-"
	int scanCode;
};


/// Modified key (modifier sequence + key name)
struct AstModifiedKey {
	std::vector<AstModifierSpec> modifiers;
	wstringi keyName;
};


//=============================================================================
// Key sequence sub-nodes
//=============================================================================

/// Base class for actions within a key sequence
class AstAction
{
public:
	std::vector<AstModifierSpec> modifiers;

	virtual ~AstAction() = default;
	virtual void accept(AstVisitor &v) const = 0;
};

using AstActionPtr = std::unique_ptr<AstAction>;


/// Modified key press: MODIFIER* KEY_NAME
class AstActionKey : public AstAction
{
public:
	wstringi keyName;

	void accept(AstVisitor &v) const override;
};


/// Reference to a named key sequence: $NAME
class AstActionKeySeqRef : public AstAction
{
public:
	wstringi name;

	void accept(AstVisitor &v) const override;
};


/// Function argument
class AstArgument
{
public:
	enum class Kind {
		String,
		Number,
		Regexp,
		/** $name.  Either a reference to a named keyseq or an argument
		    substitution ($Clipboard and friends); the two are spelled alike and
		    are told apart by the type of the parameter, which is not known
		    until the engine side loads the call. */
		DollarName,
		KeySeqLiteral,	///< (key_sequence)
		ModifierSeq,	///< modifier sequence as argument
		TokenSeq,		///< multiple raw tokens as one argument (e.g. U- D- RButton)
	};

	Kind kind;
	wstringi stringValue;
	int numberValue;
	std::unique_ptr<class AstKeySequence> keySeq;
	std::vector<AstModifierSpec> modifierSeq;
	std::vector<wstringi> tokens;		///< for Kind::TokenSeq

	AstArgument() : kind(Kind::String), numberValue(0) {}
};


/// Function call: &FUNCTION_NAME(ARGS)
class AstActionFuncCall : public AstAction
{
public:
	wstringi functionName;
	std::vector<std::unique_ptr<AstArgument>> arguments;

	void accept(AstVisitor &v) const override;
};


/// Parenthesized sub-sequence: (KEY_SEQUENCE)
class AstActionSubSeq : public AstAction
{
public:
	std::unique_ptr<class AstKeySequence> sequence;

	void accept(AstVisitor &v) const override;
};


/// A full key sequence: ACTION+
class AstKeySequence : public AstNode
{
public:
	std::vector<AstActionPtr> actions;

	void accept(AstVisitor &v) const override;
};


//=============================================================================
// Modifier sequence (as a top-level construct in key assign etc.)
//=============================================================================

class AstModifierSeq : public AstNode
{
public:
	std::vector<AstModifierSpec> specs;

	void accept(AstVisitor &v) const override;
};


//=============================================================================
// Top-level statement nodes
//=============================================================================

/// File: a sequence of statements
class AstFile : public AstNode
{
public:
	std::vector<AstNodePtr> statements;

	void accept(AstVisitor &v) const override;
};


/// Conditional block: if/elseif/else/endif
struct AstCondBranch {
	bool isNegated;				///< "!" prefix
	wstringi symbol;			///< symbol name (empty for "else")

	/// additional AND conditions (from inline "and ( ... )" on same line)
	struct AndCond {
		bool isNegated;
		wstringi symbol;
	};
	std::vector<AndCond> andConditions;

	std::vector<AstNodePtr> body;

	AstCondBranch() : isNegated(false) {}
	AstCondBranch(AstCondBranch &&other) = default;
	AstCondBranch &operator=(AstCondBranch &&other) = default;
	AstCondBranch(const AstCondBranch &) = delete;
	AstCondBranch &operator=(const AstCondBranch &) = delete;
};

class AstConditional : public AstNode
{
public:
	std::vector<AstCondBranch> branches;

	void accept(AstVisitor &v) const override;
};


/// define SYMBOL
class AstDefineSymbol : public AstNode
{
public:
	wstringi symbol;

	void accept(AstVisitor &v) const override;
};


/// include FILENAME
class AstInclude : public AstNode
{
public:
	wstringi filename;

	void accept(AstVisitor &v) const override;
};


/// def key NAMES = SCAN_CODES
class AstDefKey : public AstNode
{
public:
	bool isParenthesized;
	std::vector<wstringi> names;
	std::vector<AstScanCode> scanCodes;

	AstDefKey() : isParenthesized(false) {}

	void accept(AstVisitor &v) const override;
};


/// def mod MODIFIER_NAME = KEY_NAME*
class AstDefModifier : public AstNode
{
public:
	wstringi modifierName;			///< "shift", "alt", "control", "windows"
	std::vector<wstringi> keyNames;

	void accept(AstVisitor &v) const override;
};


/// def sync = SCAN_CODES
class AstDefSync : public AstNode
{
public:
	std::vector<AstScanCode> scanCodes;

	void accept(AstVisitor &v) const override;
};


/// def alias NAME = KEY_NAME
class AstDefAlias : public AstNode
{
public:
	wstringi aliasName;
	wstringi keyName;

	void accept(AstVisitor &v) const override;
};


/// def subst MODIFIED_KEY+ = KEY_SEQUENCE
class AstDefSubstitute : public AstNode
{
public:
	std::vector<AstModifiedKey> lhsKeys;
	std::unique_ptr<AstKeySequence> rhsKeySeq;

	void accept(AstVisitor &v) const override;
};


/// def option NAME = VALUE
class AstDefOption : public AstNode
{
public:
	wstringi optionName;		///< "KL-", "delay-of", "sts4mayu", etc.
	wstringi qualifier;			///< "!!!" for delay-of, empty otherwise
	wstringi value;

	void accept(AstVisitor &v) const override;
};


/// keymap / keymap2 / window definition
struct AstWindowSpec {
	wstringi className;			///< regexp
	wstringi titleName;			///< regexp (may be empty)
	wstringi op;				///< "&&" or "||" (empty if class-only)
};

class AstKeymapDef : public AstNode
{
public:
	wstringi keyword;			///< "keymap", "keymap2", "window"
	wstringi name;
	std::unique_ptr<AstWindowSpec> window;
	wstringi parentName;		///< after ":" (empty if none)
	std::unique_ptr<AstKeySequence> defaultKeySeq;

	void accept(AstVisitor &v) const override;
};


/// key MODIFIER* KEY+ = KEY_SEQUENCE  (key assignment)
class AstKeyAssign : public AstNode
{
public:
	std::vector<AstModifiedKey> lhsKeys;
	std::unique_ptr<AstKeySequence> rhsKeySeq;

	void accept(AstVisitor &v) const override;
};


/// event EVENT_NAME = KEY_SEQUENCE
class AstEventAssign : public AstNode
{
public:
	wstringi eventName;
	std::unique_ptr<AstKeySequence> keySeq;

	void accept(AstVisitor &v) const override;
};


/// mod (ASSIGN_MODE MODIFIER_NAME)* MODIFIER_NAME ASSIGN_OP (MODE? KEY)*
struct AstModAssignPrefix {
	wstringi assignMode;		///< "!", "!!", "!!!" or empty
	wstringi modifierName;
};

struct AstModAssignKey {
	wstringi assignMode;		///< "!", "!!", "!!!" or empty
	wstringi keyName;
};

class AstModifierAssign : public AstNode
{
public:
	std::vector<AstModAssignPrefix> prefixes;
	wstringi mainModifierName;
	wstringi op;				///< "=", "+=", "-="
	std::vector<AstModAssignKey> keys;

	void accept(AstVisitor &v) const override;
};


/// keyseq $NAME = KEY_SEQUENCE
class AstKeySeqDef : public AstNode
{
public:
	wstringi name;
	std::unique_ptr<AstKeySequence> keySeq;

	void accept(AstVisitor &v) const override;
};


#endif // !_AST_H
