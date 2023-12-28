/*
# aggressive dead code elimination
==================================

*/

#ifndef __CPL_AGGRESSIVE_DCE_H__
#define __CPL_AGGRESSIVE_DCE_H__

#include <vector>
#include <set>

#include "../ir.h"
#include "cfgbuilder.h"


namespace IR {

namespace Passes {

using namespace std;
using namespace IR;


class AggressiveDCE : public Pass {
public:
	CFGBuilder cfgBuilder;

	Module *module;

	set <Value *> reservedValues;
	set <BasicBlock *> reservedBlocks;

	void markReservedBlock(BasicBlock *block) {
		if (reservedBlocks.count(block)) {
			return;
		}
		reservedBlocks.insert(block);
		Inst *lastInst = block->insts.last();
		for (int i = 0; i < lastInst->values.size(); i++) {
			Value *value = (*lastInst)[i];
			markReserved(value);
		}
		// to do: should be domainance frontiers in cdg
		for (BasicBlock *from : block->jumpFrom) {
			markReservedBlock(from);
		}
	}

	void markReserved(Value *value) {
		if (value->regId == UNAVAILABLE) {
			return;
		}
		if (reservedValues.count(value)) {
			return;
		}
		reservedValues.insert(value);
		for (Use *use : value->uses) {
			if (use->index > 0) {
				continue;
			}
			Inst *inst = (Inst *)use->user;
			if (inst->noDef) {
				continue;
			}
			for (int i = 0; i < inst->values.size(); i++) {
				Value *value = (*inst)[i];
				markReserved(value);
			}
			markReservedBlock(inst->block);
		}
	}

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

	void visitFunction(Function *node) {
		reservedValues.clear();
		reservedBlocks.clear();

		reservedBlocks.insert(node->blocks.first());

		for (auto blockIt = node->blocks.rbegin(); blockIt != node->blocks.rend(); --blockIt) {
			BasicBlock *block = *blockIt;
			for (auto instIt = block->insts.rbegin(); instIt != block->insts.rend(); --instIt) {
				Inst *inst = *instIt;
				if (inst->instType() == TCallInst
					|| inst->instType() == TRetInst
					|| inst->instType() == TStoreInst
					|| inst->instType() == TBrInst) {
					for (int i = 0; i < inst->values.size(); i++) {
						Value *value = (*inst)[i];
						markReserved(value);
					}
				}
			}
		}

		for (auto blockIt = node->blocks.rbegin(); blockIt != node->blocks.rend(); --blockIt) {
			BasicBlock *block = *blockIt;
			for (auto instIt = block->insts.rbegin(); instIt != block->insts.rend(); --instIt) {
				Inst *inst = *instIt;
				if (!inst->noDef && !inst->terminate) {
					if (reservedValues.count((*inst)[0]) == 0) {
						inst->remove();
						module->changed = true;
					}
				}
			}
		}
	}

	void visitModule(Module *node) {
		node->accept(cfgBuilder);

		module = node;

		for (Function *func : node->funcs) {
			func->accept(*this);
		}
	}
};

}

}

#endif