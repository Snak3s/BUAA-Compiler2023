#ifndef __CPL_SCOPE_BUILDER_H__
#define __CPL_SCOPE_BUILDER_H__

#include <vector>

#include "config.h"
#include "ast.h"
#include "astvisitor.h"
#include "scope.h"
#include "lexer.h"
#include "errors.h"


namespace AST {

namespace Visitors {

using namespace std;
using namespace AST;
using namespace Scp;
using namespace Lex;


class ScopeBuilder : public Visitor {
public:
	Scope *scope;
	vector <Node *> stack;

	Err::Log *errors;

	Variable *curVar = nullptr;
	Function *curFunc = nullptr;

	ScopeBuilder(Err::Log *errors = new Err::Log()) {
		this->scope = new Scope();
		this->errors = errors;
	}
	ScopeBuilder(Scope *scope = new Scope(), Err::Log *errors = new Err::Log()) {
		this->scope = scope;
		this->errors = errors;
	}

	void visit(Node *node) {
		if (CPL_Debug) {
			for (int i = 1; i <= stack.size(); i++)
				IO::out << " ";
			IO::out << node << endl;
		}
		if (stack.size()) {
			node->parent = stack[stack.size() - 1];
		}
		node->scope = scope;
		stack.emplace_back(node);
	}

	void leave(Node *node) {
		stack.pop_back();
		if (CPL_Debug) {
			for (int i = 1; i <= stack.size(); i++)
				IO::out << " ";
			IO::out << "/" << node << endl;
		}
	}

	Node *lastNodeOfType(const Type &type) {
		for (int i = stack.size(); i; i--) {
			if (stack[i - 1] && stack[i - 1]->type() == type) {
				return stack[i - 1];
			}
		}
		return nullptr;
	}

	void visitCompUnit(CompUnit *node) {
		visit(node);
		for (Node *decl : node->decl) {
			decl->accept(*this);
		}
		for (Node *func : node->func) {
			func->accept(*this);
		}
		node->mainFunc->accept(*this);
		leave(node);
	}

	void visitConstDecl(ConstDecl *node) {
		visit(node);
		for (Node *def : node->constDef) {
			def->accept(*this);
		}
		leave(node);
	}

	void visitConstDef(ConstDef *node) {
		visit(node);
		for (Node *dimExp : node->dimExp) {
			dimExp->accept(*this);
			node->dummy |= dimExp->dummy;
			if (dimExp->computed) {
				if (dimExp->computedValue < 0) {
					errors->raise(dimExp->firstLoc, Err::NegArrLength);
					node->dummy = true;
				} else {
					node->symType.append(dimExp->computedValue);
				}
			} else {
				errors->raise(dimExp->firstLoc, Err::IndetArrLen);
				node->dummy = true;
			}
		}
		if (!node->dummy) {
			Variable var(node->ident, node->symType);
			curVar = &var;
			node->initVal->accept(*this);
			curVar = nullptr;
			node->dummy |= node->initVal->dummy;
			if (!node->dummy) {
				if (var.init) {
					if (!scope->defineVar(var)) {
						errors->raise(node->ident.loc, Err::DupIdent);
						node->dummy = true;
					}
				} else {
					errors->raise(node->ident.loc, Err::IndetInitVal);
					node->dummy = true;
				}
			}
		}
		leave(node);
	}

	void visitConstInitVal(ConstInitVal *node) {
		visit(node);
		curVar->init = true;
		if (node->isArray) {
			for (Node *initVal : node->initVal) {
				Variable *parent = curVar;
				Variable *var = new Variable(parent->ident, parent->symType);
				var->symType.pop();
				curVar = var;
				initVal->accept(*this);
				parent->initArray.emplace_back(var);
				parent->init &= var->init;
				node->dummy |= initVal->dummy;
				curVar = parent;
			}
			if (!curVar->symType.isArray || node->initVal.size() > curVar->symType[0]) {
				curVar->init = false;
				node->dummy = true;
				errors->raise(node->firstLoc, Err::InitValCnt);
			}
		} else {
			node->expr->accept(*this);
			node->dummy = node->expr->dummy;
			if (curVar->symType.isArray) {
				curVar->init = false;
				node->dummy = true;
				errors->raise(node->firstLoc, Err::InitValCnt);
			}
			if (node->expr->computed) {
				curVar->initVal = node->expr->computedValue;
			} else {
				curVar->init = false;
			}
		}
		leave(node);
	}

