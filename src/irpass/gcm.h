/*
# global code motion
====================

this pass rearrange all the instructions in basic blocks
*/

#ifndef __CPL_GCM_H__
#define __CPL_GCM_H__

#include <vector>
#include <map>
#include <algorithm>

#include "../ir.h"
#include "domanalyzer.h"
#include "loopanalyzer.h"
#include "reglabeller.h"


namespace IR {

namespace Passes {

using namespace std;
using namespace IR;


class GCM : public Pass {
public:
	DomAnalyzer domAnalyzer;
	LoopAnalyzer loopAnalyzer;
	RegLabeller regLabeller;

	Module *module;
	Function *curFunc = nullptr;
	BasicBlock *funcEntry = nullptr;

	map <Inst *, bool> visited;
	map <Inst *, BasicBlock *> instBlock;


	Inst *getDefInst(Value *value) {
		if (value->regId == UNAVAILABLE) {
			return nullptr;
		}
		for (Use *use : value->uses) {
			if (use->index > 0) {
				continue;
			}
			Inst *inst = (Inst *)use->user;
			if (inst->noDef || inst->terminate) {
				continue;
			}
			return inst;
		}
		return nullptr;
	}

	bool isPinnedInst(Inst *inst) {
		return inst->terminate
			|| inst->instType() == TPhiInst
			|| inst->instType() == TCallInst
			|| inst->instType() == TLoadInst
			|| inst->instType() == TStoreInst;
	}


	void scheduleEarly(Inst *inst) {
		if (inst == nullptr || visited[inst] || isPinnedInst(inst)) {
			return;
		}
		visited[inst] = true;
		instBlock[inst] = funcEntry;
		for (int i = inst->noDef || inst->terminate ? 0 : 1; i < inst->values.size(); i++) {
			Inst *def = getDefInst((*inst)[i]);
			if (def == nullptr) {
				continue;
			}
			scheduleEarly(def);
			if (domAnalyzer.domDepth[instBlock[def]] > domAnalyzer.domDepth[instBlock[inst]]) {
				instBlock[inst] = instBlock[def];
			}
		}
	}

	void scheduleLate(Inst *inst) {
		if (inst == nullptr || visited[inst] || isPinnedInst(inst)) {
			return;
		}
		visited[inst] = true;
		BasicBlock *lca = nullptr;
		// instructions with no def are pinned
		Value *value = (*inst)[0];
		for (Use *use : value->uses) {
			if (use->user == inst && use->index == 0) {
				continue;
			}
			Inst *user = (Inst *)use->user;
			scheduleLate(user);
			if (user->instType() == TPhiInst) {
				BasicBlock *from = (BasicBlock *)((*user)[use->index + 1]);
				if (lca == nullptr) {
					lca = from;
				} else {
					lca = domAnalyzer.domTreeLCA(lca, from);
				}
			} else {
				if (lca == nullptr) {
					lca = instBlock[user];
				} else {
					lca = domAnalyzer.domTreeLCA(lca, instBlock[user]);
				}
			}
		}
		BasicBlock *best = lca;
		while (lca != instBlock[inst]) {
			if (loopAnalyzer.loopDepth[lca] < loopAnalyzer.loopDepth[best]) {
				best = lca;
			}
			lca = domAnalyzer.domParent[lca];
		}
		if (loopAnalyzer.loopDepth[lca] < loopAnalyzer.loopDepth[best]) {
			best = lca;
		}
		instBlock[inst] = best;
	}

	void visitFunction(Function *node) {
		curFunc = node;
		funcEntry = node->blocks.first();
		instBlock.clear();

		// initialize pinned instructions
		for (BasicBlock *block : node->blocks) {
			for (Inst *inst : block->insts) {
				if (isPinnedInst(inst)) {
					instBlock[inst] = block;
				}
			}
		}

		// schedule early
		visited.clear();
		for (BasicBlock *block : node->blocks) {
			for (Inst *inst : block->insts) {
				if (isPinnedInst(inst)) {
					visited[inst] = true;
					instBlock[inst] = block;
					for (int i = inst->noDef || inst->terminate ? 0 : 1; i < inst->values.size(); i++) {
						scheduleEarly(getDefInst((*inst)[i]));
					}
				}
			}
		}

		// schedule late
		visited.clear();
		for (BasicBlock *block : node->blocks) {
			for (Inst *inst : block->insts) {
				if (isPinnedInst(inst)) {
					visited[inst] = true;
				} else {
					scheduleLate(inst);
				}
			}
		}

		// rearrange instructions
		map <BasicBlock *, vector <Inst *>> pendingInsts;
		for (BasicBlock *block : node->blocks) {
			for (Inst *inst : block->insts) {
				if (instBlock[inst] == block) {
					continue;
				}
				block->remove(inst);
				BasicBlock *target = instBlock[inst];
				pendingInsts[target].emplace_back(inst);
				module->changed = true;
			}
		}
		for (BasicBlock *block : node->blocks) {
			sort(pendingInsts[block].begin(), pendingInsts[block].end(),
				[&](Inst *instA, Inst *instB) {
					Value *defA = (*instA)[0];
					Value *defB = (*instB)[0];
					return defA->regId < defB->regId;
				}
			);
			auto it = block->insts.begin();
			auto lastIt = block->insts.rend();
			int curRegId = 0;
			if (!(*it)->noDef && !(*it)->terminate) {
				Inst *inst = *it;
				curRegId = (*inst)[0]->regId;
			}
			while ((*it)->instType() == TPhiInst) {
				lastIt = it;
				++it;
				Inst *inst = *it;
				if (!inst->noDef && !inst->terminate) {
					curRegId = (*inst)[0]->regId;
				}
			}
			for (Inst *pendingInst : pendingInsts[block]) {
				int regId = (*pendingInst)[0]->regId;
				while (curRegId < regId && !(*it)->terminate) {
					if (!(*it)->noDef) {
						lastIt = it;
					}
					++it;
					Inst *inst = *it;
					if (!inst->noDef && !inst->terminate) {
						curRegId = (*inst)[0]->regId;
					}
				}
				if (lastIt != block->insts.rend()) {
					(*lastIt)->insertAfter(pendingInst);
				} else {
					block->prepend(pendingInst);
				}
				pendingInst->block = block;
				++lastIt;
			}
		}
	}

	void visitModule(Module *node) {
		node->accept(regLabeller);
		node->accept(loopAnalyzer);

		module = node;
		for (Function *func : node->funcs) {
			func->accept(*this);
		}
	}
};

}

}

#endif