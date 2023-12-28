#ifndef __CPL_CFG_BUILDER_H__
#define __CPL_CFG_BUILDER_H__

#include <vector>

#include "../ir.h"


namespace IR {

namespace Passes {

using namespace std;
using namespace IR;


class CFGBuilder : public Pass {
public:
	void appendEdge(BasicBlock *from, BasicBlock *to) {
		from->jumpTo.emplace_back(to);
		to->jumpFrom.emplace_back(from);
	}

	void appendEdge(Function *caller, Function *callee) {
		caller->asCaller.emplace_back(callee);
		callee->asCallee.emplace_back(caller);
	}


	void visitBasicBlock(BasicBlock *node) {
		for (Inst *inst : node->insts) {
			inst->accept(*this);
		}
	}

	void visitFunction(Function *node) {
		for (BasicBlock *block : node->blocks) {
			block->jumpFrom.clear();
			block->jumpTo.clear();
		}
		for (BasicBlock *block : node->blocks) {
			block->accept(*this);
		}
	}

	void visitBrInst(BrInst *node) {
		if (node->hasCond) {
			appendEdge(node->block, (BasicBlock *)node->values[1]->value);
			appendEdge(node->block, (BasicBlock *)node->values[2]->value);
		} else {
			appendEdge(node->block, (BasicBlock *)node->values[0]->value);
		}
	}

	void visitCallInst(CallInst *node) {
		if (node->noDef) {
			appendEdge(node->block->func, (Function *)node->values[0]->value);
		} else {
			appendEdge(node->block->func, (Function *)node->values[1]->value);
		}
	}

	void visitModule(Module *node) {
		for (Function *func : node->funcs) {
			func->asCallee.clear();
			func->asCaller.clear();
		}
		for (Function *func : node->funcs) {
			func->accept(*this);
		}
	}
};

}

}

#endif