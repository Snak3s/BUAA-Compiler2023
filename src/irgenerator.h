#ifndef __CPL_IR_GENERATOR_H__
#define __CPL_IR_GENERATOR_H__

#include <vector>

#include "config.h"
#include "ast.h"
#include "astvisitor.h"
#include "ir.h"
#include "scope.h"
#include "lexer.h"


namespace AST {

namespace Visitors {

using namespace std;
using namespace AST;
using namespace Lex;
using namespace IR;
using IR::Function;


class IRGenerator : public Visitor {
public:
	Module *module;
	Function *curFunc = nullptr;
	BasicBlock *curBlock = nullptr;
	User *curVar = nullptr;
	Value *curReg = nullptr;
	Inst *curInst = nullptr;
	Scp::Scope *globalScope = nullptr;

	BasicBlock *trueBranch = nullptr;
	BasicBlock *falseBranch = nullptr;

	IRGenerator(Module *module = new Module()) {
		this->module = module;
	}


	void appendInst(Inst *inst) {
		if (!curBlock) {
			curBlock = curFunc->allocBasicBlock();
		}
		if (lastInst() && lastInst()->terminate) {
			curBlock = curFunc->allocBasicBlockAfter(curBlock);
		}
		curBlock->append(inst);
		inst->block = curBlock;
	}

	Inst *lastInst() {
		if (!curBlock->insts.empty()) {
			return curBlock->insts.last();
		}
		return nullptr;
	}

	BasicBlock *appendBlock() {
		if (!curBlock) {
			curBlock = curFunc->allocBasicBlock();
		}
		return curFunc->allocBasicBlockAfter(curBlock);
	}


	void visitCompUnit(CompUnit *node) {
		globalScope = node->scope;
		for (Node *decl : node->decl) {
			decl->accept(*this);
		}
		for (Node *func : node->func) {
			func->accept(*this);
		}
		node->mainFunc->accept(*this);
	}

	void visitConstDecl(ConstDecl *node) {
		for (Node *def : node->constDef) {
			def->accept(*this);
		}
	}

	void visitConstDef(ConstDef *node) {
		Scp::Variable *var = node->scope->getVar(node->ident);
		GlobalVar *globalVar = nullptr;
		if (node->scope == globalScope) {
			globalVar = new GlobalVar(var);
		} else {
			string name("@.scope." + to_string(node->scope->id) + "." + var->ident.token);
			globalVar = new GlobalVar(var, name);
		}
		module->appendGlobalVar(globalVar);
		curVar = globalVar;
		node->initVal->accept(*this);
	}

	void visitConstInitVal(ConstInitVal *node) {
		if (node->isArray) {
			for (Node *initVal : node->initVal) {
				initVal->accept(*this);
			}
		} else {
			curVar->appendValue(new NumberLiteral(node->expr->computedValue));
		}
	}

	void visitVarDecl(VarDecl *node) {
		for (Node *def : node->varDef) {
			def->accept(*this);
		}
	}

	vector <int> initValDim;

	void visitVarDef(VarDef *node) {
		Scp::Variable *var = node->scope->getVar(node->ident);
		if (node->scope == globalScope) {
			GlobalVar *globalVar = new GlobalVar(var);
			module->appendGlobalVar(globalVar);
			curVar = globalVar;
		} else {
			Value *reg = new Value(var->symType);
			var->irValue = reg;
			curReg = reg;
			appendInst(new AllocaInst(reg->type, reg, var));
		}
		if (node->initVal) {
			initValDim.clear();
			node->initVal->accept(*this);
		}
	}

