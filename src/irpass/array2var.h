/*
# array2var
===========

this pass finds array whose elements are used independently
and replace the array with elements, removing gep instructions
*/

#ifndef __CPL_ARRAY2VAR_H__
#define __CPL_ARRAY2VAR_H__

#include <vector>
#include <set>

#include "../ir.h"


namespace IR {

namespace Passes {

using namespace std;
using namespace IR;


class Array2Var : public Pass {
public:
	CFGBuilder cfgBuilder;

	Module *module;

	void visitBasicBlock(BasicBlock *node) {
		for (Inst *inst : node->insts) {
			inst->accept(*this);
		}
	}

	void visitFunction(Function *node) {
		for (BasicBlock *block : node->blocks) {
			block->accept(*this);
		}
	}

	void visitGlobalVar(GlobalVar *node) {
		if (!node->type.isArray) {
			return;
		}
		Value *addr = node->reg;
		int dim = node->type.dimLen;
		set <int> indices;
		for (Use *use : addr->uses) {
			if (use->user == node) {
				continue;
			}
			Inst *inst = (Inst *)use->user;
			if (inst->instType() == TGetPtrInst) {
				if (inst->values.size() < dim + 3) {
					return;
				}
				vector <int> index;
				for (int i = 2; i < inst->values.size(); i++) {
					if (!(*inst)[i]->isConst()) {
						return;
					}
					index.emplace_back((*inst)[i]->getConstValue());
				}
				for (Use *use : (*inst)[0]->uses) {
					Inst *useInst = (Inst *)use->user;
					if (useInst != inst
						&& useInst->instType() != TLoadInst
						&& useInst->instType() != TStoreInst) {
						return;
					}
				}
				int elementIndex = 0;
				for (int i = 0; i <= dim; i++) {
					elementIndex += index[i];
					if (i < dim) {
						elementIndex *= node->type[i];
					}
				}
				if (indices.count(elementIndex)) {
					return;
				}
				indices.insert(elementIndex);
			} else {
				return;
			}
		}

		vector <Inst *> removeInsts;
		for (Use *use : addr->uses) {
			if (use->user == node) {
				continue;
			}
			Inst *inst = (Inst *)use->user;
			if (inst->instType() == TGetPtrInst) {
				Scp::Variable *var = node->var;
				for (int i = 3; i < inst->values.size(); i++) {
					var = (*var)[(*inst)[i]->getConstValue()];
				}
				Scp::Variable *newVar = new Scp::Variable(var->ident, var->symType);
				newVar->init = var->init;
				newVar->initVal = var->initVal;
				Value *reg = (*inst)[0];
				reg->type = var->symType;
				reg->type.toIRType();
				inst->insertBefore(new AllocaInst(reg->type, reg, newVar));
				inst->insertAfter(new StoreInst(reg->type, new NumberLiteral(var->initVal), reg));
				newVar->irValue = reg;
				removeInsts.emplace_back(inst);
			}
		}
		for (Inst *inst : removeInsts) {
			inst->remove();
		}
		module->removeGlobalVar(node);

		module->changed = true;
	}

	void visitAllocaInst(AllocaInst *node) {
		if (!node->type.isArray) {
			return;
		}
		Value *addr = (*node)[0];
		int dim = node->type.dimLen;
		set <int> indices;
		for (Use *use : addr->uses) {
			if (use->user == node) {
				continue;
			}
			Inst *inst = (Inst *)use->user;
			if (inst->instType() == TGetPtrInst) {
				if (inst->values.size() < dim + 3) {
					return;
				}
				vector <int> index;
				for (int i = 2; i < inst->values.size(); i++) {
					if (!(*inst)[i]->isConst()) {
						return;
					}
					index.emplace_back((*inst)[i]->getConstValue());
				}
				for (Use *use : (*inst)[0]->uses) {
					Inst *useInst = (Inst *)use->user;
					if (useInst != inst
						&& useInst->instType() != TLoadInst
						&& useInst->instType() != TStoreInst) {
						return;
					}
				}
				int elementIndex = 0;
				for (int i = 0; i < dim; i++) {
					elementIndex += index[i + 1];
					if (i + 1 < dim) {
						elementIndex *= node->type[i + 1];
					}
				}
				if (indices.count(elementIndex)) {
					return;
				}
				indices.insert(elementIndex);
			} else {
				return;
			}
		}

		vector <Inst *> removeInsts;
		for (Use *use : addr->uses) {
			Inst *inst = (Inst *)use->user;
			if (inst->instType() == TGetPtrInst) {
				Scp::Variable *var = node->var;
				for (int i = 3; i < inst->values.size(); i++) {
					var = (*var)[(*inst)[i]->getConstValue()];
				}
				Scp::Variable *newVar = new Scp::Variable(var->ident, var->symType);
				newVar->init = var->init;
				newVar->initVal = var->initVal;
				Value *reg = (*inst)[0];
				reg->type = var->symType;
				reg->type.toIRType();
				inst->insertBefore(new AllocaInst(reg->type, reg, newVar));
				inst->insertAfter(new StoreInst(reg->type, new NumberLiteral(var->initVal), reg));
				newVar->irValue = reg;
				removeInsts.emplace_back(inst);
			}
		}
		for (Inst *inst : removeInsts) {
			inst->remove();
		}
		node->remove();

		module->changed = true;
	}

	void visitModule(Module *node) {
		module = node;

		for (GlobalVar *globalVar : node->globalVars) {
			globalVar->accept(*this);
		}

		for (Function *func : node->funcs) {
			func->accept(*this);
		}
	}
};

}

}

#endif