	void visitVarDecl(VarDecl *node) {
		visit(node);
		for (Node *def : node->varDef) {
			def->accept(*this);
		}
		leave(node);
	}

	void visitVarDef(VarDef *node) {
		visit(node);
		for (Node *dimExp : node->dimExp) {
			dimExp->accept(*this);
			node->dummy |= dimExp->dummy;
			if (dimExp->computed) {
				if (dimExp->computedValue < 0) {
					errors->raise(dimExp->firstLoc, Err::NegArrLength);
					node->dummy = true;
				} else {
					node->symType.append(dimExp->computedValue);
				}
			} else {
				errors->raise(dimExp->firstLoc, Err::IndetArrLen);
				node->dummy = true;
			}
		}
		if (!node->dummy) {
			Variable var(node->ident, node->symType);
			if (node->initVal) {
				curVar = &var;
				node->initVal->accept(*this);
				curVar = nullptr;
			}
			if (!scope->defineVar(var)) {
				errors->raise(node->ident.loc, Err::DupIdent);
				node->dummy = true;
			}
		}
		leave(node);
	}

	void visitInitVal(InitVal *node) {
		visit(node);
		curVar->init = true;
		if (node->isArray) {
			for (Node *initVal : node->initVal) {
				Variable *parent = curVar;
				Variable *var = new Variable(parent->ident, parent->symType);
				var->symType.pop();
				curVar = var;
				initVal->accept(*this);
				parent->initArray.emplace_back(var);
				parent->init &= var->init;
				node->dummy |= initVal->dummy;
				curVar = parent;
			}
			if (!curVar->symType.isArray || node->initVal.size() > curVar->symType[0]) {
				errors->raise(node->firstLoc, Err::InitValCnt);
			}
		} else {
			node->expr->accept(*this);
			if (curVar->symType.isArray) {
				errors->raise(node->firstLoc, Err::InitValCnt);
			} else if (!node->expr->dummy && node->expr->valType != SymType(TInt)) {
				errors->raise(node->firstLoc, Err::InitValCnt);
			}
			if (node->expr->computed) {
				curVar->initVal = node->expr->computedValue;
			} else {
				curVar->init = false;
			}
		}
		leave(node);
	}

	void visitFuncDef(FuncDef *node) {
		visit(node);
		FuncType *funcType = (FuncType *)node->funcType;
		Function func = Function(node->ident, funcType->symType);
		if (node->params) {
			curFunc = &func;
			node->params->accept(*this);
			curFunc = nullptr;
			node->dummy |= node->params->dummy;
		}
		if (!node->dummy) {
			if (!scope->defineFunc(func)) {
				errors->raise(node->ident.loc, Err::DupIdent);
				node->dummy = true;
			}
			scope = scope->allocate();
			if (!node->dummy) {
				scope->getFunc(node->ident)->scope = scope;
			}
			for (Variable *param : func.params) {
				if (!scope->defineVar(*param)) {
					errors->raise(param->ident.loc, Err::DupIdent);
					node->dummy = true;
				}
			}
			node->block->accept(*this);
			if (funcType->symType == SymType(TInt)) {
				Block *block = (Block *)node->block;
				if (block->items.size() == 0 || block->items[block->items.size() - 1]->type() != TStmtReturn) {
					errors->raise(node->lastLoc, Err::NoReturn);
					node->dummy = true;
				}
			}
			scope = scope->parent;
		}
		leave(node);
	}

