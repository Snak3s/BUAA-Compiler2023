/*
# block rearrangement
=====================

this pass merges adjacent basic blocks and rearrange the order of basic
blocks to remove the jump instructions at the end of the basic blocks
*/

#ifndef __CPL_BLOCK_REARRANGE_H__
#define __CPL_BLOCK_REARRANGE_H__

#include <map>
#include <algorithm>

#include "../mips.h"


namespace MIPS {

namespace Passes {

using namespace std;
using namespace MIPS;


class BlockRearrange : public Pass {
public:
	MFunction *curFunc = nullptr;

	map <int, MBasicBlock *> id2block;
	map <string, MBasicBlock *> label2block;
	map <int, set <int>> inEdge;
	map <int, set <int>> outEdge;

	void addEdge(int from, int to) {
		inEdge[to].insert(from);
		outEdge[from].insert(to);
	}

	void removeEdge(int from, int to) {
		inEdge[to].erase(from);
		outEdge[from].erase(to);
	}

	void buildCFG(MFunction *node) {
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
						tar = inst->operands.back();
					}
					addEdge(block->id, label2block[tar->label]->id);
				} else {
					break;
				}
				--it;
			}
		}
	}

	void redirectSingleJump(MFunction *node) {
		MBasicBlock *entry = node->blocks.first();
		for (MBasicBlock *block : node->blocks) {
			// cannot remove func entry
			if (block == entry) {
				continue;
			}
			if (block->insts.first()->instType() == TJInst) {
				// there is only a single jump instruction
				Register *target = block->insts.first()->operands[0];
				int targetId = label2block[target->label]->id;
				removeEdge(block->id, targetId);
				vector <int> redirectId;
				for (int fromId : inEdge[block->id]) {
					MBasicBlock *from = id2block[fromId];
					auto it = from->insts.rbegin();
					while (it != from->insts.rend()) {
						MInst *inst = *it;
						if (inst->terminate) {
							if (inst->instType() == TJInst) {
								if (inst->operands[0] == block->label) {
									inst->operands[0] = target;
								}
							} else if (inst->instType() == TJrInst) {
								break;
							} else {
								int cnt = inst->operands.size();
								cnt = max(0, cnt - 1);
								if (inst->operands[cnt] == block->label) {
									inst->operands[cnt] = target;
								}
							}
						} else {
							break;
						}
						--it;
					}
					redirectId.emplace_back(fromId);
				}
				for (int fromId : redirectId) {
					removeEdge(fromId, block->id);
					addEdge(fromId, targetId);
				}
				node->remove(block);
			}
		}
	}

	void mergeBlock(MFunction *node) {
		for (MBasicBlock *block : node->blocks) {
			int blockId = block->id;
			while (true) {
				if (outEdge[blockId].size() != 1) {
					break;
				}
				int targetId = *(outEdge[blockId].begin());
				if (inEdge[targetId].size() != 1) {
					break;
				}
				// maintain cfg
				removeEdge(blockId, targetId);
				for (int to : outEdge[targetId]) {
					inEdge[to].erase(targetId);
					inEdge[to].insert(blockId);
					outEdge[blockId].insert(to);
				}
				outEdge[targetId].clear();
				// remove the jump instruction at the end of the block
				while (!block->insts.empty() && block->insts.last()->terminate) {
					block->insts.last()->remove();
				}
				MBasicBlock *target = id2block[targetId];
				MInst *lastInst = nullptr;
				for (MInst *inst : target->insts) {
					if (lastInst != nullptr) {
						lastInst->block = block;
						block->append(lastInst);
					}
					inst->remove();
					lastInst = inst;
				}
				if (lastInst != nullptr) {
					lastInst->block = block;
					block->append(lastInst);
				}
				node->remove(target);
			}
		}
	}

	void jumpReorder(MFunction *node) {
		for (MBasicBlock *block : node->blocks) {
			int blockId = block->id;
			if (outEdge[blockId].size() <= 1) {
				continue;
			}
			MInst *jump = block->insts.last();
			MInst *condJump = (MInst *)jump->__ll_prev;
			if (jump->instType() != TJInst) {
				continue;
			}
			MBasicBlock *jumpTar = label2block[(*jump)[0]->label];
			if (condJump->instType() == TBeqInst) {
				MBasicBlock *condJumpTar = label2block[(*condJump)[2]->label];
				if (jumpTar->loopDepth > condJumpTar->loopDepth) {
					condJump->replaceWith(new BneInst((*condJump)[0], (*condJump)[1], jumpTar->label));
					jump->replaceWith(new JInst(condJumpTar->label));
				}
				continue;
			}
			if (condJump->instType() == TBneInst) {
				MBasicBlock *condJumpTar = label2block[(*condJump)[2]->label];
				if (jumpTar->loopDepth > condJumpTar->loopDepth) {
					condJump->replaceWith(new BeqInst((*condJump)[0], (*condJump)[1], jumpTar->label));
					jump->replaceWith(new JInst(condJumpTar->label));
				}
				continue;
			}
		}
	}

	map <int, vector <int>> children;
	map <int, int> maxLength;
	map <int, int> arrangeNext;
	map <int, int> parent;
	map <int, int> belong;

	void calcMaxLength(int u) {
		maxLength[u] = 0;
		int heavyChild = -1;
		for (int i : children[u]) {
			calcMaxLength(i);
			if (heavyChild == -1 || maxLength[i] > maxLength[heavyChild]) {
				heavyChild = i;
			}
		}
		int loopDepth = 0;
		if (id2block.count(u)) {
			loopDepth = id2block[u]->loopDepth;
		}
		maxLength[u] += 1 << min(10, 2 * loopDepth);
		if (heavyChild >= 0 && u >= 0) {
			arrangeNext[heavyChild] = u;
		}
	}

	int getBelong(int u) {
		if (belong.count(u) == 0) {
			return u;
		}
		belong[u] = getBelong(belong[u]);
		return belong[u];
	}

	void rearrangeBlock(MFunction *node) {
		children.clear();
		maxLength.clear();
		arrangeNext.clear();
		parent.clear();
		belong.clear();
		vector <MBasicBlock *> blocks;
		for (MBasicBlock *block : node->blocks) {
			blocks.emplace_back(block);
		}
		sort(blocks.begin(), blocks.end(), [&](MBasicBlock *u, MBasicBlock *v) {
			if (u->loopDepth != v->loopDepth) {
				return u->loopDepth < v->loopDepth;
			}
			return outEdge[u->id].size() < outEdge[v->id].size();
		});
		for (MBasicBlock *block : blocks) {
			MInst *inst = block->insts.last();
			int pid = -1;
			if (inst->instType() == TJInst) {
				pid = label2block[inst->operands[0]->label]->id;
			}
			if (getBelong(pid) == getBelong(block->id)) {
				pid = -1;
			}
			parent[block->id] = pid;
			belong[block->id] = pid;
			children[pid].emplace_back(block->id);
		}
		calcMaxLength(-1);
		for (MBasicBlock *block : node->blocks) {
			if (children[block->id].size() == 0) {
				MBasicBlock *cur = block;
				while (arrangeNext.count(cur->id) > 0) {
					cur->insts.last()->remove();
					MBasicBlock *nextBlock = id2block[arrangeNext[cur->id]];
					curFunc->remove(nextBlock);
					curFunc->blocks.insertAfter(cur, nextBlock);
					cur = nextBlock;
				}
			}
		}
	}

	void visitMFunction(MFunction *node) {
		curFunc = node;
		buildCFG(node);
		redirectSingleJump(node);
		mergeBlock(node);
		jumpReorder(node);
		rearrangeBlock(node);
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