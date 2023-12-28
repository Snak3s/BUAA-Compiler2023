/*
# remove $fp
============

this pass maintains offset of $sp and remove / replace $fp with $sp
*/

#ifndef __CPL_REMOVE_FP_H__
#define __CPL_REMOVE_FP_H__

#include <map>
#include <set>

#include "../mips.h"


namespace MIPS {

namespace Passes {

using namespace std;
using namespace MIPS;


class RemoveFp : public Pass {
public:
	MModule *module;

	int spOffset;

	void visitMBasicBlock(MBasicBlock *node) {
		for (MInst *inst : node->insts) {
			inst->accept(*this);
		}
	}

	void visitMFunction(MFunction *node) {
		spOffset = 0;
		for (MBasicBlock *block : node->blocks) {
			block->accept(*this);
		}
	}

	void visitAddInst(AddInst *node) {
		if ((*node)[0] == FP) {
			node->remove();
		}
		if ((*node)[0] == SP && (*node)[2]->type == RImmediate) {
			spOffset += (*node)[2]->immediate;
		}
	}

	void visitLaInst(LaInst *node) {
		if ((*node)[2] == FP) {
			int offset = (*node)[1]->immediate;
			offset -= spOffset;
			node->operands[1] = new Register(offset);
			node->operands[2] = SP;
			return;
		}
	}

	void visitLwInst(LwInst *node) {
		if ((*node)[2] == FP) {
			int offset = (*node)[1]->immediate;
			offset -= spOffset;
			node->operands[1] = new Register(offset);
			node->operands[2] = SP;
			return;
		}
	}

	void visitSwInst(SwInst *node) {
		if ((*node)[2] == FP) {
			int offset = (*node)[1]->immediate;
			offset -= spOffset;
			node->operands[1] = new Register(offset);
			node->operands[2] = SP;
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