	void visitMainFuncDef(MainFuncDef *node) {
		visit(node);
		Function func = Function(node->ident, SymType(TInt));
		if (!scope->defineFunc(func)) {
			errors->raise(node->ident.loc, Err::DupIdent);
			node->dummy = true;
		}
		scope = scope->allocate();
		if (!node->dummy) {
			scope->getFunc(node->ident)->scope = scope;
		}
		node->block->accept(*this);
		Block *block = (Block *)node->block;
		if (block->items.size() == 0 || block->items[block->items.size() - 1]->type() != TStmtReturn) {
			errors->raise(node->lastLoc, Err::NoReturn);
			node->dummy = true;
		}
		scope = scope->parent;
		leave(node);
	}

	void visitFuncFParams(FuncFParams *node) {
		visit(node);
		for (Node *param : node->params) {
			param->accept(*this);
			node->dummy |= param->dummy;
		}
		leave(node);
	}

	void visitFuncFParam(FuncFParam *node) {
		visit(node);
		if (node->isArray) {
			node->symType.isPointer = true;
			node->symType.append(0);
		}
		for (Node *dimExp : node->dimExp) {
			dimExp->accept(*this);
			node->dummy |= dimExp->dummy;
			if (dimExp->computed) {
				if (dimExp->computedValue < 0) {
					errors->raise(dimExp->firstLoc, Err::NegArrLength);
					node->dummy = true;
				} else {
					node->symType.append(dimExp->computedValue);
				}
			} else {
				errors->raise(dimExp->firstLoc, Err::IndetArrLen);
				node->dummy = true;
			}
		}
		if (!node->dummy) {
			Variable *param = new Variable(node->ident, node->symType);
			curFunc->params.emplace_back(param);
		}
		leave(node);
	}

	void visitBlock(Block *node) {
		visit(node);
		if (node->parent->type() != TFuncDef && node->parent->type() != TMainFuncDef) {
			scope = scope->allocate();
		}
		for (Node *item : node->items) {
			item->accept(*this);
			node->dummy |= item->dummy;
		}
		if (node->parent->type() != TFuncDef && node->parent->type() != TMainFuncDef) {
			scope = scope->parent;
		}
		leave(node);
	}

	void visitStmtAssign(StmtAssign *node) {
		visit(node);
		node->lVal->accept(*this);
		node->expr->accept(*this);
		node->dummy |= node->lVal->dummy | node->expr->dummy;
		if (!node->lVal->dummy) {
			LVal *lVal = (LVal *)node->lVal;
			if (lVal->var->isConst()) {
				errors->raise(lVal->firstLoc, Err::ConstAssign);
				node->dummy = true;
			}
		}
		if (!node->dummy) {
			if (node->lVal->valType != SymType(TInt)) {
				errors->raise(node->lVal->firstLoc, Err::InvalOpTypes);
				node->dummy = true;
			}
			if (node->expr->valType != SymType(TInt)) {
				errors->raise(node->expr->firstLoc, Err::InvalOpTypes);
				node->dummy = true;
			}
		}
		leave(node);
	}

	void visitStmtExp(StmtExp *node) {
		visit(node);
		if (node->expr) {
			node->expr->accept(*this);
			node->dummy |= node->expr->dummy;
		}
		leave(node);
	}

	void visitStmtBlock(StmtBlock *node) {
		visit(node);
		node->block->accept(*this);
		node->dummy |= node->block->dummy;
		leave(node);
	}

	void visitStmtIf(StmtIf *node) {
		visit(node);
		node->cond->accept(*this);
		node->thenStmt->accept(*this);
		node->dummy |= node->cond->dummy | node->thenStmt->dummy;
		if (node->elseStmt) {
			node->elseStmt->accept(*this);
			node->dummy |= node->elseStmt->dummy;
		}
		if (!node->cond->dummy && node->cond->valType != SymType(TInt)) {
			errors->raise(node->cond->firstLoc, Err::CondNotInt);
			node->dummy = true;
		}
		leave(node);
	}

