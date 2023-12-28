#ifndef __CPL_LINEAR_ALLOCATOR_H__
#define __CPL_LINEAR_ALLOCATOR_H__

#include <map>


namespace MIPS {

namespace Allocator {

using namespace std;
using namespace MIPS::Passes;


class LinearAllocator : public Pass {
public:
	int regCnt;
	vector <Register *> regs;

	MFunction *curFunc = nullptr;
	MBasicBlock *curBlock = nullptr;
	MInst *curInst = nullptr;

	vector <Register *> active;
	vector <int> modified;
	vector <int> counter;
	vector <MAddress *> spillAddr;
	vector <int> spillUsed;
	map <int, int> allocated;
	map <int, int> spillIndex;

	map <int, MInst *> firstUse;
	map <int, MInst *> lastUse;


	LinearAllocator() {
		regCnt = 18;
		regs = {T0, T1, T2, T3, T4, T5, T6, T7, T8, T9, S0, S1, S2, S3, S4, S5, S6, S7};
	}


	void appendInst(MInst *inst) {
		curBlock->append(inst);
		inst->block = curBlock;
	}

	void insertBefore(MInst *inst) {
		if (curInst != nullptr) {
			curInst->insertBefore(inst);
			return;
		}
		appendInst(inst);
	}


	MAddress *allocSpillAddr(Register *reg) {
		spillAddr.emplace_back(curFunc->stack->alloc(4));
		spillUsed.emplace_back(true);
		spillIndex[reg->value->id] = spillAddr.size() - 1;
		return spillAddr[spillIndex[reg->value->id]];
	}

	void spillReg(int index = -1) {
		if (index < 0) {
			int maxCntIndex = 0;
			for (int i = 0; i < regCnt; i++) {
				if (active[i] == nullptr) {
					continue;
				}
				if (counter[i] < counter[maxCntIndex]) {
					maxCntIndex = i;
				}
			}
			index = maxCntIndex;
		}
		Register *reg = active[index];
		active[index] = nullptr;
		if (reg == nullptr) {
			return;
		}
		allocated.erase(reg->value->id);
		if (!modified[index]) {
			return;
		}
		if (spillIndex.count(reg->value->id)) {
			insertBefore(new SwInst(regs[index], spillAddr[spillIndex[reg->value->id]]));
			return;
		}
		insertBefore(new SwInst(regs[index], allocSpillAddr(reg)));
	}

	Register *allocReg(Register *reg) {
		static int lastIndex = -1;
		for (int i = 0; i < regCnt; i++) {
			counter[i]++;
		}
		int index = -1;
		for (int i = 0; i < regCnt; i++) {
			if (active[i] == nullptr) {
				if (i == lastIndex) {
					continue;
				}
				if (index < 0) {
					index = i;
				} else if (counter[i] > counter[index]) {
					index = i;
				}
			} else if (active[i] == reg) {
				return regs[i];
			}
		}
		if (index >= 0) {
			lastIndex = index;
			active[index] = reg;
			allocated[reg->value->id] = index;
			counter[index] = 0;
			modified[index] = false;
			return regs[index];
		}
		spillReg();
		return allocReg(reg);
	}

	Register *getReg(Register *reg) {
		if (reg->type != RVirtual) {
			return reg;
		}
		if (allocated.count(reg->value->id)) {
			int index = allocated[reg->value->id];
			if (active[index] != nullptr && active[index] == reg) {
				for (int i = 0; i < regCnt; i++) {
					counter[i]++;
				}
				counter[index] = 0;
				return regs[index];
			}
		}
		if (!spillIndex.count(reg->value->id)) {
			allocSpillAddr(reg);
		}
		Register *tar = allocReg(reg);
		insertBefore(new LwInst(tar, spillAddr[spillIndex[reg->value->id]]));
		return tar;
	}

	void releaseReg(Register *reg) {
		if (allocated.count(reg->value->id)) {
			if (spillIndex.count(reg->value->id)
				&& modified[allocated[reg->value->id]]) {
				curInst->insertAfter(new SwInst(regs[allocated[reg->value->id]], spillAddr[spillIndex[reg->value->id]]));
			}
			active[allocated[reg->value->id]] = nullptr;
			allocated.erase(reg->value->id);
		}
		if (spillIndex.count(reg->value->id)) {
			spillUsed[spillIndex[reg->value->id]] = false;
			spillIndex.erase(reg->value->id);
		}
	}


	void visitMBasicBlock(MBasicBlock *node) {
		curBlock = node;
		for (MInst *inst : node->insts) {
			inst->accept(*this);
			visitMInst(inst);
		}
		if (node->insts.last()->instType() != TJInst
			&& node->insts.last()->instType() != TJrInst) {
			if (!curInst->terminate) {
				curInst = nullptr;
			}
			for (int i = 0; i < regCnt; i++) {
				if (active[i] != nullptr) {
					spillReg(i);
				}
			}
		}
	}

	void visitMFunction(MFunction *node) {
		curFunc = node;
		spillAddr.clear();
		spillUsed.clear();
		active.clear();
		active.resize(regCnt);
		modified.clear();
		modified.resize(regCnt);
		counter.clear();
		counter.resize(regCnt);
		for (MBasicBlock *block : node->blocks) {
			for (MInst *inst : block->insts) {
				for (Register *reg : inst->operands) {
					if (reg->type == RVirtual) {
						if (firstUse.count(reg->value->id) == 0) {
							firstUse[reg->value->id] = inst;
						}
						lastUse[reg->value->id] = inst;
					}
				}
			}
		}
		for (MBasicBlock *block : node->blocks) {
			block->accept(*this);
		}
	}

	void visitMInst(MInst *node) {
		curInst = node;
		int index = 0;
		vector <Register *> releaseRegs;
		if (!node->noDef) {
			Register *def = (*node)[0];
			if (def->type == RVirtual) {
				if (lastUse[def->value->id] == node) {
					releaseRegs.emplace_back(def);
				}
				Register *reg = allocReg(def);
				node->operands[0] = reg;
				modified[allocated[def->value->id]] = true;
			}
			index = 1;
		}
		for (int i = index; i < node->operands.size(); i++) {
			Register *op = (*node)[i];
			if (op->type == RVirtual) {
				if (lastUse[op->value->id] == node) {
					releaseRegs.emplace_back(op);
				}
				node->operands[i] = getReg(op);
			}
		}
		for (Register *reg : releaseRegs) {
			releaseReg(reg);
		}
	}

	void visitJalInst(JalInst *node) {
		curInst = node;
		for (int i = 0; i < regCnt; i++) {
			if (active[i] != nullptr) {
				spillReg(i);
			}
		}
	}

	void visitJrInst(JrInst *node) {
		for (int i = 0; i < regCnt; i++) {
			if (active[i] != nullptr) {
				allocated.erase(active[i]->value->id);
				active[i] = nullptr;
			}
		}
	}

	void visitJInst(JInst *node) {
		if (!curInst->terminate) {
			curInst = node;
		}
		for (int i = 0; i < regCnt; i++) {
			if (active[i] != nullptr) {
				spillReg(i);
			}
		}
	}

	void visitMModule(MModule *node) {
		for (MFunction *func : node->funcs) {
			func->accept(*this);
		}
	}
};

}

}

#endif