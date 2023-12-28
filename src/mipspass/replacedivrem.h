/*
# replace div / rem
===================

this pass replaces div / rem with div + mflo / mfhi
*/

#ifndef __CPL_REPLACE_DIV_REM_H__
#define __CPL_REPLACE_DIV_REM_H__

#include <map>
#include <set>

#include "../mips.h"


namespace MIPS {

namespace Passes {

using namespace std;
using namespace MIPS;


class ReplaceDivRem : public Pass {
public:
	MModule *module;

	void visitMBasicBlock(MBasicBlock *node) {
		for (MInst *inst : node->insts) {
			inst->accept(*this);
		}
	}

	void visitMFunction(MFunction *node) {
		for (MBasicBlock *block : node->blocks) {
			block->accept(*this);
		}
	}

	void visitDivInst(DivInst *node) {
		if ((*node)[2]->type == RImmediate) {
			return;
		}
		node->insertAfter(new MfloInst((*node)[0]));
		node->replaceWith(new ExDivInst((*node)[1], (*node)[2]));
		module->changed = true;
	}

	void visitRemInst(RemInst *node) {
		if ((*node)[2]->type == RImmediate) {
			return;
		}
		node->insertAfter(new MfhiInst((*node)[0]));
		node->replaceWith(new ExDivInst((*node)[1], (*node)[2]));
		module->changed = true;
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