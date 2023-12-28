#ifndef __CPL_REG_LABELLER_H
#define __CPL_REG_LABELLER_H

#include <set>

#include "../ir.h"


namespace IR {

namespace Passes {

using namespace std;
using namespace IR;


class RegLabeller : public Pass {
public:
	int curRegId = 0;
	set <int> labelled;


	void label(Value *value) {
		if (value->regId == UNAVAILABLE) {
			return;
		}
		if (labelled.count(value->id)) {
			return;
		}
		labelled.insert(value->id);
		value->regId = curRegId++;
	}


	void visitBasicBlock(BasicBlock *node) {
		if (node->label) {
			label(node->label);
		}
		for (Inst *inst : node->insts) {
			visitInst(inst);
		}
	}

	void visitFunction(Function *node) {
		curRegId = 0;
		for (Value *param : node->params) {
			label(param);
		}
		for (BasicBlock *block : node->blocks) {
			block->accept(*this);
		}
	}

	void visitInst(Inst *node) {
		if (node->terminate || node->noDef) {
			return;
		}
		Use *use = node->values[0];
		if (use) {
			label(use->value);
		}
	}

	void visitModule(Module *node) {
		labelled.clear();
		for (Function *func : node->funcs) {
			func->accept(*this);
		}
	}
};

}

}

#endif