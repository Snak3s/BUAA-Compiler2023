/*
# inline functions
==================

this pass finds the functions with no recursive calls and inline them

the return value is saved in the phi instruction
note that this phi instruction has no corresponding variable in the scope
*/

#ifndef __CPL_INLINE_FUNC_H__
#define __CPL_INLINE_FUNC_H__

#include <vector>

#include "../ir.h"
#include "cfgbuilder.h"


namespace IR {

namespace Passes {

using namespace std;
using namespace IR;


class InlineFunc : public Pass {
public:
	CFGBuilder cfgBuilder;

	Module *module;
	Function *curFunc = nullptr;


	map <Value *, Value *> mapping;

	void inlineFunc(CallInst *inst, Function *callee) {
		mapping.clear();
		module->changed = true;

		// split basic block
		BasicBlock *curBlock = inst->block;
		BasicBlock *afterCall = curFunc->allocBasicBlockAfter(curBlock);
		auto instIt = curBlock->insts.rbegin();
		Inst *lastInst = nullptr;
		while (instIt != curBlock->insts.rend()) {
			if (lastInst != nullptr) {
				curBlock->remove(lastInst);
				afterCall->prepend(lastInst);
				lastInst->block = afterCall;
			}
			if (*instIt == inst) {
				break;
			}
			lastInst = *instIt;
			--instIt;
		}
		afterCall->jumpTo = curBlock->jumpTo;
		for (BasicBlock *to : afterCall->jumpTo) {
			for (Inst *inst : to->insts) {
				if (inst->instType() != TPhiInst) {
					break;
				}
				for (int i = 1; i < inst->values.size(); i += 2) {
					if ((*inst)[i + 1] == curBlock) {
						inst->values[i + 1]->setValue(afterCall);
					}
				}
			}
		}

		// insert phi instruction
		PhiInst *phi = nullptr;
		if (!inst->noDef) {
			phi = new PhiInst((*inst)[0]->type, (*inst)[0]);
			afterCall->prepend(phi);
			phi->block = afterCall;
		}

		// copy callee
		int index = 0;
		if (!inst->noDef) {
			index = 1;
		}
		for (int i = 0; i < callee->params.size(); i++) {
			mapping[callee->params[i]] = (*inst)[i + index + 1];
		}
		BasicBlock *lastBlock = curBlock;
		for (BasicBlock *srcBlock : callee->blocks) {
			BasicBlock *destBlock = curFunc->allocBasicBlockAfter(lastBlock);
			lastBlock = destBlock;
			mapping[srcBlock] = destBlock;
			mapping[srcBlock->label] = destBlock->label;
		}
		BasicBlock *inlineEntry = (BasicBlock *)mapping[callee->blocks.first()];
		for (BasicBlock *srcBlock : callee->blocks) {
			BasicBlock *destBlock = (BasicBlock *)mapping[srcBlock];
			for (Inst *srcInst : srcBlock->insts) {
				Inst *destInst = srcInst->copy();
				destBlock->append(destInst);
				destInst->block = destBlock;
				for (int i = 0; i < destInst->values.size(); i++) {
					Value *reg = (*destInst)[i];
					if (reg->regId != UNAVAILABLE) {
						if (mapping.count(reg) == 0) {
							mapping[reg] = new Value(reg->type);
						}
					}
					if (mapping.count(reg)) {
						destInst->values[i]->setValue(mapping[reg]);
					}
					if (reg->isConst()) {
						destInst->values[i]->setValue(new NumberLiteral(reg->getConstValue()));
					}
				}
				if (destInst->instType() == TRetInst) {
					if (inst->noDef) {
						destInst->replaceWith(new BrInst(afterCall));
					} else {
						Value *ret = (*destInst)[0];
						destInst->replaceWith(new BrInst(afterCall));
						phi->appendValue(ret);
						phi->appendValue(destBlock);
					}
				}
			}
		}
		inst->replaceWith(new BrInst(inlineEntry));
	}


	void visitBasicBlock(BasicBlock *node) {
		for (Inst *inst : node->insts) {
			inst->accept(*this);
		}
	}

	void visitFunction(Function *node) {
		curFunc = node;
		for (BasicBlock *block : node->blocks) {
			block->accept(*this);
		}
	}

	void visitCallInst(CallInst *node) {
		int index = 0;
		if (!node->noDef) {
			index = 1;
		}
		Function *func = (Function *)(node->values[index]->value);
		if (func->reserved) {
			return;
		}
		for (Function *callee : func->asCaller) {
			if (callee == func) {
				return;
			}
		}
		inlineFunc(node, func);
	}

	void visitModule(Module *node) {
		node->accept(cfgBuilder);

		module = node;
		for (Function *func : node->funcs) {
			func->accept(*this);
		}
	}
};

}

}

#endif