	void visitStmtFor(StmtFor *node) {
		visit(node);
		node->stmt->accept(*this);
		node->dummy |= node->stmt->dummy;
		if (node->initStmt) {
			node->initStmt->accept(*this);
			node->dummy |= node->initStmt->dummy;
		}
		if (node->cond) {
			node->cond->accept(*this);
			node->dummy |= node->cond->dummy;
		}
		if (node->stepStmt) {
			node->stepStmt->accept(*this);
			node->dummy |= node->stepStmt->dummy;
		}
		if (node->cond && !node->cond->dummy && node->cond->valType != SymType(TInt)) {
			errors->raise(node->cond->firstLoc, Err::CondNotInt);
			node->dummy = true;
		}
		leave(node);
	}

	void visitStmtBreak(StmtBreak *node) {
		visit(node);
		if (!lastNodeOfType(TStmtFor)) {
			errors->raise(node->firstLoc, Err::BreakCont);
			node->dummy = true;
		}
		leave(node);
	}

	void visitStmtContinue(StmtContinue *node) {
		visit(node);
		if (!lastNodeOfType(TStmtFor)) {
			errors->raise(node->firstLoc, Err::BreakCont);
			node->dummy = true;
		}
		leave(node);
	}

	void visitStmtReturn(StmtReturn *node) {
		visit(node);
		SymType symType = SymType(TInt);
		FuncDef *func = (FuncDef *)lastNodeOfType(TFuncDef);
		if (func) {
			FuncType *funcType = (FuncType *)func->funcType;
			symType = funcType->symType;
		}
		if (node->expr) {
			node->expr->accept(*this);
			node->dummy |= node->expr->dummy;
			if (!node->expr->dummy && node->expr->valType != symType) {
				if (symType == SymType(TVoid)) {
					errors->raise(node->firstLoc, Err::UnexpReturn);
				} else {
					errors->raise(node->firstLoc, Err::RetValType);
				}
				node->dummy = true;
			}
		} else {
			if (symType != SymType(TVoid)) {
				errors->raise(node->firstLoc, Err::RetValType);
			}
		}
		leave(node);
	}

	void visitStmtGetInt(StmtGetInt *node) {
		visit(node);
		node->lVal->accept(*this);
		node->dummy |= node->lVal->dummy;
		if (!node->lVal->dummy) {
			LVal *lVal = (LVal *)node->lVal;
			if (lVal->var->isConst()) {
				errors->raise(lVal->firstLoc, Err::ConstAssign);
				node->dummy = true;
			}
		}
		if (!node->dummy) {
			if (node->lVal->valType != SymType(TInt)) {
				errors->raise(node->lVal->firstLoc, Err::InvalOpTypes);
				node->dummy = true;
			}
		}
		leave(node);
	}

	void visitStmtPrintf(StmtPrintf *node) {
		visit(node);
		for (Node *param : node->params) {
			param->accept(*this);
			node->dummy |= param->dummy;
			if (!param->dummy && param->valType != SymType(TInt)) {
				errors->raise(param->firstLoc, Err::PrintfArgs);
				node->dummy = true;
			}
		}
		bool dummy = false;
		int paramCnt = 0;
		string constString;
		for (int i = 0; i < node->format.strVal.length(); i++) {
			char cur = node->format.strVal[i];
			if (cur < 32 || (33 < cur && cur < 40 && cur != 37) || cur > 126) {
				dummy = true;
				break;
			}
			if (node->format.strVal[i] == '\\') {
				if (i + 1 >= node->format.strVal.length()) {
					dummy = true;
					break;
				}
				if (node->format.strVal[i + 1] != 'n') {
					dummy = true;
					break;
				}
				constString += '\n';
				i++;
				continue;
			}
			if (node->format.strVal[i] == '%') {
				if (i + 1 >= node->format.strVal.length()) {
					dummy = true;
					break;
				}
				if (node->format.strVal[i + 1] != 'd') {
					dummy = true;
					break;
				}
				paramCnt++;
				node->constString.emplace_back(constString);
				constString.clear();
				i++;
				continue;
			}
			constString += node->format.strVal[i];
		}
		node->constString.emplace_back(constString);
		if (dummy) {
			errors->raise(node->firstLoc, Err::IllegalSym);
			node->dummy = true;
		} else if (paramCnt != node->params.size()) {
			errors->raise(node->firstLoc, Err::PrintfArgs);
			node->dummy = true;
		}
		leave(node);
	}