	void visitInitVal(InitVal *node) {
		if (node->scope == globalScope) {
			if (node->isArray) {
				for (Node *initVal : node->initVal) {
					initVal->accept(*this);
				}
			} else {
				curVar->appendValue(new NumberLiteral(node->expr->computedValue));
			}
		} else {
			if (node->isArray) {
				int dim = 0;
				for (Node *initVal : node->initVal) {
					initValDim.emplace_back(dim);
					initVal->accept(*this);
					initValDim.pop_back();
					dim++;
				}
			} else {
				if (node->expr->computed && CPL_IR_UseComputedValue) {
					node->value = new NumberLiteral(node->expr->computedValue);
					node->valType = Int32;
				} else {
					node->expr->accept(*this);
					node->value = node->expr->value;
					node->valType = node->expr->valType;
				}
				if (initValDim.size() == 0) {
					appendInst(new StoreInst(node->valType, node->value, curReg));
				} else {
					Value *addr = new Value(node->valType);
					GetPtrInst *inst = new GetPtrInst(curReg->type, addr, curReg);
					for (int dim : initValDim) {
						inst->appendValue(new NumberLiteral(dim));
					}
					appendInst(inst);
					appendInst(new StoreInst(node->valType, node->value, addr));
				}
			}
		}
	}

	void visitFuncDef(FuncDef *node) {
		Scp::Function *defFunc = node->scope->getFunc(node->ident);
		Function *func = new Function(defFunc);
		module->appendFunc(func);
		curFunc = func;
		curBlock = nullptr;
		if (node->params) {
			node->params->accept(*this);
		}
		node->block->accept(*this);
		if (!curBlock || curBlock->insts.empty() || !curBlock->insts.last()->terminate) {
			appendInst(new RetInst());
		}
	}

	void visitMainFuncDef(MainFuncDef *node) {
		Scp::Function *mainFunc = node->scope->getFunc(node->ident);
		Function *func = new Function(mainFunc);
		module->appendFunc(func);
		curFunc = func;
		curBlock = nullptr;
		node->block->accept(*this);
	}

	void visitFuncFParams(FuncFParams *node) {
		for (Node *param : node->params) {
			param->accept(*this);
		}
	}

	void visitFuncFParam(FuncFParam *node) {
		node->symType.toIRType();
		Value *param = new Value(node->symType);
		Scp::Variable *var = curFunc->func->scope->getVar(node->ident);
		var->irValue = param;
		curFunc->appendParam(param);
		if (!node->symType.isPointer) {
			Value *reg = new Value(node->symType);
			var->irValue = reg;
			appendInst(new AllocaInst(node->symType, reg, var));
			appendInst(new StoreInst(node->symType, param, reg));
		}
	}

	void visitBlock(Block *node) {
		for (Node *item : node->items) {
			item->accept(*this);
		}
	}

	void visitStmtAssign(StmtAssign *node) {
		node->lVal->accept(*this);
		node->expr->accept(*this);
		appendInst(new StoreInst(node->lVal->valType, node->expr->value, node->lVal->value));
	}

	void visitStmtExp(StmtExp *node) {
		if (!node->expr) {
			return;
		}
		if (node->expr->computed) {
			return;
		}
		node->expr->accept(*this);
	}

	void visitStmtBlock(StmtBlock *node) {
		node->block->accept(*this);
	}

	void visitStmtIf(StmtIf *node) {
		BasicBlock *endBlock = appendBlock();
		BasicBlock *elseBlock = nullptr;
		if (node->elseStmt) {
			elseBlock = appendBlock();
			falseBranch = elseBlock;
		} else {
			falseBranch = endBlock;
		}
		BasicBlock *thenBlock = appendBlock();
		trueBranch = thenBlock;
		node->cond->accept(*this);
		curBlock = thenBlock;
		node->thenStmt->accept(*this);
		if (!lastInst() || !lastInst()->terminate) {
			appendInst(new BrInst(endBlock));
		}
		if (node->elseStmt) {
			curBlock = elseBlock;
			node->elseStmt->accept(*this);
			if (!lastInst() || !lastInst()->terminate) {
				appendInst(new BrInst(endBlock));
			}
		}
		curBlock = endBlock;
	}

	BasicBlock *breakEntry = nullptr;
	BasicBlock *continueEntry = nullptr;

