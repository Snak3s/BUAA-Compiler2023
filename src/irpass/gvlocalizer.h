/*
# global variable localizer
===========================

this pass finds the global i32 variable used only in main and localizes it
*/

#ifndef __CPL_GV_LOCALIZER_H__
#define __CPL_GV_LOCALIZER_H__

#include <vector>
#include <set>

#include "../ir.h"


namespace IR {

namespace Passes {

using namespace std;
using namespace IR;


class GVLocalizer : public Pass {
public:
	Module *module;
	Function *main = nullptr;

	void replaceReg(Value *oldReg, Value *newReg) {
		for (Use *use : oldReg->uses) {
			use->user->setValue(use->index, newReg);
		}
	}

	void localize(GlobalVar *globalVar) {
		module->changed = true;
		module->removeGlobalVar(globalVar);
		BasicBlock *mainEntry = main->blocks.first();
		Value *reg = new Value(globalVar->type);
		globalVar->var->irValue = reg;
		AllocaInst *alloca = new AllocaInst(globalVar->type, reg, globalVar->var);
		alloca->block = mainEntry;
		StoreInst *store = new StoreInst(globalVar->type, new NumberLiteral(globalVar->var->get()), reg);
		store->block = mainEntry;
		mainEntry->prepend(store);
		mainEntry->prepend(alloca);
		replaceReg(globalVar->reg, reg);
		globalVar->destroy();
	}

	void visitModule(Module *node) {
		module = node;
		main = node->funcs.last();

		for (GlobalVar *globalVar : node->globalVars) {
			if (globalVar->type != Int32) {
				continue;
			}
			if (globalVar->var->symType.isConst) {
				continue;
			}
			Value *reg = globalVar->reg;
			bool inMain = true;
			for (Use *use : reg->uses) {
				if (use->user == globalVar) {
					continue;
				}
				Inst *inst = (Inst *)use->user;
				if (inst->block->func != main) {
					inMain = false;
					break;
				}
			}
			if (inMain) {
				localize(globalVar);
			}
		}
	}
};

}

}

#endif