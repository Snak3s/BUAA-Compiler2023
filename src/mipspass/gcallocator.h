#ifndef __CPL_GC_ALLOCATOR__
#define __CPL_GC_ALLOCATOR__

#include <vector>
#include <set>
#include <map>
#include <algorithm>

#include "../mips.h"


namespace MIPS {

namespace Allocator {

using namespace std;
using namespace MIPS::Passes;
using namespace Bits;


class GCAllocator : public Pass {
public:
	int availRegCnt;
	vector <Register *> availRegs;
	vector <Register *> tempRegs;

	MFunction *mainFunc = nullptr;
	MFunction *curFunc = nullptr;

	using RegSet = set <Register *, RegisterPtrComp>;

	map <MInst *, RegSet> in, out;
	map <MInst *, RegSet> def, use;

	map <Register *, int, RegisterPtrComp> activeLength;
	map <Register *, set <MInst *>, RegisterPtrComp> regDefs;
	map <Register *, set <MInst *>, RegisterPtrComp> regUses;

	RegSet initialRegs;
	RegSet simplifyWorklist;
	RegSet freezeWorklist;
	RegSet spillWorklist;
	RegSet spilledRegs;
	RegSet coalescedRegs;
	RegSet coloredRegs;
	vector <Register *> selectedList;
	RegSet selectedRegs;

	set <MInst *> coalescedMoves;
	set <MInst *> constrainedMoves;
	set <MInst *> frozenMoves;
	set <MInst *> worklistMoves;
	set <MInst *> activeMoves;

	map <Register *, RegSet, RegisterPtrComp> adjacent;
	map <Register *, int, RegisterPtrComp> degree;
	map <Register *, vector <MInst *>, RegisterPtrComp> moveList;
	map <Register *, Register *, RegisterPtrComp> alias;
	map <Register *, Register *, RegisterPtrComp> color;


	GCAllocator() {
		availRegCnt = 18;
		availRegs = {T0, T1, T2, T3, T4, T5, T6, T7, S0, S1, S2, S3, S4, S5, S6, S7, T8, T9};
		tempRegs = {T0, T1, T2, T3, T4, T5, T6, T7, T8, T9, A0, A1, A2, A3, V0, V1};
	}


	void insertInto(RegSet &s, Register *reg) {
		if (reg->type != RVirtual) {
			return;
		}
		s.insert(reg);
	}

	void insertInto(RegSet &s, RegSet &t) {
		for (Register *reg : t) {
			s.insert(reg);
		}
	}

	void removeFrom(RegSet &s, Register *reg) {
		s.erase(reg);
	}

	void removeFrom(RegSet &s, RegSet &t) {
		for (Register *reg : t) {
			s.erase(reg);
		}
	}


	bool hasEdge(Register *u, Register *v) {
		if (u->type == RVirtual) {
			return adjacent[u].count(v);
		}
		if (v->type == RVirtual) {
			return adjacent[v].count(u);
		}
		return false;
	}

	void addEdge(Register *u, Register *v) {
		if (u->type == RImmediate || u->type == RLabel) {
			return;
		}
		if (v->type == RImmediate || v->type == RLabel) {
			return;
		}
		if (u != v && !hasEdge(u, v)) {
			if (u->type == RVirtual) {
				adjacent[u].insert(v);
				degree[u]++;
			}
			if (v->type == RVirtual) {
				adjacent[v].insert(u);
				degree[v]++;
			}
		}
	}


	Register *getAlias(Register *reg) {
		if (alias.count(reg) == 0 || coalescedRegs.count(reg) == 0) {
			return reg;
		}
		alias[reg] = getAlias(alias[reg]);
		return alias[reg];
	}


	bool isMoveInst(MInst *inst) {
		if (inst->instType() == TAddInst || inst->instType() == TAdduInst) {
			if (inst->operands[1] == ZERO && inst->operands[2]->type != RImmediate) {
				return true;
			}
			if (inst->operands[2] == ZERO) {
				return true;
			}
		}
		return false;
	}