	void visitStmtFor(StmtFor *node) {
		BasicBlock *endBlock = appendBlock();
		BasicBlock *stepBlock = nullptr;
		if (node->stepStmt) {
			stepBlock = appendBlock();
		}
		BasicBlock *stmtBlock = appendBlock();
		BasicBlock *condBlock = nullptr;
		if (node->cond) {
			condBlock = appendBlock();
		}
		BasicBlock *condEntry = condBlock ? condBlock : stmtBlock;
		BasicBlock *stepEntry = stepBlock ? stepBlock : condEntry;
		BasicBlock *lastBreakEntry = breakEntry;
		breakEntry = endBlock;
		BasicBlock *lastContinueEntry = continueEntry;
		continueEntry = stepEntry;
		if (node->initStmt) {
			node->initStmt->accept(*this);
		}
		appendInst(new BrInst(condEntry));
		if (node->cond) {
			curBlock = condBlock;
			trueBranch = stmtBlock;
			falseBranch = endBlock;
			node->cond->accept(*this);
		}
		curBlock = stmtBlock;
		node->stmt->accept(*this);
		appendInst(new BrInst(stepEntry));
		if (node->stepStmt) {
			curBlock = stepBlock;
			node->stepStmt->accept(*this);
			appendInst(new BrInst(condEntry));
		}
		curBlock = endBlock;
		breakEntry = lastBreakEntry;
		continueEntry = lastContinueEntry;
	}

	void visitStmtBreak(StmtBreak *node) {
		appendInst(new BrInst(breakEntry));
	}

	void visitStmtContinue(StmtContinue *node) {
		appendInst(new BrInst(continueEntry));
	}

	void visitStmtReturn(StmtReturn *node) {
		if (node->expr) {
			node->expr->accept(*this);
			appendInst(new RetInst(node->expr->valType, node->expr->value));
		} else {
			appendInst(new RetInst());
		}
	}

	void visitStmtGetInt(StmtGetInt *node) {
		node->lVal->accept(*this);
		Value *reg = new Value(Int32);
		appendInst(new CallInst(Int32, reg, module->getint));
		appendInst(new StoreInst(node->lVal->valType, reg, node->lVal->value));
	}

	void printString(const string &constString) {
		static int constStringCnt = 0;
		if (constString.length() <= 0) {
			return;
		}
		if (constString.length() <= CPL_IR_PrintStrMinLength) {
			for (char c : constString) {
				CallInst *printChar = new CallInst(Void, module->putch);
				printChar->appendValue(new NumberLiteral((int)c));
				appendInst(printChar);
			}
			return;
		}
		StringLiteral *str = new StringLiteral(constString);
		str->name = "@.printf_str." + to_string(constStringCnt++);
		GlobalVar *globalVar = new GlobalVar(str);
		module->appendGlobalVar(globalVar);
		Value *addr = new Value(str->type);
		GetPtrInst *getPtr = new GetPtrInst(str->type, addr, globalVar->reg);
		getPtr->appendValue(new NumberLiteral(0));
		appendInst(getPtr);
		CallInst *printStr = new CallInst(Void, module->putstr);
		printStr->appendValue(addr);
		appendInst(printStr);
	}

	void visitStmtPrintf(StmtPrintf *node) {
		for (int i = 0; i < node->params.size(); i++) {
			node->params[i]->accept(*this);
		}
		for (int i = 0; i < node->params.size(); i++) {
			printString(node->constString[i]);
			CallInst *printValue = new CallInst(Void, module->putint);
			printValue->appendValue(node->params[i]->value);
			appendInst(printValue);
		}
		printString(node->constString[node->params.size()]);
	}

	void visitForStmt(ForStmt *node) {
		node->lVal->accept(*this);
		node->expr->accept(*this);
		appendInst(new StoreInst(node->lVal->valType, node->expr->value, node->lVal->value));
	}

	void visitExp(Exp *node) {
		node->valType.toIRType();
		if (node->computed && CPL_IR_UseComputedValue) {
			node->value = new NumberLiteral(node->computedValue);
			return;
		}
		node->expr->accept(*this);
		node->value = node->expr->value;
	}

	void visitCond(Cond *node) {
		node->valType.toIRType();
		if (node->computed && CPL_IR_UseComputedValue) {
			if (node->computedValue) {
				appendInst(new BrInst(trueBranch));
			} else {
				appendInst(new BrInst(falseBranch));
			}
			return;
		}
		node->expr->accept(*this);
	}

