/*
# peephole optimization
=======================

*/

#ifndef __CPL_PEEPHOLE_H__
#define __CPL_PEEPHOLE_H__

#include <vector>

#include "../mips.h"


namespace MIPS {

namespace Passes {

using namespace std;
using namespace MIPS;


class Peephole : public Pass {
public:
	MModule *module;
	MFunction *curFunc = nullptr;

	bool is16Bits(int value) {
		return -32768 <= value && value < 32768;
	}

	void visitMBasicBlock(MBasicBlock *node) {
		for (MInst *inst : node->insts) {
			inst->accept(*this);
		}
	}

	void visitMFunction(MFunction *node) {
		curFunc = node;
		for (MBasicBlock *block : node->blocks) {
			block->accept(*this);
		}
	}

	void visitAddInst(AddInst *node) {
		// add $same, $same, $zero
		// -> remove
		if ((*node)[0] == (*node)[1] && (*node)[2] == ZERO) {
			node->remove();
			module->changed = true;
			return;
		}

		// add $same, $same, 0
		// -> remove
		if ((*node)[0] == (*node)[1]
			&& (*node)[2]->type == RImmediate
			&& (*node)[2]->immediate == 0) {
			node->remove();
			module->changed = true;
			return;
		}
	}

	void visitAdduInst(AdduInst *node) {
		// addu $same, $same, $zero
		// -> remove
		if ((*node)[0] == (*node)[1] && (*node)[2] == ZERO) {
			node->remove();
			module->changed = true;
			return;
		}

		// addu $reg, $reg, 0
		// -> addu $reg, $reg, $zero
		if ((*node)[2]->type == RImmediate
			&& (*node)[2]->immediate == 0) {
			node->operands[2] = ZERO;
			module->changed = true;
			return;
		}

		// addu $reg, $reg, immediate
		// -> addiu $reg, $reg, immediate
		if ((*node)[0]->type != RVirtual
			&& (*node)[1]->type != RVirtual
			&& (*node)[2]->type == RImmediate) {
			node->replaceWith(new AddiuInst((*node)[0], (*node)[1], (*node)[2]));
			module->changed = true;
			return;
		}
	}

	void visitSubInst(SubInst *node) {
		// sub $same, $same, $zero
		// -> remove
		if ((*node)[0] == (*node)[1] && (*node)[2] == ZERO) {
			node->remove();
			module->changed = true;
			return;
		}
	}

	void visitSubuInst(SubuInst *node) {
		// subu $same, $same, $zero
		// -> remove
		if ((*node)[0] == (*node)[1] && (*node)[2] == ZERO) {
			node->remove();
			module->changed = true;
			return;
		}
	}

	void visitSeqInst(SeqInst *node) {
		// seq $virtual, $op1, $op2
		// beq $virtual, $zero, target
		// -> bne $op1, $op2, target
		if ((*node)[0]->type != RVirtual) {
			return;
		}
		for (MBasicBlock *block : curFunc->blocks) {
			for (auto it = block->insts.rbegin(); it != block->insts.rend(); --it) {
				MInst *inst = *it;
				if (!inst->terminate) {
					break;
				}
				if (inst->instType() == TBeqInst
					&& (*node)[0] == (*inst)[0]
					&& (*inst)[1] == ZERO) {
					inst->replaceWith(new BneInst((*node)[1], (*node)[2], (*inst)[2]));
					module->changed = true;
				}
			}
		}
	}

	void visitSneInst(SneInst *node) {
		// sne $virtual, $op1, $op2
		// beq $virtual, $zero, target
		// -> beq $op1, $op2, target
		if ((*node)[0]->type != RVirtual) {
			return;
		}
		for (MBasicBlock *block : curFunc->blocks) {
			for (auto it = block->insts.rbegin(); it != block->insts.rend(); --it) {
				MInst *inst = *it;
				if (!inst->terminate) {
					break;
				}
				if (inst->instType() == TBeqInst
					&& (*node)[0] == (*inst)[0]
					&& (*inst)[1] == ZERO) {
					inst->replaceWith(new BeqInst((*node)[1], (*node)[2], (*inst)[2]));
					module->changed = true;
				}
			}
		}
	}

	void visitSgtInst(SgtInst *node) {
		// sgt $virtual, $op, 0
		// beq $virtual, $zero, target
		// -> blez $op, target
		if ((*node)[0]->type != RVirtual) {
			return;
		}
		if (((*node)[2]->type == RImmediate && (*node)[2]->immediate == 0)
			|| (*node)[2] == ZERO) {
			for (MBasicBlock *block : curFunc->blocks) {
				for (auto it = block->insts.rbegin(); it != block->insts.rend(); --it) {
					MInst *inst = *it;
					if (!inst->terminate) {
						break;
					}
					if (inst->instType() == TBeqInst
						&& (*node)[0] == (*inst)[0]
						&& (*inst)[1] == ZERO) {
						inst->replaceWith(new BlezInst((*node)[1], (*inst)[2]));
						module->changed = true;
					}
				}
			}
		}

		// sgt $virtual, $op1, $op2
		// -> slt $virtual, $op2, $op1
		if ((*node)[2]->type == RVirtual) {
			node->replaceWith(new SltInst((*node)[0], (*node)[2], (*node)[1]));
			module->changed = true;
			return;
		}
	}