	bool isMoveRelated(Register *reg) {
		for (MInst *inst : moveList[reg]) {
			if (activeMoves.count(inst) || worklistMoves.count(inst)) {
				return true;
			}
		}
		return false;
	}

	void enableMoves(Register *reg) {
		for (MInst *inst : moveList[reg]) {
			if (activeMoves.count(inst) == 0) {
				continue;
			}
			activeMoves.erase(inst);
			worklistMoves.insert(inst);
		}
	}

	void getMoveInstOperands(MInst *inst, Register **x, Register **y) {
		if (inst->instType() == TAddInst || inst->instType() == TAdduInst) {
			*x = inst->operands[0];
			if (inst->operands[1] == ZERO) {
				*y = inst->operands[2];
			} else {
				*y = inst->operands[1];
			}
			return;
		}
	}

	void freezeMoves(Register *reg) {
		for (MInst *inst : moveList[reg]) {
			if (!activeMoves.count(inst) && !worklistMoves.count(inst)) {
				continue;
			}
			activeMoves.erase(inst);
			frozenMoves.insert(inst);
			Register *x = nullptr, *y = nullptr;
			getMoveInstOperands(inst, &x, &y);
			Register *tar = nullptr;
			if (getAlias(x) == getAlias(reg)) {
				tar = getAlias(y);
			} else {
				tar = getAlias(x);
			}
			if (!isMoveRelated(tar) && degree[tar] < availRegCnt) {
				removeFrom(freezeWorklist, tar);
				insertInto(simplifyWorklist, tar);
			}
		}
	}


	void decDegree(Register *reg) {
		degree[reg]--;
		if (degree[reg] == availRegCnt) {
			enableMoves(reg);
			for (Register *adj : adjacent[reg]) {
				if (selectedRegs.count(adj) || coalescedRegs.count(adj)) {
					continue;
				}
				enableMoves(adj);
			}
			removeFrom(spillWorklist, reg);
			if (isMoveRelated(reg)) {
				insertInto(freezeWorklist, reg);
			} else {
				insertInto(simplifyWorklist, reg);
			}
		}
	}

	void addWorkList(Register *reg) {
		if (reg->type == RVirtual && !isMoveRelated(reg) && degree[reg] < availRegCnt) {
			removeFrom(freezeWorklist, reg);
			insertInto(simplifyWorklist, reg);
		}
	}


	void analyse() {
		def.clear();
		use.clear();
		in.clear();
		out.clear();
		activeLength.clear();
		regDefs.clear();
		regUses.clear();
		initialRegs.clear();
		map <Register *, MInst *> label2entry;
		for (MBasicBlock *block : curFunc->blocks) {
			label2entry[block->label] = block->insts.first();
			for (MInst *inst : block->insts) {
				int index = 0;
				if (!inst->noDef) {
					insertInto(def[inst], inst->operands[0]);
					index = 1;
				}
				for (int i = index; i < inst->operands.size(); i++) {
					insertInto(use[inst], inst->operands[i]);
				}
			}
		}
		bool changed = true;
		while (changed) {
			changed = false;
			auto blockIt = curFunc->blocks.rbegin();
			while (blockIt != curFunc->blocks.rend()) {
				MBasicBlock *block = *blockIt;
				auto instIt = block->insts.rbegin();
				MInst *nextInst = nullptr;
				while (instIt != block->insts.rend()) {
					MInst *inst = *instIt;
					int sizeIn = in[inst].size();
					int sizeOut = out[inst].size();

					// out[inst] = union in[successors of inst]
					out[inst].clear();
					if (inst->terminate) {
						if (inst->instType() == TJInst) {
							insertInto(out[inst], in[label2entry[inst->operands[0]]]);
						} else if (inst->instType() != TJrInst) {
							insertInto(out[inst], in[label2entry[inst->operands.back()]]);
							insertInto(out[inst], in[nextInst]);
						}
					} else {
						insertInto(out[inst], in[nextInst]);
					}

					// in[inst] = use[inst] union (out[inst] - def[inst])
					in[inst].clear();
					insertInto(in[inst], out[inst]);
					if (!inst->noDef) {
						removeFrom(in[inst], inst->operands[0]);
					}
					insertInto(in[inst], use[inst]);

					// spill cost ~ activeLength
					// spill cost ~ 1 / (regDefs count + regUses count)
					for (Register *reg : in[inst]) {
						activeLength[reg]++;
					}
					int index = 0;
					if (!inst->noDef) {
						if (inst->operands[0]->type == RVirtual) {
							regDefs[inst->operands[0]].insert(inst);
						}
						index = 1;
					}
					for (int i = index; i < inst->operands.size(); i++) {
						if (inst->operands[i]->type == RVirtual) {
							regUses[inst->operands[i]].insert(inst);
						}
					}

					for (Register *reg : inst->operands) {
						insertInto(initialRegs, reg);
					}

					if (in[inst].size() != sizeIn || out[inst].size() != sizeOut) {
						changed = true;
					}
					nextInst = inst;
					--instIt;
				}
				--blockIt;
			}
		}
	}