	void visitForStmt(ForStmt *node) {
		visit(node);
		node->lVal->accept(*this);
		node->expr->accept(*this);
		node->dummy |= node->lVal->dummy | node->expr->dummy;
		if (!node->lVal->dummy) {
			LVal *lVal = (LVal *)node->lVal;
			if (lVal->var->isConst()) {
				errors->raise(lVal->firstLoc, Err::ConstAssign);
				node->dummy = true;
			}
		}
		if (!node->dummy) {
			if (node->lVal->valType != SymType(TInt)) {
				errors->raise(node->lVal->firstLoc, Err::InvalOpTypes);
				node->dummy = true;
			}
			if (node->expr->valType != SymType(TInt)) {
				errors->raise(node->expr->firstLoc, Err::InvalOpTypes);
				node->dummy = true;
			}
		}
		leave(node);
	}

	void visitExp(Exp *node) {
		visit(node);
		node->expr->accept(*this);
		node->computed = node->expr->computed;
		node->computedValue = node->expr->computedValue;
		node->valType = node->expr->valType;
		node->dummy |= node->expr->dummy;
		leave(node);
	}

	void visitCond(Cond *node) {
		visit(node);
		node->expr->accept(*this);
		node->computed = node->expr->computed;
		node->computedValue = node->expr->computedValue;
		node->valType = node->expr->valType;
		node->dummy |= node->expr->dummy;
		leave(node);
	}

	void visitLVal(LVal *node) {
		visit(node);
		for (Node *dimExp : node->dimExp) {
			dimExp->accept(*this);
			node->dummy |= dimExp->dummy;
		}
		Variable *var = scope->getVar(node->ident);
		node->var = var;
		if (!var) {
			errors->raise(node->ident.loc, Err::UndefIdent);
			node->dummy = true;
			leave(node);
			return;
		}
		node->valType = var->symType;
		for (Node *dimExp : node->dimExp) {
			if (dimExp->valType != SymType(TInt)) {
				errors->raise(dimExp->firstLoc, Err::IndexNotInt);
				node->dummy = true;
			}
			node->valType.pop();
		}
		bool isGlobalScope = !lastNodeOfType(TFuncDef) && !lastNodeOfType(TMainFuncDef);
		if (var->init && !node->dummy && (var->isConst() || isGlobalScope)) {
			node->computed = true;
			SymType symType = SymType(var->symType.type);
			for (Node *dimExp : node->dimExp) {
				if (dimExp->computed) {
					symType.append(dimExp->computedValue);
				}
				node->computed &= dimExp->computed;
			}
			if (node->computed) {
				if (symType <= var->symType) {
					for (int i = 0; i < symType.dimLen; i++) {
						var = (*var)[symType[i]];
					}
					node->computedValue = var->get();
				} else {
					errors->raise(node->ident.loc, Err::ArrayDim);
					node->computed = false;
					node->dummy = true;
				}
			}
		}
		leave(node);
	}

	void visitPrimaryExp(PrimaryExp *node) {
		visit(node);
		Node *child = nullptr;
		if (node->expr) {
			child = node->expr;
		} else if (node->number) {
			child = node->number;
		} else if (node->lVal) {
			child = node->lVal;
		} else {
			node->dummy = true;
		}
		if (child) {
			child->accept(*this);
			node->computed = child->computed;
			node->computedValue = child->computedValue;
			node->valType = child->valType;
			node->dummy |= child->dummy;
		}
		leave(node);
	}

	void visitNumber(Number *node) {
		visit(node);
		node->computed = true;
		node->computedValue = node->number.numVal;
		node->valType = SymType(TInt);
		leave(node);
	}