	void visitLVal(LVal *node) {
		node->valType.toIRType();
		Scp::Variable *var = node->var;
		if (node->dimExp.size() == 0 && !var->symType.isArray) {
			node->value = var->irValue;
			return;
		}
		node->value = new Value(node->valType);
		GetPtrInst *inst = new GetPtrInst(var->irValue->type, node->value, var->irValue);
		for (Node *dimExp : node->dimExp) {
			dimExp->accept(*this);
			inst->appendValue(dimExp->value);
		}
		if (node->valType.isArray) {
			inst->appendValue(new NumberLiteral(0));
		}
		appendInst(inst);
	}

	void visitPrimaryExp(PrimaryExp *node) {
		node->valType.toIRType();
		if (node->computed && CPL_IR_UseComputedValue) {
			node->value = new NumberLiteral(node->computedValue);
			return;
		}
		Node *child = nullptr;
		if (node->expr) {
			child = node->expr;
		} else if (node->number) {
			child = node->number;
		} else if (node->lVal) {
			child = node->lVal;
		}
		child->accept(*this);
		node->value = child->value;
		if (node->lVal && !node->valType.isArray) {
			node->value = new Value(node->valType);
			appendInst(new LoadInst(node->valType, node->value, child->value));
		}
	}

	void visitNumber(Number *node) {
		node->valType.toIRType();
		node->value = new NumberLiteral(node->computedValue);
	}

	void visitUnaryExp(UnaryExp *node) {
		node->valType.toIRType();
		if (node->funcCall) {
			CallInst *inst = nullptr;
			Scp::Function *func = node->scope->getFunc(node->ident);
			if (node->valType != Void) {
				node->value = new Value(node->valType);
				inst = new CallInst(node->valType, node->value, func->irFunc);
			} else {
				node->value = new NumberLiteral(UNAVAILABLE);
				inst = new CallInst(node->valType, func->irFunc);
			}
			if (node->params) {
				Inst *lastInst = curInst;
				curInst = inst;
				node->params->accept(*this);
				curInst = lastInst;
			}
			appendInst(inst);
		} else {
			if (node->computed && CPL_IR_UseComputedValue) {
				node->value = new NumberLiteral(node->computedValue);
				return;
			}
			node->expr->accept(*this);
			node->value = node->expr->value;
			if (node->op) {
				UnaryOp *op = (UnaryOp *)node->op;
				if (op->op != PLUS) {
					node->value = new Value(node->valType);
					if (op->op == MINU) {
						appendInst(new SubInst(node->valType, node->value, new NumberLiteral(0), node->expr->value));
					} else if (op->op == NOT) {
						Value *value = new Value(Int1);
						appendInst(new IcmpInst(node->valType, CondEq, value, new NumberLiteral(0), node->expr->value));
						appendInst(new ZextInst(Int1, node->valType, node->value, value));
					}
				}
			}
		}
	}

	void visitFuncRParams(FuncRParams *node) {
		for (Node *param : node->params) {
			param->accept(*this);
			curInst->appendValue(param->value);
		}
	}

	void visitMulExp(MulExp *node) {
		node->valType.toIRType();
		if (node->computed && CPL_IR_UseComputedValue) {
			node->value = new NumberLiteral(node->computedValue);
			return;
		}
		node->exprL->accept(*this);
		node->value = node->exprL->value;
		if (node->exprR) {
			node->exprR->accept(*this);
			node->value = new Value(node->valType);
			if (node->op == MULT) {
				appendInst(new MulInst(node->valType, node->value, node->exprL->value, node->exprR->value));
			} else if (node->op == DIV) {
				appendInst(new SdivInst(node->valType, node->value, node->exprL->value, node->exprR->value));
			} else if (node->op == MOD) {
				appendInst(new SremInst(node->valType, node->value, node->exprL->value, node->exprR->value));
			}
		}
	}

	void visitAddExp(AddExp *node) {
		node->valType.toIRType();
		if (node->computed && CPL_IR_UseComputedValue) {
			node->value = new NumberLiteral(node->computedValue);
			return;
		}
		node->exprL->accept(*this);
		node->value = node->exprL->value;
		if (node->exprR) {
			node->exprR->accept(*this);
			node->value = new Value(node->valType);
			if (node->op == PLUS) {
				appendInst(new AddInst(node->valType, node->value, node->exprL->value, node->exprR->value));
			} else if (node->op == MINU) {
				appendInst(new SubInst(node->valType, node->value, node->exprL->value, node->exprR->value));
			}
		}
	}

