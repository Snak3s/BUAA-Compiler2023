/*
# function evaluation
=====================

this pass evaluates const-expr-like functions
*/

#ifndef __CPL_FUNC_EVAL_H__
#define __CPL_FUNC_EVAL_H__

#include <vector>
#include <algorithm>

#include "../ir.h"
#include "cfgbuilder.h"


namespace IR {

namespace Passes {

using namespace std;
using namespace IR;


class FuncEval : public Pass {
public:
	CFGBuilder cfgBuilder;

	Module *module;
	Function *curFunc = nullptr;


	map <Function *, bool> evaluable;
	vector <map <Value *, int>> recursionStack;


	const int execCountLimit = 1 << 16;
	const int recursionLimit = 1 << 10;
	int execCount = 0;
	int maxRecursionDepth = 0;
	int retValue = 0;


	void replaceReg(Value *oldReg, Value *newReg) {
		for (Use *use : oldReg->uses) {
			use->user->setValue(use->index, newReg);
		}
	}


	void allocStack() {
		recursionStack.emplace_back(map <Value *, int> ());
	}

	void popStack() {
		recursionStack.pop_back();
	}

	void setValue(Value *var, int value) {
		recursionStack.back()[var] = value;
	}

	int getValue(Value *var) {
		if (var->isConst()) {
			return var->getConstValue();
		}
		return recursionStack.back()[var];
	}


	BasicBlock *lastBlock = nullptr;

	bool eval(Function *func) {
		lastBlock = nullptr;
		maxRecursionDepth = max(maxRecursionDepth, (int)recursionStack.size());
		if (maxRecursionDepth > recursionLimit) {
			popStack();
			return false;
		}
		func->blocks.first()->accept(*this);
		popStack();
		if (execCount > execCountLimit) {
			return false;
		}
		return true;
	}

	void visitBasicBlock(BasicBlock *node) {
		map <Value *, int> phiValue;
		for (Inst *inst : node->insts) {
			if (inst->instType() != TPhiInst) {
				break;
			}
			for (int i = 1; i < inst->values.size(); i += 2) {
				if ((*inst)[i + 1] == lastBlock) {
					phiValue[(*inst)[0]] = getValue((*inst)[i]);
				}
			}
		}
		for (Inst *inst : node->insts) {
			if (inst->instType() != TPhiInst) {
				break;
			}
			setValue((*inst)[0], phiValue[(*inst)[0]]);
		}
		for (Inst *inst : node->insts) {
			if (++execCount > execCountLimit) {
				return;
			}
			if (maxRecursionDepth > recursionLimit) {
				return;
			}
			if (inst->instType() == TPhiInst) {
				continue;
			}
			inst->accept(*this);
		}
	}

	void visitAddInst(AddInst *node) {
		setValue((*node)[0], getValue((*node)[1]) + getValue((*node)[2]));
	}

	void visitSubInst(SubInst *node) {
		setValue((*node)[0], getValue((*node)[1]) - getValue((*node)[2]));
	}

	void visitMulInst(MulInst *node) {
		setValue((*node)[0], getValue((*node)[1]) * getValue((*node)[2]));
	}

	void visitSdivInst(SdivInst *node) {
		if (getValue((*node)[2]) == 0) {
			setValue((*node)[0], 0);
			return;
		}
		setValue((*node)[0], getValue((*node)[1]) / getValue((*node)[2]));
	}

	void visitSremInst(SremInst *node) {
		if (getValue((*node)[2]) == 0) {
			setValue((*node)[0], 0);
			return;
		}
		setValue((*node)[0], getValue((*node)[1]) % getValue((*node)[2]));
	}

	void visitIcmpInst(IcmpInst *node) {
		if (node->cond == CondEq) {
			setValue((*node)[0], getValue((*node)[1]) == getValue((*node)[2]));
		} else if (node->cond == CondNe) {
			setValue((*node)[0], getValue((*node)[1]) != getValue((*node)[2]));
		} else if (node->cond == CondSgt) {
			setValue((*node)[0], getValue((*node)[1]) > getValue((*node)[2]));
		} else if (node->cond == CondSge) {
			setValue((*node)[0], getValue((*node)[1]) >= getValue((*node)[2]));
		} else if (node->cond == CondSlt) {
			setValue((*node)[0], getValue((*node)[1]) < getValue((*node)[2]));
		} else if (node->cond == CondSle) {
			setValue((*node)[0], getValue((*node)[1]) <= getValue((*node)[2]));
		}
	}

	void visitCallInst(CallInst *node) {
		if (node->noDef) {
			return;
		}
		vector <int> params;
		for (int i = 2; i < node->values.size(); i++) {
			params.emplace_back(getValue((*node)[i]));
		}
		allocStack();
		Function *callee = (Function *)((*node)[1]);
		for (int i = 0; i < callee->params.size(); i++) {
			setValue(callee->params[i], params[i]);
		}
		if (!eval(callee)) {
			return;
		}
		setValue((*node)[0], retValue);
	}

	void visitZextInst(ZextInst *node) {
		setValue((*node)[0], getValue((*node)[1]));
	}

	void visitTruncInst(TruncInst *node) {
		setValue((*node)[0], getValue((*node)[1]));
	}

	void visitBrInst(BrInst *node) {
		lastBlock = node->block;
		if (node->hasCond) {
			if (getValue((*node)[0])) {
				(*node)[1]->accept(*this);
			} else {
				(*node)[2]->accept(*this);
			}
		} else {
			(*node)[0]->accept(*this);
		}
	}

	void visitRetInst(RetInst *node) {
		if (node->values.size() == 0) {
			return;
		}
		retValue = getValue((*node)[0]);
	}


	void visitFunction(Function *node) {
		curFunc = node;
		for (BasicBlock *block : node->blocks) {
			for (Inst *inst : block->insts) {
				if (inst->instType() != TCallInst) {
					continue;
				}
				Function *callee = nullptr;
				if (inst->noDef) {
					callee = (Function *)((*inst)[0]);
				} else {
					callee = (Function *)((*inst)[1]);
				}
				if (evaluable[callee]) {
					if (inst->noDef) {
						inst->remove();
						module->changed = true;
						continue;
					}
					bool isConst = true;
					for (int i = 2; i < inst->values.size(); i++) {
						isConst &= (*inst)[i]->isConst();
					}
					if (!isConst) {
						continue;
					}
					execCount = 0;
					maxRecursionDepth = 0;
					allocStack();
					for (int i = 2; i < inst->values.size(); i++) {
						setValue(callee->params[i - 2], (*inst)[i]->getConstValue());
					}
					if (!eval(callee)) {
						continue;
					}
					replaceReg((*inst)[0], new NumberLiteral(retValue));
					inst->remove();
					module->changed = true;
				}
			}
		}
	}

	void visitModule(Module *node) {
		node->accept(cfgBuilder);

		module = node;

		for (Function *func : node->funcs) {
			if (func->reserved) {
				continue;
			}
			evaluable[func] = true;
			for (Function *callee : func->asCaller) {
				if (callee->reserved) {
					evaluable[func] = false;
					continue;
				}
				evaluable[func] &= evaluable[callee];
			}
			for (BasicBlock *block : func->blocks) {
				for (Inst *inst : block->insts) {
					if (!evaluable[func]) {
						break;
					}
					if (inst->instType() == TLoadInst
						|| inst->instType() == TStoreInst) {
						evaluable[func] = false;
					}
				}
			}
		}

		for (Function *func : node->funcs) {
			func->accept(*this);
		}
	}
};

}

}

#endif