	void visitUnaryExp(UnaryExp *node) {
		visit(node);
		if (node->funcCall) {
			int paramCnt = 0;
			FuncRParams *params = (FuncRParams *)node->params;
			if (params) {
				params->accept(*this);
				node->dummy |= params->dummy;
				paramCnt = params->params.size();
			}
			Function *func = scope->getFunc(node->ident);
			if (!func) {
				errors->raise(node->ident.loc, Err::UndefIdent);
				node->dummy = true;
				leave(node);
				return;
			}
			node->valType = func->funcType;
			if (func->params.size() != paramCnt) {
				errors->raise(node->firstLoc, Err::FuncArgsCnt);
				node->dummy = true;
			} else if (!node->dummy && paramCnt > 0) {
				for (int i = 0; i < func->params.size(); i++) {
					if (params->params[i]->valType != func->params[i]->symType) {
						errors->raise(node->firstLoc, Err::FuncArgType);
						node->dummy = true;
						break;
					}
				}
			}
		} else {
			node->expr->accept(*this);
			node->computed = node->expr->computed;
			node->computedValue = node->expr->computedValue;
			node->valType = node->expr->valType;
			node->dummy |= node->expr->dummy;
			if (node->op) {
				UnaryOp *op = (UnaryOp *)node->op;
				if (node->valType != SymType(TInt)) {
					errors->raise(op->op.loc, Err::InvalOpTypes);
					node->dummy = true;
				} else if (node->computed) {
					if (op->op == MINU) {
						node->computedValue = -(node->computedValue);
					} else if (op->op == NOT) {
						node->computedValue = !(node->computedValue);
					}
				}
			}
		}
		leave(node);
	}

	void visitFuncRParams(FuncRParams *node) {
		visit(node);
		for (Node *param : node->params) {
			param->accept(*this);
			node->dummy |= param->dummy;
		}
		leave(node);
	}

	void visitMulExp(MulExp *node) {
		visit(node);
		node->exprL->accept(*this);
		node->computed = node->exprL->computed;
		node->computedValue = node->exprL->computedValue;
		node->valType = node->exprL->valType;
		node->dummy |= node->exprL->dummy;
		if (node->exprR) {
			node->exprR->accept(*this);
			node->computed &= node->exprR->computed;
			node->dummy |= node->exprR->dummy;
			if (node->valType != SymType(TInt) || node->exprR->valType != SymType(TInt)) {
				errors->raise(node->op.loc, Err::InvalOpTypes);
				node->dummy = true;
			} else if (node->computed) {
				if (node->op == MULT) {
					node->computedValue *= node->exprR->computedValue;
				} else if (node->op == DIV) {
					if (node->exprR->computedValue != 0) {
						node->computedValue /= node->exprR->computedValue;
					} else {
						errors->raise(node->op.loc, Err::DivByZero);
						node->computed = false;
						node->computedValue = 0;
						node->dummy = true;
					}
				} else if (node->op == MOD) {
					if (node->exprR->computedValue != 0) {
						node->computedValue %= node->exprR->computedValue;
					} else {
						errors->raise(node->op.loc, Err::DivByZero);
						node->computed = false;
						node->computedValue = 0;
						node->dummy = true;
					}
				}
			}
		}
		leave(node);
	}

	void visitAddExp(AddExp *node) {
		visit(node);
		node->exprL->accept(*this);
		node->computed = node->exprL->computed;
		node->computedValue = node->exprL->computedValue;
		node->valType = node->exprL->valType;
		node->dummy |= node->exprL->dummy;
		if (node->exprR) {
			node->exprR->accept(*this);
			node->computed &= node->exprR->computed;
			node->dummy |= node->exprR->dummy;
			if (node->valType != SymType(TInt) || node->exprR->valType != SymType(TInt)) {
				errors->raise(node->op.loc, Err::InvalOpTypes);
				node->dummy = true;
			} if (node->computed) {
				if (node->op == PLUS) {
					node->computedValue += node->exprR->computedValue;
				} else if (node->op == MINU) {
					node->computedValue -= node->exprR->computedValue;
				}
			}
		}
		leave(node);
	}

