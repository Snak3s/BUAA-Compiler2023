/*
# dead code elimination
=======================

this pass removes def instructions with no uses
*/

#ifndef __CPL_MIPS_DCE_H__
#define __CPL_MIPS_DCE_H__

#include <map>
#include <set>

#include "../mips.h"


namespace MIPS {

namespace Passes {

using namespace std;
using namespace MIPS;


class DCE : public Pass {
public:
	MModule *module;

	map <Register *, set <MInst *>, RegisterPtrComp> uses;

	void visitMFunction(MFunction *node) {
		uses.clear();
		for (MBasicBlock *block : node->blocks) {
			for (MInst *inst : block->insts) {
				for (int i = inst->noDef ? 0 : 1; i < inst->operands.size(); i++) {
					if ((*inst)[i]->type != RVirtual) {
						continue;
					}
					uses[(*inst)[i]].insert(inst);
				}
			}
		}
		for (MBasicBlock *block : node->blocks) {
			for (MInst *inst : block->insts) {
				if (inst->noDef || inst->terminate) {
					continue;
				}
				if ((*inst)[0]->type != RVirtual) {
					continue;
				}
				if (uses[(*inst)[0]].size() == 0) {
					inst->remove();
				}
			}
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