	void build() {
		adjacent.clear();
		moveList.clear();
		coalescedRegs.clear();
		coloredRegs.clear();
		coalescedMoves.clear();
		constrainedMoves.clear();
		frozenMoves.clear();
		worklistMoves.clear();
		activeMoves.clear();
		for (MBasicBlock *block : curFunc->blocks) {
			auto instIt = block->insts.rbegin();
			MInst *lastInst = block->insts.last();
			while (instIt != block->insts.rend()) {
				MInst *inst = *instIt;
				if (!inst->terminate) {
					break;
				}
				lastInst = inst;
				--instIt;
			}
			RegSet live = out[lastInst];
			instIt = block->insts.rbegin();
			while (instIt != block->insts.rend()) {
				MInst *inst = *instIt;
				if (isMoveInst(inst)) {
					removeFrom(live, use[inst]);
					for (Register *reg : inst->operands) {
						if (reg->type == RVirtual) {
							moveList[reg].emplace_back(inst);
						}
					}
					worklistMoves.insert(inst);
				}
				if (!inst->noDef) {
					insertInto(live, inst->operands[0]);
					for (Register *reg : live) {
						addEdge(reg, inst->operands[0]);
					}
				}

				if (inst->instType() == TJalInst) {
					for (Register *reg : live) {
						for (Register *temp : tempRegs) {
							addEdge(reg, temp);
						}
					}
				}

				removeFrom(live, def[inst]);
				insertInto(live, use[inst]);
				--instIt;
			}
		}

		for (Register *reg : initialRegs) {
			if (degree[reg] >= availRegCnt) {
				insertInto(spillWorklist, reg);
			} else if (isMoveRelated(reg)) {
				insertInto(freezeWorklist, reg);
			} else {
				insertInto(simplifyWorklist, reg);
			}
		}
		initialRegs.clear();
	}

	void simplify() {
		Register *reg = *simplifyWorklist.begin();
		removeFrom(simplifyWorklist, reg);
		selectedList.emplace_back(reg);
		insertInto(selectedRegs, reg);
		for (Register *adj : adjacent[reg]) {
			if (selectedRegs.count(adj) || coalescedRegs.count(adj)) {
				continue;
			}
			decDegree(adj);
		}
	}

	void combine(Register *u, Register *v) {
		removeFrom(freezeWorklist, v);
		removeFrom(spillWorklist, v);
		insertInto(coalescedRegs, v);
		alias[v] = u;
		for (MInst *inst : moveList[v]) {
			moveList[u].emplace_back(inst);
		}
		enableMoves(v);
		for (Register *adj : adjacent[v]) {
			if (selectedRegs.count(adj) || coalescedRegs.count(adj)) {
				continue;
			}
			addEdge(u, adj);
			decDegree(adj);
		}
		if (degree[u] >= availRegCnt && freezeWorklist.count(u)) {
			removeFrom(freezeWorklist, u);
			insertInto(spillWorklist, u);
		}
	}