	void visitRelExp(RelExp *node) {
		visit(node);
		node->exprL->accept(*this);
		node->computed = node->exprL->computed;
		node->computedValue = node->exprL->computedValue;
		node->valType = node->exprL->valType;
		node->dummy |= node->exprL->dummy;
		if (node->exprR) {
			node->exprR->accept(*this);
			node->computed &= node->exprR->computed;
			node->dummy |= node->exprR->dummy;
			if (node->valType != SymType(TInt) || node->exprR->valType != SymType(TInt)) {
				errors->raise(node->op.loc, Err::InvalOpTypes);
				node->dummy = true;
			} if (node->computed) {
				if (node->op == LSS) {
					node->computedValue = (node->computedValue < node->exprR->computedValue);
				} else if (node->op == GRE) {
					node->computedValue = (node->computedValue > node->exprR->computedValue);
				} else if (node->op == LEQ) {
					node->computedValue = (node->computedValue <= node->exprR->computedValue);
				} else if (node->op == GEQ) {
					node->computedValue = (node->computedValue >= node->exprR->computedValue);
				}
			}
		}
		leave(node);
	}

	void visitEqExp(EqExp *node) {
		visit(node);
		node->exprL->accept(*this);
		node->computed = node->exprL->computed;
		node->computedValue = node->exprL->computedValue;
		node->valType = node->exprL->valType;
		node->dummy |= node->exprL->dummy;
		if (node->exprR) {
			node->exprR->accept(*this);
			node->computed &= node->exprR->computed;
			node->dummy |= node->exprR->dummy;
			if (node->valType != SymType(TInt) || node->exprR->valType != SymType(TInt)) {
				errors->raise(node->op.loc, Err::InvalOpTypes);
				node->dummy = true;
			} if (node->computed) {
				if (node->op == EQL) {
					node->computedValue = (node->computedValue == node->exprR->computedValue);
				} else if (node->op == NEQ) {
					node->computedValue = (node->computedValue != node->exprR->computedValue);
				}
			}
		}
		leave(node);
	}

	void visitLAndExp(LAndExp *node) {
		visit(node);
		node->exprL->accept(*this);
		node->computed = node->exprL->computed;
		node->computedValue = node->exprL->computedValue;
		node->valType = node->exprL->valType;
		node->dummy |= node->exprL->dummy;
		if (node->exprR) {
			node->exprR->accept(*this);
			node->computed &= node->exprR->computed;
			node->dummy |= node->exprR->dummy;
			if (node->valType != SymType(TInt) || node->exprR->valType != SymType(TInt)) {
				errors->raise(node->op.loc, Err::InvalOpTypes);
				node->dummy = true;
			} if (node->computed) {
				node->computedValue = node->computedValue && node->exprR->computedValue;
			}
		}
		leave(node);
	}

	void visitLOrExp(LOrExp *node) {
		visit(node);
		node->exprL->accept(*this);
		node->computed = node->exprL->computed;
		node->computedValue = node->exprL->computedValue;
		node->valType = node->exprL->valType;
		node->dummy |= node->exprL->dummy;
		if (node->exprR) {
			node->exprR->accept(*this);
			node->computed &= node->exprR->computed;
			node->dummy |= node->exprR->dummy;
			if (node->valType != SymType(TInt) || node->exprR->valType != SymType(TInt)) {
				errors->raise(node->op.loc, Err::InvalOpTypes);
				node->dummy = true;
			} if (node->computed) {
				node->computedValue = node->computedValue || node->exprR->computedValue;
			}
		}
		leave(node);
	}

	void visitConstExp(ConstExp *node) {
		visit(node);
		node->expr->accept(*this);
		node->computed = node->expr->computed;
		node->computedValue = node->expr->computedValue;
		node->valType = node->expr->valType;
		node->dummy |= node->expr->dummy;
		leave(node);
	}
};

}

}

#endif