	void visitRelExp(RelExp *node) {
		node->valType.toIRType();
		if (node->computed && CPL_IR_UseComputedValue) {
			node->value = new NumberLiteral(node->computedValue);
			return;
		}
		node->exprL->accept(*this);
		node->value = node->exprL->value;
		if (node->exprR) {
			node->exprR->accept(*this);
			node->value = new Value(node->valType);
			Value *value = new Value(Int1);
			if (node->op == LSS) {
				appendInst(new IcmpInst(node->valType, CondSlt, value, node->exprL->value, node->exprR->value));
			} else if (node->op == GRE) {
				appendInst(new IcmpInst(node->valType, CondSgt, value, node->exprL->value, node->exprR->value));
			} else if (node->op == LEQ) {
				appendInst(new IcmpInst(node->valType, CondSle, value, node->exprL->value, node->exprR->value));
			} else if (node->op == GEQ) {
				appendInst(new IcmpInst(node->valType, CondSge, value, node->exprL->value, node->exprR->value));
			}
			appendInst(new ZextInst(Int1, node->valType, node->value, value));
		}
	}

	void visitEqExp(EqExp *node) {
		node->valType.toIRType();
		if (node->computed && CPL_IR_UseComputedValue) {
			node->value = new NumberLiteral(node->computedValue);
			return;
		}
		node->exprL->accept(*this);
		node->value = node->exprL->value;
		if (node->exprR) {
			node->exprR->accept(*this);
			node->value = new Value(node->valType);
			Value *value = new Value(Int1);
			if (node->op == EQL) {
				appendInst(new IcmpInst(node->valType, CondEq, value, node->exprL->value, node->exprR->value));
			} else if (node->op == NEQ) {
				appendInst(new IcmpInst(node->valType, CondNe, value, node->exprL->value, node->exprR->value));
			}
			appendInst(new ZextInst(Int1, node->valType, node->value, value));
		}
	}

	void visitLAndExp(LAndExp *node) {
		node->valType.toIRType();
		if (node->computed && CPL_IR_UseComputedValue) {
			if (node->computedValue) {
				appendInst(new BrInst(trueBranch));
			} else {
				appendInst(new BrInst(falseBranch));
			}
			return;
		}
		BasicBlock *expr = nullptr;
		if (node->exprR) {
			expr = trueBranch;
			trueBranch = appendBlock();
		}
		node->exprL->accept(*this);
		node->value = node->exprL->value;
		if (node->exprR) {
			curBlock = trueBranch;
			trueBranch = expr;
			node->exprR->accept(*this);
			node->value = node->exprR->value;
		}
		if (node->value) {
			Value *cond = new Value(Int1);
			appendInst(new IcmpInst(node->valType, CondNe, cond, new NumberLiteral(0), node->value));
			appendInst(new BrInst(cond, trueBranch, falseBranch));
		}
		node->value = nullptr;
	}

	void visitLOrExp(LOrExp *node) {
		node->valType.toIRType();
		if (node->computed && CPL_IR_UseComputedValue) {
			if (node->computedValue) {
				appendInst(new BrInst(trueBranch));
			} else {
				appendInst(new BrInst(falseBranch));
			}
			return;
		}
		BasicBlock *expr = nullptr;
		if (node->exprR) {
			expr = falseBranch;
			falseBranch = appendBlock();
		}
		node->exprL->accept(*this);
		node->value = node->exprL->value;
		if (node->exprR) {
			curBlock = falseBranch;
			falseBranch = expr;
			node->exprR->accept(*this);
			node->value = node->exprR->value;
		}
		if (node->value) {
			Value *cond = new Value(Int1);
			appendInst(new IcmpInst(node->valType, CondNe, cond, new NumberLiteral(0), node->value));
			appendInst(new BrInst(cond, trueBranch, falseBranch));
		}
		node->value = nullptr;
	}

	void visitConstExp(ConstExp *node) {
		node->valType.toIRType();
		node->value = new NumberLiteral(node->computedValue);
	}
};

}

}

#endif