	void coalesce() {
		MInst *inst = *worklistMoves.begin();
		worklistMoves.erase(inst);
		Register *x = nullptr, *y = nullptr;
		getMoveInstOperands(inst, &x, &y);
		x = getAlias(x);
		y = getAlias(y);
		if (y->type != RVirtual) {
			swap(x, y);
		}
		if (x == y) {
			coalescedMoves.insert(inst);
			addWorkList(x);
			return;
		}
		if (y->type != RVirtual || hasEdge(x, y)) {
			constrainedMoves.insert(inst);
			addWorkList(x);
			addWorkList(y);
			return;
		}
		if (x->type != RVirtual) {
			bool valid = true;
			for (Register *adj : adjacent[y]) {
				if (selectedRegs.count(adj) || coalescedRegs.count(adj)) {
					continue;
				}
				if (degree[adj] < availRegCnt || adj->type != RVirtual || hasEdge(x, adj)) {
					continue;
				}
				valid = false;
				break;
			}
			if (valid) {
				coalescedMoves.insert(inst);
				combine(x, y);
				addWorkList(x);
				return;
			}
		}
		if (x->type == RVirtual) {
			RegSet adjacents;
			insertInto(adjacents, adjacent[x]);
			insertInto(adjacents, adjacent[y]);
			int count = 0;
			for (Register *adj : adjacents) {
				if (selectedRegs.count(adj) || coalescedRegs.count(adj)) {
					continue;
				}
				if (degree[adj] >= availRegCnt) {
					count++;
				}
			}
			if (count < availRegCnt) {
				coalescedMoves.insert(inst);
				combine(x, y);
				addWorkList(x);
				return;
			}
		}
		activeMoves.insert(inst);
	}

	void freeze() {
		Register *reg = *freezeWorklist.begin();
		removeFrom(freezeWorklist, reg);
		insertInto(simplifyWorklist, reg);
		freezeMoves(reg);
	}

	void selectSpill() {
		Register *reg = nullptr;
		double maxCost = 0;
		for (Register *r : spillWorklist) {
			// useCount ~ the instructions to be inserted
			int useCount = regDefs[r].size() + regUses[r].size();
			double cost = 1.0 * (1 + activeLength[r]) / (1 + useCount);
			if (cost > maxCost) {
				maxCost = cost;
				reg = r;
			}
		}
		removeFrom(spillWorklist, reg);
		insertInto(simplifyWorklist, reg);
		freezeMoves(reg);
	}

	void assignColors() {
		while (!selectedList.empty()) {
			Register *reg = selectedList.back();
			selectedList.pop_back();
			RegSet candidates;
			for (Register *availReg : availRegs) {
				candidates.insert(availReg);
			}
			for (Register *adj : adjacent[reg]) {
				Register *alias = getAlias(adj);
				if (alias->type != RVirtual) {
					candidates.erase(alias);
				}
				if (coloredRegs.count(alias)) {
					candidates.erase(color[alias]);
				}
			}
			if (candidates.empty()) {
				spilledRegs.insert(reg);
			} else {
				coloredRegs.insert(reg);
				color[reg] = *candidates.begin();
			}
		}
		selectedList.clear();
		selectedRegs.clear();
		for (Register *reg : coalescedRegs) {
			Register *alias = getAlias(reg);
			if (alias->type != RVirtual) {
				color[reg] = alias;
			} else {
				color[reg] = color[alias];
			}
		}
	}

