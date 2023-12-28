#ifndef __CPL_AST_H__
#define __CPL_AST_H__

#include <iostream>
#include <map>
#include <vector>

#include "types.h"
#include "location.h"
#include "lexer.h"
#include "symtypes.h"
#include "astvisitor.h"
#include "scope.h"
#include "ir.h"


namespace AST {

using namespace std;
using namespace Loc;
using namespace Types;
using namespace Lex;
using namespace SymTypes;
using namespace Visitors;


const Type TDefault      = Type("default");
const Type TCompUnit     = Type("<CompUnit>");
const Type TConstDecl    = Type("<ConstDecl>");
const Type TConstDef     = Type("<ConstDef>");
const Type TConstInitVal = Type("<ConstInitVal>");
const Type TVarDecl      = Type("<VarDecl>");
const Type TVarDef       = Type("<VarDef>");
const Type TInitVal      = Type("<InitVal>");
const Type TFuncDef      = Type("<FuncDef>");
const Type TMainFuncDef  = Type("<MainFuncDef>");
const Type TFuncType     = Type("<FuncType>");
const Type TFuncFParams  = Type("<FuncFParams>");
const Type TFuncFParam   = Type("<FuncFParam>");
const Type TBlock        = Type("<Block>");
const Type TStmt         = Type("<Stmt>");
const Type TStmtFor      = Type("<Stmt>");
const Type TStmtReturn   = Type("<Stmt>");
const Type TForStmt      = Type("<ForStmt>");
const Type TExp          = Type("<Exp>");
const Type TCond         = Type("<Cond>");
const Type TLVal         = Type("<LVal>");
const Type TPrimaryExp   = Type("<PrimaryExp>");
const Type TNumber       = Type("<Number>");
const Type TUnaryExp     = Type("<UnaryExp>");
const Type TUnaryOp      = Type("<UnaryOp>");
const Type TFuncRParams  = Type("<FuncRParams>");
const Type TMulExp       = Type("<MulExp>");
const Type TAddExp       = Type("<AddExp>");
const Type TRelExp       = Type("<RelExp>");
const Type TEqExp        = Type("<EqExp>");
const Type TLAndExp      = Type("<LAndExp>");
const Type TLOrExp       = Type("<LOrExp>");
const Type TConstExp     = Type("<ConstExp>");


class Node {
public:
	Location firstLoc;
	Location lastLoc;
	int computedValue = 0;
	bool computed = false;
	bool dummy = false;
	Node *parent = nullptr;
	Scp::Scope *scope = nullptr;
	SymType valType;
	IR::Value *value;

	virtual Type type() const { return TDefault; }

	friend ostream& operator << (ostream &out, const Node *node) {
		if (node->type() != TDefault) {
			out << node->type();
		}
		return out;
	}

	virtual void accept(Visitor &visitor) = 0;
};


class CompUnit : public Node {
public:
	vector <Node *> decl;
	vector <Node *> func;
	Node *mainFunc = nullptr;

	Type type() const { return TCompUnit; }

	void accept(Visitor &visitor) {
		visitor.visitCompUnit(this);
	}
};


class ConstDecl : public Node {
public:
	vector <Node *> constDef;

	Type type() const { return TConstDecl; }

	void accept(Visitor &visitor) {
		visitor.visitConstDecl(this);
	}
};


class ConstDef : public Node {
public:
	Token ident;
	SymType symType;
	vector <Node *> dimExp;
	Node *initVal = nullptr;

	Type type() const { return TConstDef; }

	void accept(Visitor &visitor) {
		visitor.visitConstDef(this);
	}
};


class ConstInitVal : public Node {
public:
	bool isArray = false;
	Node *expr = nullptr;
	vector <Node *> initVal;

	Type type() const { return TConstInitVal; }

	void accept(Visitor &visitor) {
		visitor.visitConstInitVal(this);
	}
};


class VarDecl : public Node {
public:
	vector <Node *> varDef;

	Type type() const { return TVarDecl; }

