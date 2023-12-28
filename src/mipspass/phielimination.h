#ifndef __CPL_PHI_ELIMINATION_H__
#define __CPL_PHI_ELIMINATION_H__

#include <map>

#include "../mips.h"


namespace MIPS {

namespace Passes {

using namespace std;
using namespace MIPS;


class PhiElimination : public Pass {
public:
	MFunction *curFunc = nullptr;

	map <int, MBasicBlock *> id2block;
	map <string, MBasicBlock *> label2block;
	map <int, vector <int>> inEdge;
	map <int, vector <int>> outEdge;

	void addEdge(int from, int to) {
		inEdge[to].emplace_back(from);
		outEdge[from].emplace_back(to);
	}

	void splitCriticalEdge(MBasicBlock *node) {
		if (node->insts.first()->instType() != TPhiInst) {
			return;
		}
		int nodeId = node->id;
		for (int id : inEdge[nodeId]) {
			MBasicBlock *jump = curFunc->allocBasicBlock();
			jump->label = new Register(curFunc->name.substr(1) + "_jump_" + to_string(id) + "_" + to_string(nodeId));
			MBasicBlock *from = id2block[id];
			jump->loopDepth = from->loopDepth;
			auto it = from->insts.rbegin();
			while (it != from->insts.rend()) {
				MInst *inst = *it;
				if (!inst->terminate || inst->instType() == TJrInst) {
					break;
				}
				if (inst->instType() == TJInst) {
					if (inst->operands[0] == node->label) {
						inst->operands[0] = jump->label;
					}
				} else {
					if (inst->operands[2] == node->label) {
						inst->operands[2] = jump->label;
					}
				}
				--it;
			}
			for (MInst *inst : node->insts) {
				if (inst->instType() != TPhiInst) {
					break;
				}
				for (int i = 1; i < inst->operands.size(); i += 2) {
					if (inst->operands[i + 1] == from->label) {
						inst->operands[i + 1] = jump->label;
						MInst *pcopy = new PCopyInst(inst->operands[0], inst->operands[i]);
						pcopy->block = jump;
						jump->append(pcopy);
					}
				}
			}
			MInst *jumpInst = new JInst(node->label);
			jumpInst->block = jump;
			jump->append(jumpInst);
		}
		for (MInst *inst : node->insts) {
			if (inst->instType() != TPhiInst) {
				break;
			}
			inst->remove();
		}
	}

	void sequentialize(MBasicBlock *node) {
		if (node->insts.first()->instType() != TPCopyInst) {
			return;
		}
		MInst *jumpInst = node->insts.last();
		while (true) {
			map <int, int> useCount;
			int removalCount = 0;
			int pcopyCount = 0;
			MInst *pcopy = nullptr;
			for (MInst *inst : node->insts) {
				if (inst->instType() == TPCopyInst) {
					if (inst->operands[1]->type == RVirtual) {
						useCount[inst->operands[1]->value->id]++;
					}
					if (inst->operands[0] != inst->operands[1]) {
						pcopyCount++;
						pcopy = inst;
					}
				}
			}
			if (pcopyCount == 0) {
				break;
			}
			for (MInst *inst : node->insts) {
				if (inst->instType() == TPCopyInst) {
					if (useCount[inst->operands[0]->value->id] == 0) {
						jumpInst->insertBefore(new AddInst(inst->operands[0], ZERO, inst->operands[1]));
						inst->remove();
						removalCount++;
					}
				}
			}
			if (removalCount > 0) {
				continue;
			}
			Register *reg = new Register();
			jumpInst->insertBefore(new AddInst(reg, ZERO, pcopy->operands[1]));
			pcopy->operands[1] = reg;
		}
		for (MInst *inst : node->insts) {
			if (inst->instType() == TPCopyInst) {
				inst->remove();
			}
		}
	}

	void visitMFunction(MFunction *node) {
		curFunc = node;
		id2block.clear();
		label2block.clear();
		inEdge.clear();
		outEdge.clear();
		for (MBasicBlock *block : node->blocks) {
			id2block[block->id] = block;
			label2block[block->label->label] = block;
		}
		for (MBasicBlock *block : node->blocks) {
			auto it = block->insts.rbegin();
			while (it != block->insts.rend()) {
				MInst *inst = *it;
				if (inst->terminate) {
					Register *tar = nullptr;
					if (inst->instType() == TJInst) {
						tar = inst->operands[0];
					} else if (inst->instType() == TJrInst) {
						break;
					} else {
						tar = inst->operands[2];
					}
					addEdge(block->id, label2block[tar->label]->id);
				} else {
					break;
				}
				--it;
			}
		}
		for (MBasicBlock *block : node->blocks) {
			splitCriticalEdge(block);
		}
		for (MBasicBlock *block : node->blocks) {
			sequentialize(block);
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