	void rewrite() {
		for (Register *reg : spilledRegs) {
			if (CPL_Opt_EnableAddrToReg) {
				if (regDefs[reg].size() == 1) {
					MInst *defInst = *(regDefs[reg].begin());
					if (defInst->instType() == TLaInst
						|| defInst->instType() == TLiInst) {
						bool noRVirtualUse = true;
						for (int i = 1; i < defInst->operands.size(); i++) {
							if (defInst->operands[i]->type == RVirtual) {
								noRVirtualUse = false;
								break;
							}
						}
						if (noRVirtualUse) {
							for (MInst *inst : regUses[reg]) {
								if (defInst->instType() == TLaInst
									&& (inst->instType() == TLwInst
										|| inst->instType() == TSwInst
										|| inst->instType() == TLaInst)
									&& inst->operands[1]->immediate == 0) {
									inst->operands[1] = (*defInst)[1];
									inst->operands[2] = (*defInst)[2];
									continue;
								}

								Register *newReg = new Register();
								int index = 0;
								if (!inst->noDef) {
									index = 1;
								}
								MInst *instCopy = nullptr;
								if (defInst->instType() == TLaInst) {
									instCopy = new LaInst(newReg, (*defInst)[1], (*defInst)[2]);
								} else if (defInst->instType() == TLiInst) {
									instCopy = new LiInst(newReg, (*defInst)[1]);
								}
								inst->insertBefore(instCopy);
								for (int i = index; i < inst->operands.size(); i++) {
									if (inst->operands[i] == reg) {
										inst->operands[i] = newReg;
									}
								}
							}
							defInst->remove();
							continue;
						}
					}
				}
			}

			MAddress *addr = curFunc->stack->alloc(4);
			for (MInst *inst : regDefs[reg]) {
				Register *newReg = new Register();
				inst->operands[0] = newReg;
				inst->insertAfter(new SwInst(newReg, addr));
			}
			for (MInst *inst : regUses[reg]) {
				Register *newReg = new Register();
				int index = 0;
				if (!inst->noDef) {
					index = 1;
				}
				inst->insertBefore(new LwInst(newReg, addr));
				for (int i = index; i < inst->operands.size(); i++) {
					if (inst->operands[i] == reg) {
						inst->operands[i] = newReg;
					}
				}
			}
		}
		spilledRegs.clear();
		coloredRegs.clear();
		coalescedRegs.clear();
	}


	void allocateRegs() {
		analyse();
		build();
		while (true) {
			if (!simplifyWorklist.empty()) {
				simplify();
				continue;
			}
			if (!worklistMoves.empty()) {
				coalesce();
				continue;
			}
			if (!freezeWorklist.empty()) {
				freeze();
				continue;
			}
			if (!spillWorklist.empty()) {
				selectSpill();
				continue;
			}
			break;
		}
		assignColors();
		if (!spilledRegs.empty()) {
			rewrite();
			allocateRegs();
		}
	}


	void replaceRegs() {
		for (MBasicBlock *block : curFunc->blocks) {
			for (MInst *inst : block->insts) {
				for (int i = 0; i < inst->operands.size(); i++) {
					if (inst->operands[i]->type != RVirtual) {
						continue;
					}
					inst->operands[i] = color[inst->operands[i]];
				}
			}
		}
		for (MInst *inst : coalescedMoves) {
			inst->remove();
		}
	}

	void calleeSavedRegs() {
		RegSet savedRegs;
		for (Register *reg : coloredRegs) {
			savedRegs.insert(color[reg]);
		}
		if (curFunc != mainFunc) {
			for (Register *reg : tempRegs) {
				savedRegs.erase(reg);
			}
		} else {
			for (Register *reg : availRegs) {
				savedRegs.erase(reg);
			}
		}
		MInst *firstInst = curFunc->blocks.first()->insts.first();
		map <Register *, MAddress *> savedAddr;
		for (Register *reg : savedRegs) {
			MAddress *addr = curFunc->stack->alloc(4);
			savedAddr[reg] = addr;
			firstInst->insertAfter(new SwInst(reg, addr));
		}
		for (MBasicBlock *block : curFunc->blocks) {
			for (MInst *inst : block->insts) {
				if (inst->instType() != TJrInst) {
					continue;
				}
				for (Register *reg : savedRegs) {
					inst->insertBefore(new LwInst(reg, savedAddr[reg]));
				}
			}
		}
	}


	void visitMFunction(MFunction *node) {
		curFunc = node;
		allocateRegs();
		replaceRegs();
		calleeSavedRegs();
	}

	void visitMModule(MModule *node) {
		mainFunc = node->funcs.last();
		for (MFunction *func : node->funcs) {
			func->accept(*this);
		}
	}
};

}

}

#endif