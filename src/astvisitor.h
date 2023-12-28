#ifndef __CPL_AST_VISITOR_H__
#define __CPL_AST_VISITOR_H__

#include "ast.h"


namespace AST {

class Node;

class CompUnit;
class ConstDecl;
class ConstDef;
class ConstInitVal;
class VarDecl;
class VarDef;
class InitVal;
class FuncDef;
class MainFuncDef;
class FuncType;
class FuncFParams;
class FuncFParam;
class Block;
class StmtAssign;
class StmtExp;
class StmtBlock;
class StmtIf;
class StmtFor;
class StmtBreak;
class StmtContinue;
class StmtReturn;
class StmtGetInt;
class StmtPrintf;
class ForStmt;
class Exp;
class Cond;
class LVal;
class PrimaryExp;
class Number;
class UnaryExp;
class UnaryOp;
class FuncRParams;
class MulExp;
class AddExp;
class RelExp;
class EqExp;
class LAndExp;
class LOrExp;
class ConstExp;


namespace Visitors {

using namespace std;
using namespace AST;


class Visitor {
public:
	virtual void visitCompUnit(CompUnit *node) {};
	virtual void visitConstDecl(ConstDecl *node) {};
	virtual void visitConstDef(ConstDef *node) {};
	virtual void visitConstInitVal(ConstInitVal *node) {};
	virtual void visitVarDecl(VarDecl *node) {};
	virtual void visitVarDef(VarDef *node) {};
	virtual void visitInitVal(InitVal *node) {};
	virtual void visitFuncDef(FuncDef *node) {};
	virtual void visitMainFuncDef(MainFuncDef *node) {};
	virtual void visitFuncType(FuncType *node) {};
	virtual void visitFuncFParams(FuncFParams *node) {};
	virtual void visitFuncFParam(FuncFParam *node) {};
	virtual void visitBlock(Block *node) {};
	virtual void visitStmtAssign(StmtAssign *node) {};
	virtual void visitStmtExp(StmtExp *node) {};
	virtual void visitStmtBlock(StmtBlock *node) {};
	virtual void visitStmtIf(StmtIf *node) {};
	virtual void visitStmtFor(StmtFor *node) {};
	virtual void visitStmtBreak(StmtBreak *node) {};
	virtual void visitStmtContinue(StmtContinue *node) {};
	virtual void visitStmtReturn(StmtReturn *node) {};
	virtual void visitStmtGetInt(StmtGetInt *node) {};
	virtual void visitStmtPrintf(StmtPrintf *node) {};
	virtual void visitForStmt(ForStmt *node) {};
	virtual void visitExp(Exp *node) {};
	virtual void visitCond(Cond *node) {};
	virtual void visitLVal(LVal *node) {};
	virtual void visitPrimaryExp(PrimaryExp *node) {};
	virtual void visitNumber(Number *node) {};
	virtual void visitUnaryExp(UnaryExp *node) {};
	virtual void visitUnaryOp(UnaryOp *node) {};
	virtual void visitFuncRParams(FuncRParams *node) {};
	virtual void visitMulExp(MulExp *node) {};
	virtual void visitAddExp(AddExp *node) {};
	virtual void visitRelExp(RelExp *node) {};
	virtual void visitEqExp(EqExp *node) {};
	virtual void visitLAndExp(LAndExp *node) {};
	virtual void visitLOrExp(LOrExp *node) {};
	virtual void visitConstExp(ConstExp *node) {};
};

}

}

#endif