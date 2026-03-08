//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ast_visitor.h
//
// Visitor interface for AST nodes.


#ifndef _AST_VISITOR_H
#  define _AST_VISITOR_H

#  include "ast.h"


///
class AstVisitor
{
public:
	virtual ~AstVisitor() = default;

	// Top-level
	virtual void visit(const AstFile &node) = 0;
	virtual void visit(const AstConditional &node) = 0;
	virtual void visit(const AstDefineSymbol &node) = 0;
	virtual void visit(const AstInclude &node) = 0;

	// Keyboard definitions
	virtual void visit(const AstDefKey &node) = 0;
	virtual void visit(const AstDefModifier &node) = 0;
	virtual void visit(const AstDefSync &node) = 0;
	virtual void visit(const AstDefAlias &node) = 0;
	virtual void visit(const AstDefSubstitute &node) = 0;
	virtual void visit(const AstDefOption &node) = 0;

	// Keymap and assignments
	virtual void visit(const AstKeymapDef &node) = 0;
	virtual void visit(const AstKeyAssign &node) = 0;
	virtual void visit(const AstEventAssign &node) = 0;
	virtual void visit(const AstModifierAssign &node) = 0;
	virtual void visit(const AstKeySeqDef &node) = 0;

	// Sub-structures
	virtual void visit(const AstKeySequence &node) = 0;
	virtual void visit(const AstModifierSeq &node) = 0;

	// Actions
	virtual void visit(const AstActionKey &node) = 0;
	virtual void visit(const AstActionKeySeqRef &node) = 0;
	virtual void visit(const AstActionFuncCall &node) = 0;
	virtual void visit(const AstActionSubSeq &node) = 0;
};


//=============================================================================
// accept() implementations (inline, referencing AstVisitor)
//=============================================================================

inline void AstFile::accept(AstVisitor &v) const { v.visit(*this); }
inline void AstConditional::accept(AstVisitor &v) const { v.visit(*this); }
inline void AstDefineSymbol::accept(AstVisitor &v) const { v.visit(*this); }
inline void AstInclude::accept(AstVisitor &v) const { v.visit(*this); }
inline void AstDefKey::accept(AstVisitor &v) const { v.visit(*this); }
inline void AstDefModifier::accept(AstVisitor &v) const { v.visit(*this); }
inline void AstDefSync::accept(AstVisitor &v) const { v.visit(*this); }
inline void AstDefAlias::accept(AstVisitor &v) const { v.visit(*this); }
inline void AstDefSubstitute::accept(AstVisitor &v) const { v.visit(*this); }
inline void AstDefOption::accept(AstVisitor &v) const { v.visit(*this); }
inline void AstKeymapDef::accept(AstVisitor &v) const { v.visit(*this); }
inline void AstKeyAssign::accept(AstVisitor &v) const { v.visit(*this); }
inline void AstEventAssign::accept(AstVisitor &v) const { v.visit(*this); }
inline void AstModifierAssign::accept(AstVisitor &v) const { v.visit(*this); }
inline void AstKeySeqDef::accept(AstVisitor &v) const { v.visit(*this); }
inline void AstKeySequence::accept(AstVisitor &v) const { v.visit(*this); }
inline void AstModifierSeq::accept(AstVisitor &v) const { v.visit(*this); }
inline void AstActionKey::accept(AstVisitor &v) const { v.visit(*this); }
inline void AstActionKeySeqRef::accept(AstVisitor &v) const { v.visit(*this); }
inline void AstActionFuncCall::accept(AstVisitor &v) const { v.visit(*this); }
inline void AstActionSubSeq::accept(AstVisitor &v) const { v.visit(*this); }


#endif // !_AST_VISITOR_H