	void accept(Visitor &visitor) {
		visitor.visitVarDecl(this);
	}
};


class VarDef : public Node {
public:
	Token ident;
	SymType symType;
	vector <Node *> dimExp;
	Node *initVal = nullptr;

	Type type() const { return TVarDef; }

	void accept(Visitor &visitor) {
		visitor.visitVarDef(this);
	}
};


class InitVal : public Node {
public:
	bool isArray = false;
	Node *expr = nullptr;
	vector <Node *> initVal;

	Type type() const { return TInitVal; }

	void accept(Visitor &visitor) {
		visitor.visitInitVal(this);
	}
};


class FuncDef : public Node {
public:
	Token ident;
	Node *funcType = nullptr;
	Node *params = nullptr;
	Node *block = nullptr;

	Type type() const { return TFuncDef; }

	void accept(Visitor &visitor) {
		visitor.visitFuncDef(this);
	}
};


class MainFuncDef : public Node {
public:
	Token ident;
	Node *block = nullptr;

	Type type() const { return TMainFuncDef; }

	void accept(Visitor &visitor) {
		visitor.visitMainFuncDef(this);
	}
};


class FuncType : public Node {
public:
	SymType symType;

	Type type() const { return TFuncType; }

	void accept(Visitor &visitor) {
		visitor.visitFuncType(this);
	}
};


class FuncFParams : public Node {
public:
	vector <Node *> params;

	Type type() const { return TFuncFParams; }

	void accept(Visitor &visitor) {
		visitor.visitFuncFParams(this);
	}
};


class FuncFParam : public Node {
public:
	Token ident;
	SymType symType;
	bool isArray = false;
	vector <Node *> dimExp;

	Type type() const { return TFuncFParam; }

	void accept(Visitor &visitor) {
		visitor.visitFuncFParam(this);
	}
};


class Block : public Node {
public:
	Token start, end;
	vector <Node *> items;

	Type type() const { return TBlock; }

	void accept(Visitor &visitor) {
		visitor.visitBlock(this);
	}
};


class Stmt : public Node {
public:
	Type type() const { return TStmt; }

	virtual void accept(Visitor &visitor) = 0;
};


class StmtAssign : public Stmt {
public:
	Node *lVal = nullptr;
	Node *expr = nullptr;

	void accept(Visitor &visitor) {
		visitor.visitStmtAssign(this);
	}
};


class StmtExp : public Stmt {
public:
	Node *expr = nullptr;

	virtual void accept(Visitor &visitor) {
		visitor.visitStmtExp(this);
	}
};


class StmtBlock : public Stmt {
public:
	Node *block = nullptr;

	void accept(Visitor &visitor) {
		visitor.visitStmtBlock(this);
	}
};


class StmtIf : public Stmt {
public:
	Node *cond = nullptr;
	Node *thenStmt = nullptr;
	Node *elseStmt = nullptr;

	void accept(Visitor &visitor) {
		visitor.visitStmtIf(this);
	};
};


class StmtFor : public Stmt {
public:
	Node *initStmt = nullptr;
	Node *cond = nullptr;
	Node *stepStmt = nullptr;
	Node *stmt = nullptr;

	Type type() const { return TStmtFor; }

	void accept(Visitor &visitor) {
		visitor.visitStmtFor(this);
	}
};


class StmtBreak : public Stmt {
public:
	void accept(Visitor &visitor) {
		visitor.visitStmtBreak(this);
	}
};


class StmtContinue : public Stmt {
public:
	void accept(Visitor &visitor) {
		visitor.visitStmtContinue(this);
	}
};


class StmtReturn : public Stmt {
public:
	Node *expr = nullptr;

	Type type() const { return TStmtReturn; }

	void accept(Visitor &visitor) {
		visitor.visitStmtReturn(this);
	}
};


class StmtGetInt : public Stmt {
public:
	Node *lVal = nullptr;