	void visitSgeInst(SgeInst *node) {
		// sge $virtual, $op, 0
		// beq $virtual, $zero, target
		// -> bltz $op, target
		if ((*node)[0]->type != RVirtual) {
			return;
		}
		if (((*node)[2]->type == RImmediate && (*node)[2]->immediate == 0)
			|| (*node)[2] == ZERO) {
			for (MBasicBlock *block : curFunc->blocks) {
				for (auto it = block->insts.rbegin(); it != block->insts.rend(); --it) {
					MInst *inst = *it;
					if (!inst->terminate) {
						break;
					}
					if (inst->instType() == TBeqInst
						&& (*node)[0] == (*inst)[0]
						&& (*inst)[1] == ZERO) {
						inst->replaceWith(new BltzInst((*node)[1], (*inst)[2]));
						module->changed = true;
					}
				}
			}
		}

		// sge $virtual, $op1, $op2
		// -> sle $virtual, $op2, $op1
		if ((*node)[2]->type == RVirtual) {
			node->replaceWith(new SleInst((*node)[0], (*node)[2], (*node)[1]));
			module->changed = true;
			return;
		}
	}

	void visitSltiInst(SltiInst *node) {
		// slti $virtual, $op, 0
		// beq $virtual, $zero, target
		// -> bgez $op, target
		if ((*node)[0]->type != RVirtual) {
			return;
		}
		if (((*node)[2]->type == RImmediate && (*node)[2]->immediate == 0)
			|| (*node)[2] == ZERO) {
			for (MBasicBlock *block : curFunc->blocks) {
				for (auto it = block->insts.rbegin(); it != block->insts.rend(); --it) {
					MInst *inst = *it;
					if (!inst->terminate) {
						break;
					}
					if (inst->instType() == TBeqInst
						&& (*node)[0] == (*inst)[0]
						&& (*inst)[1] == ZERO) {
						inst->replaceWith(new BgezInst((*node)[1], (*inst)[2]));
						module->changed = true;
					}
				}
			}
		}
	}

	void visitSleInst(SleInst *node) {
		// sle $virtual, $op, 0
		// beq $virtual, $zero, target
		// -> bgtz $op, target
		if ((*node)[0]->type != RVirtual) {
			return;
		}
		if (((*node)[2]->type == RImmediate && (*node)[2]->immediate == 0)
			|| (*node)[2] == ZERO) {
			for (MBasicBlock *block : curFunc->blocks) {
				for (auto it = block->insts.rbegin(); it != block->insts.rend(); --it) {
					MInst *inst = *it;
					if (!inst->terminate) {
						break;
					}
					if (inst->instType() == TBeqInst
						&& (*node)[0] == (*inst)[0]
						&& (*inst)[1] == ZERO) {
						inst->replaceWith(new BgtzInst((*node)[1], (*inst)[2]));
						module->changed = true;
					}
				}
			}
		}

		// sle $virtual, $op1, $op2
		// beq $virtual, $zero, target
		// -> slt $virtual2, $op2, $op1
		// -> xori $virtual1, $virtual2, 1
		// -> bne $virtual2, $zero, target
		if ((*node)[2]->type == RVirtual) {
			Register *dest = (*node)[0];
			Register *sltDest = new Register();
			node->insertAfter(new XoriInst(dest, sltDest, new Register(1)));
			node->replaceWith(new SltInst(sltDest, (*node)[2], (*node)[1]));
			module->changed = true;
			for (MBasicBlock *block : curFunc->blocks) {
				for (auto it = block->insts.rbegin(); it != block->insts.rend(); --it) {
					MInst *inst = *it;
					if (!inst->terminate) {
						break;
					}
					if (inst->instType() == TBeqInst
						&& (*inst)[0] == dest
						&& (*inst)[1] == ZERO) {
						inst->replaceWith(new BneInst(sltDest, ZERO, (*inst)[2]));
						module->changed = true;
					} else if (inst->instType() == TBneInst
						&& (*inst)[0] == dest
						&& (*inst)[1] == ZERO) {
						inst->replaceWith(new BeqInst(sltDest, ZERO, (*inst)[2]));
						module->changed = true;
					}
				}
			}
			return;
		}

		// sle $virtual, $op, 2147483647
		// -> addiu $virtual, $zero, 1
		if ((*node)[2]->type == RImmediate
			&& (*node)[2]->immediate == 2147483647) {
			node->replaceWith(new AddiuInst((*node)[0], ZERO, new Register(1)));
			module->changed = true;
			return;
		}

		// sle $virtual, $op, immediate
		// -> slti $virtual, $op, (immediate + 1)
		// or li $immediate, (immediate + 1)
		//    slti $virtual, $op, $immediate
		if ((*node)[2]->type == RImmediate
			&& (*node)[2]->immediate < 2147483647) {
			int immediate = (*node)[2]->immediate + 1;
			Register *reg = new Register(immediate);
			if (!is16Bits(immediate)) {
				Register *liDest = new Register();
				node->insertBefore(new LiInst(liDest, reg));
				node->replaceWith(new SltInst((*node)[0], (*node)[1], liDest));
			} else {
				node->replaceWith(new SltiInst((*node)[0], (*node)[1], reg));
			}
			module->changed = true;
			return;
		}
	}

	void visitLaInst(LaInst *node) {
		// la $dest, immediate($virtual)
		// -> addiu $dest, $virtual, immediate
		if ((*node)[1]->type == RImmediate && (*node)[2]->type == RVirtual) {
			node->replaceWith(new AddiuInst((*node)[0], (*node)[2], (*node)[1]));
			module->changed = true;
			return;
		}
	}

	void visitMModule(MModule *node) {
		module = node;
		for (MFunction *func : node->funcs) {
			func->accept(*this);
		}
	}
};

}

}

#endif