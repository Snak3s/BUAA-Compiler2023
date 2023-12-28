/*
# dead code elimination
=======================

this pass removes unreachable functions, unreachable basic blocks,
and the instructions whose defined values are not used
*/

#ifndef __CPL_DCE_H__
#define __CPL_DCE_H__

#include <vector>

#include "../ir.h"
#include "cfgbuilder.h"


namespace IR {

namespace Passes {

using namespace std;
using namespace IR;


class DCE : public Pass {
public:
	CFGBuilder cfgBuilder;

	Module *module;

	void visitBasicBlock(BasicBlock *node) {
		for (auto it = node->insts.rbegin(); it != node->insts.rend(); --it) {
			Inst *inst = *it;
			if (inst->noDef || inst->terminate) {
				continue;
			}
			if (inst->instType() == TCallInst) {
				continue;
			}
			Value *def = inst->values[0]->value;
			if (def->uses.size() == 1) {
				inst->remove();
				module->changed = true;
			}
		}
	}

	map <int, BasicBlock *> reachableBlocks;

	void removePhiEntry(BasicBlock *block, BasicBlock *from) {
		for (Inst *inst : block->insts) {
			if (inst->instType() != TPhiInst) {
				break;
			}
			Use *value = nullptr;
			Use *label = nullptr;
			for (int i = 1; i < inst->values.size(); i += 2) {
				if (inst->values[i + 1]->value == from) {
					value = inst->values[i];
					label = inst->values[i + 1];
					break;
				}
			}
			inst->removeValue(value);
			inst->removeValue(label);
		}
	}

	void reachBlock(BasicBlock *block) {
		if (reachableBlocks.count(block->id) > 0) {
			return;
		}
		reachableBlocks[block->id] = block;
		for (BasicBlock *to : block->jumpTo) {
			reachBlock(to);
		}
	}

	void visitFunction(Function *node) {
		reachableBlocks.clear();
		BasicBlock *entry = node->blocks.first();
		reachBlock(entry);
		for (BasicBlock *block : node->blocks) {
			if (reachableBlocks.count(block->id) == 0) {
				for (BasicBlock *to : block->jumpTo) {
					removePhiEntry(to, block);
				}
				block->destroy();
				node->remove(block);
				module->changed = true;
			}
		}

		for (auto it = node->blocks.rbegin(); it != node->blocks.rend(); --it) {
			BasicBlock *block = *it;
			block->accept(*this);
		}
	}

	map <int, Function *> reachableFuncs;

	void reachFunc(Function *func) {
		if (reachableFuncs.count(func->id) > 0) {
			return;
		}
		reachableFuncs[func->id] = func;
		for (Function *callee : func->asCaller) {
			reachFunc(callee);
		}
	}

	void visitModule(Module *node) {
		node->accept(cfgBuilder);

		module = node;
		reachableFuncs.clear();
		Function *mainFunc = node->funcs.last();
		reachFunc(mainFunc);
		for (Function *func : node->funcs) {
			if (reachableFuncs.count(func->id) == 0) {
				func->destroy();
				node->removeFunc(func);
				module->changed = true;
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