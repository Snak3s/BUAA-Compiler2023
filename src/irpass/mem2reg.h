/*
# mem2reg
=========

this pass removes local i32 alloca instructions
and inserts phi instructions for them

unreachable functions and basic blocks are removed
*/

#ifndef __CPL_MEM2REG_H__
#define __CPL_MEM2REG_H__

#include <vector>
#include <map>
#include <set>

#include "../ir.h"
#include "../bitmask.h"
#include "domanalyzer.h"


namespace IR {

namespace Passes {

using namespace std;
using namespace IR;
using namespace Bits;


class Mem2Reg : public Pass {
public:
	DomAnalyzer domAnalyzer;

	Module *module;
	Function *curFunc = nullptr;
	map <int, int> index;

	vector <BasicBlock *> blocks;

	vector <AllocaInst *> varAllocs;
	set <int> varAddrId;
	map <int, BasicBlock *> regDefBlock;
	map <int, Value *> reachingDef;
	map <int, Value *> replace;

	set <Inst *> insertedPhis;

	Value *getReachingDef(int id, BasicBlock *u) {
		Value *ret = reachingDef[id];
		while (ret != nullptr && domAnalyzer.dom[u].count(regDefBlock[ret->id]) == 0) {
			ret = reachingDef[ret->id];
		}
		reachingDef[id] = ret;
		return ret;
	}

	Value *getReplaced(Value *value) {
		if (value == nullptr) {
			return nullptr;
		}
		if (replace.count(value->id)) {
			return replace[value->id];
		}
		return value;
	}

	void removeMemInsts(BasicBlock *block) {
		for (Inst *inst : block->insts) {
			if (inst->instType() == TAllocaInst) {
				int addrId = inst->values[0]->value->id;
				if (varAddrId.count(addrId) == 0) {
					continue;
				}
				inst->remove();
				continue;
			}
			if (inst->instType() == TLoadInst) {
				int regId = inst->values[0]->value->id;
				int addrId = inst->values[1]->value->id;
				if (varAddrId.count(addrId)) {
					Value *def = getReachingDef(addrId, block);
					reachingDef[regId] = def;
					if (def == nullptr || def == inst->values[1]->value) {
						Value *value = new NumberLiteral(0);
						reachingDef[regId] = value;
						regDefBlock[value->id] = block;
					}
					regDefBlock[regId] = nullptr;
					inst->remove();
					continue;
				}
			}
			if (inst->instType() == TStoreInst) {
				int regId = inst->values[0]->value->id;
				int addrId = inst->values[1]->value->id;
				if (varAddrId.count(addrId)) {
					Value *def = getReplaced(getReachingDef(regId, block));
					if (def == nullptr) {
						def = inst->values[0]->value;
					}
					Value *reg = new Value(inst->values[0]->value->type);
					regId = reg->id;
					replace[regId] = def;
					reachingDef[regId] = getReachingDef(addrId, block);
					regDefBlock[regId] = block;
					reachingDef[addrId] = reg;
					inst->remove();
					continue;
				}
			}
			if (inst->instType() == TPhiInst && insertedPhis.count(inst)) {
				PhiInst *phi = (PhiInst *)inst;
				int regId = phi->values[0]->value->id;
				int addrId = phi->var->irValue->id;
				if (varAddrId.count(addrId)) {
					reachingDef[regId] = getReachingDef(addrId, block);
					regDefBlock[regId] = block;
					reachingDef[addrId] = phi->values[0]->value;
					continue;
				}
			}
			for (Use *use : inst->values) {
				if (reachingDef.count(use->value->id)) {
					Value *def = getReplaced(getReachingDef(use->value->id, block));
					if (def) {
						use->setValue(def);
					}
				}
			}
		}
		for (BasicBlock *to : block->jumpTo) {
			for (Inst *inst : to->insts) {
				if (inst->instType() != TPhiInst) {
					break;
				}
				PhiInst *phi = (PhiInst *)inst;
				if (insertedPhis.count(inst) == 0) {
					continue;
				}
				Value *def = getReplaced(getReachingDef(phi->var->irValue->id, block));
				if (def) {
					phi->appendValue(def);
					phi->appendValue(block);
				} else {
					phi->appendValue(new NumberLiteral(0));
					phi->appendValue(block);
				}
			}
		}

		for (BasicBlock *child : domAnalyzer.domChildren[block]) {
			removeMemInsts(child);
		}
	}

	void visitFunction(Function *node) {
		curFunc = node;
		index.clear();
		int cnt = 0;
		blocks.clear();
		for (BasicBlock *block : node->blocks) {
			index[block->id] = cnt++;
			blocks.emplace_back(block);
		}

		// insert phi functions
		varAllocs.clear();
		varAddrId.clear();
		insertedPhis.clear();
		for (BasicBlock *block : node->blocks) {
			for (Inst *inst : block->insts) {
				if (inst->instType() != TAllocaInst) {
					continue;
				}
				if (inst->type.isArray) {
					continue;
				}
				AllocaInst *alloca = (AllocaInst *)inst;
				varAllocs.emplace_back(alloca);
				varAddrId.insert(alloca->values[0]->value->id);
			}
		}
		for (AllocaInst *inst : varAllocs) {
			Bitmask defs(cnt);
			Bitmask finished(cnt);
			Bitmask waiting(cnt);
			Value *value = inst->values[0]->value;
			for (Use *use : value->uses) {
				Inst *inst = (Inst *)use->user;
				BasicBlock *block = inst->block;
				defs.set(index[block->id], 1);
				waiting.set(index[block->id], 1);
			}
			int blockIndex = index[inst->block->id];
			while (waiting.count() > 0) {
				int id = waiting.ctz();
				waiting.set(id, 0);
				for (int i = blockIndex + 1; i < cnt; i++) {
					if (domAnalyzer.domFrontier[blocks[id]].count(blocks[i]) == 0) {
						continue;
					}
					if (finished.get(i) == 0) {
						Value *reg = new Value(inst->type);
						PhiInst *phi = new PhiInst(inst->type, reg, inst->var);
						phi->block = blocks[i];
						blocks[i]->prepend(phi);
						insertedPhis.insert(phi);
						finished.set(i, 1);
						if (defs.get(i) == 0) {
							waiting.set(i, 1);
						}
						module->changed = true;
					}
				}
			}
		}

		// remove memory instructions
		regDefBlock.clear();
		reachingDef.clear();
		replace.clear();
		removeMemInsts(blocks[0]);

		// remove unreachable basic blocks
		vector <BasicBlock *> removeBlocks;
		for (int i = 1; i < cnt; i++) {
			if (domAnalyzer.domParent[blocks[i]] == nullptr) {
				removeBlocks.emplace_back(blocks[i]);
			}
		}
		for (BasicBlock *block : removeBlocks) {
			block->destroy();
			curFunc->remove(block);
			module->changed = true;
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
		node->accept(domAnalyzer);

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