	void accept(Visitor &visitor) {
		visitor.visitStmtGetInt(this);
	}
};


class StmtPrintf : public Stmt {
public:
	Token format;
	vector <Node *> params;
	vector <string> constString;

	void accept(Visitor &visitor) {
		visitor.visitStmtPrintf(this);
	}
};


class ForStmt : public Node {
public:
	Node *lVal = nullptr;
	Node *expr = nullptr;

	Type type() const { return TForStmt; }

	void accept(Visitor &visitor) {
		visitor.visitForStmt(this);
	}
};


class Exp : public Node {
public:
	Node *expr = nullptr;

	Type type() const { return TExp; }

	void accept(Visitor &visitor) {
		visitor.visitExp(this);
	}
};


class Cond : public Node {
public:
	Node *expr = nullptr;

	Type type() const { return TCond; }

	void accept(Visitor &visitor) {
		visitor.visitCond(this);
	}
};


class LVal : public Node {
public:
	Token ident;
	vector <Node *> dimExp;

	Scp::Variable *var;

	Type type() const { return TLVal; }

	void accept(Visitor &visitor) {
		visitor.visitLVal(this);
	}
};


class PrimaryExp : public Node {
public:
	Node *expr = nullptr;
	Node *number = nullptr;
	Node *lVal = nullptr;

	Type type() const { return TPrimaryExp; }

	void accept(Visitor &visitor) {
		visitor.visitPrimaryExp(this);
	}
};


class Number : public Node {
public:
	Token number;

	Type type() const { return TNumber; }

	void accept(Visitor &visitor) {
		visitor.visitNumber(this);
	}
};


class UnaryExp : public Node {
public:
	Node *expr = nullptr;
	Node *op = nullptr;

	bool funcCall = false;
	Token ident;
	Node *params = nullptr;

	Type type() const { return TUnaryExp; }

	void accept(Visitor &visitor) {
		visitor.visitUnaryExp(this);
	}
};


class UnaryOp : public Node {
public:
	Token op;

	Type type() const { return TUnaryOp; }

	void accept(Visitor &visitor) {
		visitor.visitUnaryOp(this);
	}
};


class FuncRParams : public Node {
public:
	vector <Node *> params;

	Type type() const { return TFuncRParams; }

	void accept(Visitor &visitor) {
		visitor.visitFuncRParams(this);
	}
};


class MulExp : public Node {
public:
	Node *exprL = nullptr, *exprR = nullptr;
	Token op;

	Type type() const { return TMulExp; }

	void accept(Visitor &visitor) {
		visitor.visitMulExp(this);
	}
};


class AddExp : public Node {
public:
	Node *exprL = nullptr, *exprR = nullptr;
	Token op;

	Type type() const { return TAddExp; }

	void accept(Visitor &visitor) {
		visitor.visitAddExp(this);
	}
};


class RelExp : public Node {
public:
	Node *exprL = nullptr, *exprR = nullptr;
	Token op;

	Type type() const { return TRelExp; }

	void accept(Visitor &visitor) {
		visitor.visitRelExp(this);
	}
};


class EqExp : public Node {
public:
	Node *exprL = nullptr, *exprR = nullptr;
	Token op;

	Type type() const { return TEqExp; }

	void accept(Visitor &visitor) {
		visitor.visitEqExp(this);
	}
};


class LAndExp : public Node {
public:
	Node *exprL = nullptr, *exprR = nullptr;
	Token op;

	Type type() const { return TLAndExp; }

	void accept(Visitor &visitor) {
		visitor.visitLAndExp(this);
	}
};


class LOrExp : public Node {
public:
	Node *exprL = nullptr, *exprR = nullptr;
	Token op;

	Type type() const { return TLOrExp; }

	void accept(Visitor &visitor) {
		visitor.visitLOrExp(this);
	}
};


class ConstExp : public Node {
public:
	Node *expr = nullptr;

	Type type() const { return TConstExp; }

	void accept(Visitor &visitor) {
		visitor.visitConstExp(this);
	}
};

}

#endif