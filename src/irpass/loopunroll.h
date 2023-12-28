/*
# loop unroll
=============

this pass unrolls simple loops
*/

#ifndef __CPL_LOOP_UNROLL_H__
#define __CPL_LOOP_UNROLL_H__

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


class LoopUnroll : public Pass {
public:
	DomAnalyzer domAnalyzer;
	LoopAnalyzer loopAnalyzer;
	RegLabeller regLabeller;

	Module *module;
	Function *curFunc = nullptr;

	const int maxInstCnt = 1 << 14;
	const int maxBlockCnt = 1 << 11;

	vector <map <Value *, Value *>> mapping;

	void replaceReg(Value *oldReg, Value *newReg) {
		for (Use *use : oldReg->uses) {
			use->user->setValue(use->index, newReg);
		}
	}

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

	void tryUnrollLoop(Loop *loop) {
		if (loop->exits.size() > 1) {
			return;
		}
		int instCnt = 0;
		for (BasicBlock *block : loop->body) {
			if (block != loop->header && loopAnalyzer.loopAsHeader[block].size()) {
				return;
			}
			for (Inst *inst : block->insts) {
				if (inst->instType() == TCallInst) {
					Function *func = nullptr;
					if (inst->noDef) {
						func = (Function *)((*inst)[0]);
					} else {
						func = (Function *)((*inst)[1]);
					}
					if (!func->reserved) {
						return;
					}
				}
				instCnt++;
			}
		}
		BasicBlock *exiting = loop->exits.begin()->first;
		BasicBlock *exit = loop->exits.begin()->second;
		// for-loop only
		if (exiting != loop->header) {
			return;
		}
		BasicBlock *preheader = nullptr;
		BasicBlock *latch = nullptr;
		BasicBlock *target = nullptr;
		for (BasicBlock *from : exiting->jumpFrom) {
			if (loop->body.count(from)) {
				latch = from;
			} else {
				preheader = from;
			}
		}
		for (BasicBlock *to : exiting->jumpTo) {
			if (loop->body.count(to)) {
				target = to;
			}
		}
		Inst *exitInst = exiting->insts.last();
		Inst *exitCond = getDefInst((*exitInst)[0]);
		Value *op1 = (*exitCond)[1], *op2 = (*exitCond)[2];
		Type cond = ((IcmpInst *)exitCond)->cond;
		Value *var = nullptr;
		int init, step, final;
		if (op1->isConst()) {
			final = op1->getConstValue();
			var = op2;
		} else if (op2->isConst()) {
			var = op1;
			final = op2->getConstValue();
		} else {
			return;
		}
		Inst *phiInst = getDefInst(var);
		if (phiInst == nullptr
			|| phiInst->instType() != TPhiInst
			|| phiInst->block != exiting) {
			return;
		}
		Value *stepVar = nullptr;
		Value *initVar = nullptr;
		for (int i = 1; i < phiInst->values.size(); i += 2) {
			if ((*phiInst)[i + 1] == latch) {
				stepVar = (*phiInst)[i];
			} else if ((*phiInst)[i + 1] == preheader) {
				initVar = (*phiInst)[i];
			}
		}
		if (initVar == nullptr || stepVar == nullptr) {
			return;
		}
		if (initVar->isConst()) {
			init = initVar->getConstValue();
		} else {
			return;
		}
		Inst *stepInst = getDefInst(stepVar);
		if (stepInst == nullptr || loop->body.count(stepInst->block) == 0) {
			return;
		}
		if (stepInst->instType() != TAddInst || (*stepInst)[1] != var) {
			return;
		}
		if ((*stepInst)[2]->isConst()) {
			step = (*stepInst)[2]->getConstValue();
		} else {
			return;
		}

		int loopCnt;
		if (cond == CondSlt && step > 0) {
			loopCnt = (final + step - 1 - init) / step;
		} else if (cond == CondSle && step > 0) {
			loopCnt = (final + step - init) / step;
		} else if (cond == CondSgt && step < 0) {
			loopCnt = (final + step + 1 - init) / step;
		} else if (cond == CondSge && step < 0) {
			loopCnt = (final + step - init) / step;
		} else {
			return;
		}
		loopCnt = max(loopCnt, 0);

		// unroll empty loop
		if (target == latch
			&& target->insts.first() == stepInst
			&& ((Inst *)stepInst->__ll_next)->terminate
			&& exiting->insts.first() == phiInst
			&& phiInst->__ll_next == exitCond
			&& exitCond->__ll_next == exitInst) {
			replaceReg((*phiInst)[0], new NumberLiteral(init + loopCnt * step));
			module->changed = true;
			return;
		}

		if (loopCnt > maxBlockCnt / loop->body.size()) {
			return;
		}
		if (loopCnt > maxInstCnt / instCnt) {
			return;
		}

		// copy loop body
		vector <BasicBlock *> body;
		for (BasicBlock *block : loop->body) {
			body.emplace_back(block);
		}
		sort(body.begin(), body.end(), [&](BasicBlock *a, BasicBlock *b) {
			return a->label->regId < b->label->regId;
		});
		// now exiting should be at the beginning of loop body
		mapping.clear();
		mapping.resize(loopCnt + 1);
		BasicBlock *lastBlock = body.back();
		for (int i = 0; i <= loopCnt; i++) {
			for (BasicBlock *srcBlock : body) {
				if (i == loopCnt && srcBlock != exiting) {
					continue;
				}
				BasicBlock *destBlock = curFunc->allocBasicBlockAfter(lastBlock);
				lastBlock = destBlock;
				mapping[i][srcBlock] = destBlock;
				mapping[i][srcBlock->label] = destBlock->label;
			}
			for (BasicBlock *srcBlock : body) {
				if (i == loopCnt && srcBlock != exiting) {
					continue;
				}
				BasicBlock *destBlock = (BasicBlock *)mapping[i][srcBlock];
				for (Inst *srcInst : srcBlock->insts) {
					Inst *destInst = srcInst->copy();
					destBlock->append(destInst);
					destInst->block = destBlock;
					if (!destInst->noDef && !destInst->terminate) {
						mapping[i][(*destInst)[0]] = new Value((*destInst)[0]->type);
					}
				}
			}
			for (BasicBlock *srcBlock : body) {
				if (i == loopCnt && srcBlock != exiting) {
					continue;
				}
				BasicBlock *destBlock = (BasicBlock *)mapping[i][srcBlock];
				for (Inst *destInst : destBlock->insts) {
					for (int j = 0; j < destInst->values.size(); j++) {
						Value *reg = (*destInst)[j];
						if (mapping[i].count(reg)
							&& !(srcBlock == exiting && destInst->instType() == TPhiInst && j > 0)) {
							destInst->values[j]->setValue(mapping[i][reg]);
						} else if (i > 0 && mapping[i - 1].count(reg)) {
							destInst->values[j]->setValue(mapping[i - 1][reg]);
						}
						if (reg->isConst()) {
							destInst->values[j]->setValue(new NumberLiteral(reg->getConstValue()));
						}
					}
				}
			}
		}
		for (int i = 0; i <= loopCnt; i++) {
			BasicBlock *curExiting = (BasicBlock *)mapping[i][exiting];
			if (i < loopCnt) {
				curExiting->insts.last()->replaceWith(new BrInst(mapping[i][target]));
				BasicBlock *curLatch = (BasicBlock *)mapping[i][latch];
				curLatch->insts.last()->replaceWith(new BrInst(mapping[i + 1][exiting]));
			} else {
				curExiting->insts.last()->replaceWith(new BrInst(exit));
			}
			for (Inst *inst : curExiting->insts) {
				if (inst->instType() != TPhiInst) {
					break;
				}
				for (int j = 1; j < inst->values.size(); j += 2) {
					if (i == 0 && (*inst)[j + 1] != preheader) {
						inst->removeValue(inst->values[j + 1]);
						inst->removeValue(inst->values[j]);
						break;
					}
					if (i > 0 && (*inst)[j + 1] != latch) {
						inst->removeValue(inst->values[j + 1]);
						inst->removeValue(inst->values[j]);
						break;
					}
				}
			}
		}
		Inst *preheaderJump = preheader->insts.last();
		for (int i = 0; i < preheaderJump->values.size(); i++) {
			if ((*preheaderJump)[i] == exiting) {
				preheaderJump->values[i]->setValue(mapping[0][exiting]);
			}
		}
		for (BasicBlock *block : body) {
			for (Inst *inst : block->insts) {
				if (inst->noDef || inst->terminate) {
					continue;
				}
				Value *def = (*inst)[0];
				for (int i = loopCnt; i >= 0; i--) {
					if (mapping[i].count(def)) {
						replaceReg(def, mapping[i][def]);
						break;
					}
				}
			}
			curFunc->remove(block);
			block->destroy();
		}

		module->changed = true;
	}

	void visitFunction(Function *node) {
		curFunc = node;
		for (BasicBlock *block : node->blocks) {
			if (block->insts.empty()) {
				continue;
			}
			if (loopAnalyzer.loopAsHeader[block].size() == 1) {
				Loop *loop = loopAnalyzer.loopAsHeader[block][0];
				tryUnrollLoop(loop);
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