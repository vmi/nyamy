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
	tstringi filename;
	size_t line;

	AstSourceLoc() : line(0) {}
	AstSourceLoc(const tstringi &i_filename, size_t i_line)
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
	enum Flag { Press, Release, Dontcare };
	tstringi name;		///< "S-", "C-", "*", "~", etc.
	Flag flag;
};


/// Scan code with optional E0-/E1- extensions
struct AstScanCode {
	std::vector<tstringi> extensions;	///< "E0-", "E1-"
	int scanCode;
};


/// Modified key (modifier sequence + key name)
struct AstModifiedKey {
	std::vector<AstModifierSpec> modifiers;
	tstringi keyName;
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
	tstringi keyName;

	void accept(AstVisitor &v) const override;
};


/// Reference to a named key sequence: $NAME
class AstActionKeySeqRef : public AstAction
{
public:
	tstringi name;

	void accept(AstVisitor &v) const override;
};


/// Function argument
class AstArgument
{
public:
	enum Kind {
		Kind_String,
		Kind_Number,
		Kind_Regexp,
		Kind_KeySeqRef,		///< $name
		Kind_KeySeqLiteral,	///< (key_sequence)
		Kind_ModifierSeq,	///< modifier sequence as argument
	};

	Kind kind;
	tstringi stringValue;
	int numberValue;
	std::unique_ptr<class AstKeySequence> keySeq;
	std::vector<AstModifierSpec> modifierSeq;

	AstArgument() : kind(Kind_String), numberValue(0) {}
};


/// Function call: &FUNCTION_NAME(ARGS)
class AstActionFuncCall : public AstAction
{
public:
	tstringi functionName;
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
	tstringi symbol;			///< symbol name (empty for "else")

	/// additional AND conditions (from inline "and ( ... )" on same line)
	struct AndCond {
		bool isNegated;
		tstringi symbol;
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
	tstringi symbol;

	void accept(AstVisitor &v) const override;
};


/// include FILENAME
class AstInclude : public AstNode
{
public:
	tstringi filename;

	void accept(AstVisitor &v) const override;
};


/// def key NAMES = SCAN_CODES
class AstDefKey : public AstNode
{
public:
	bool isParenthesized;
	std::vector<tstringi> names;
	std::vector<AstScanCode> scanCodes;

	AstDefKey() : isParenthesized(false) {}

	void accept(AstVisitor &v) const override;
};


/// def mod MODIFIER_NAME = KEY_NAME*
class AstDefModifier : public AstNode
{
public:
	tstringi modifierName;			///< "shift", "alt", "control", "windows"
	std::vector<tstringi> keyNames;

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
	tstringi aliasName;
	tstringi keyName;

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
	tstringi optionName;		///< "KL-", "delay-of", "sts4mayu", etc.
	tstringi qualifier;			///< "!!!" for delay-of, empty otherwise
	tstringi value;

	void accept(AstVisitor &v) const override;
};


/// keymap / keymap2 / window definition
struct AstWindowSpec {
	tstringi className;			///< regexp
	tstringi titleName;			///< regexp (may be empty)
	tstringi op;				///< "&&" or "||" (empty if class-only)
};

class AstKeymapDef : public AstNode
{
public:
	tstringi keyword;			///< "keymap", "keymap2", "window"
	tstringi name;
	std::unique_ptr<AstWindowSpec> window;
	tstringi parentName;		///< after ":" (empty if none)
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


/// key MODIFIER = MODIFIER  (default modifier change)
class AstKeyDefaultModifier : public AstNode
{
public:
	std::vector<AstModifierSpec> assignModifier;
	std::vector<AstModifierSpec> keySeqModifier;

	void accept(AstVisitor &v) const override;
};


/// event EVENT_NAME = KEY_SEQUENCE
class AstEventAssign : public AstNode
{
public:
	tstringi eventName;
	std::unique_ptr<AstKeySequence> keySeq;

	void accept(AstVisitor &v) const override;
};


/// mod (ASSIGN_MODE MODIFIER_NAME)* MODIFIER_NAME ASSIGN_OP (MODE? KEY)*
struct AstModAssignPrefix {
	tstringi assignMode;		///< "!", "!!", "!!!" or empty
	tstringi modifierName;
};

struct AstModAssignKey {
	tstringi assignMode;		///< "!", "!!", "!!!" or empty
	tstringi keyName;
};

class AstModifierAssign : public AstNode
{
public:
	std::vector<AstModAssignPrefix> prefixes;
	tstringi mainModifierName;
	tstringi op;				///< "=", "+=", "-="
	std::vector<AstModAssignKey> keys;

	void accept(AstVisitor &v) const override;
};


/// keyseq $NAME = KEY_SEQUENCE
class AstKeySeqDef : public AstNode
{
public:
	tstringi name;
	std::unique_ptr<AstKeySequence> keySeq;

	void accept(AstVisitor &v) const override;
};


#endif // !_AST_H
