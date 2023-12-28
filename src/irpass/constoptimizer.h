/*
# const optimizer
=================

this pass optimizes:
+ operations with const number literals
+ phi instructions
  + has only one predecessor
  + with same operands
+ basic blocks
  + merge adjacent basic blocks
  + redirect jumps
  + remove condition when targets are same
*/

#ifndef __CPL_CONST_OPTIMIZER_H__
#define __CPL_CONST_OPTIMIZER_H__

#include <vector>

#include "../ir.h"
#include "cfgbuilder.h"
#include "reglabeller.h"


namespace IR {

namespace Passes {

using namespace std;
using namespace IR;


class ConstOptimizer : public Pass {
public:
	CFGBuilder cfgBuilder;
	RegLabeller regLabeller;

	Module *module;

	void replaceReg(Value *oldReg, Value *newReg) {
		for (Use *use : oldReg->uses) {
			use->user->setValue(use->index, newReg);
		}
	}

	void visitBasicBlock(BasicBlock *node) {
		for (Inst *inst : node->insts) {
			inst->accept(*this);
		}
	}

	void visitFunction(Function *node) {
		for (BasicBlock *block : node->blocks) {
			block->accept(*this);
		}
	}

	Inst *getDefineInst(Value *value) {
		if (value->name.length() > 0) {
			return nullptr;
		}
		for (Use *use : value->uses) {
			if (use->index != 0) {
				continue;
			}
			Inst *inst = (Inst *)use->user;
			if (inst->noDef) {
				continue;
			}
			return inst;
		}
		return nullptr;
	}

	void visitAddInst(AddInst *node) {
		Value *arg1 = node->values[1]->value;
		Value *arg2 = node->values[2]->value;
		if (arg1->isConst() && arg2->isConst()) {
			NumberLiteral *value = new NumberLiteral(arg1->getConstValue() + arg2->getConstValue());
			replaceReg(node->values[0]->value, value);
			node->remove();
			module->changed = true;
			return;
		}

		// 0 + a = a
		if (arg1->isConst() && arg1->getConstValue() == 0) {
			replaceReg(node->values[0]->value, arg2);
			node->remove();
			module->changed = true;
			return;
		}

		// a + 0 = a
		if (arg2->isConst() && arg2->getConstValue() == 0) {
			replaceReg(node->values[0]->value, arg1);
			node->remove();
			module->changed = true;
			return;
		}

		// const + a = a + const
		if (arg1->isConst()) {
			swap(node->values[1], node->values[2]);
			node->relabel();
			module->changed = true;
			return;
		}

		// a + const1 + const2 = a + (const1 + const2)
		if (arg2->isConst()) {
			Inst *inst = getDefineInst(arg1);
			if (inst && inst->instType() == TAddInst) {
				Value *instArg2 = inst->values[2]->value;
				if (instArg2->isConst()) {
					NumberLiteral *value = new NumberLiteral(arg2->getConstValue() + instArg2->getConstValue());
					Value *reg = node->values[0]->value;
					node->replaceWith(new AddInst(node->type, reg, inst->values[1]->value, value));
					module->changed = true;
					return;
				}
			}
		}
	}

	void visitSubInst(SubInst *node) {
		Value *arg1 = node->values[1]->value;
		Value *arg2 = node->values[2]->value;
		if (arg1->isConst() && arg2->isConst()) {
			NumberLiteral *value = new NumberLiteral(arg1->getConstValue() - arg2->getConstValue());
			replaceReg(node->values[0]->value, value);
			node->remove();
			module->changed = true;
			return;
		}

		// a - 0 = a
		if (arg2->isConst() && arg2->getConstValue() == 0) {
			replaceReg(node->values[0]->value, arg1);
			node->remove();
			module->changed = true;
			return;
		}

		// a - a = 0
		if (arg1->id == arg2->id) {
			NumberLiteral *value = new NumberLiteral(0);
			replaceReg(node->values[0]->value, value);
			node->remove();
			module->changed = true;
			return;
		}

		// a - const = a + (-const)
		if (arg2->isConst()) {
			NumberLiteral *value = new NumberLiteral(-arg2->getConstValue());
			Value *reg = node->values[0]->value;
			node->replaceWith(new AddInst(node->type, reg, node->values[1]->value, value));
			module->changed = true;
			return;
		}
	}

	void visitMulInst(MulInst *node) {
		Value *arg1 = node->values[1]->value;
		Value *arg2 = node->values[2]->value;
		if (arg1->isConst() && arg2->isConst()) {
			NumberLiteral *value = new NumberLiteral(arg1->getConstValue() * arg2->getConstValue());
			replaceReg(node->values[0]->value, value);
			node->remove();
			module->changed = true;
			return;
		}

		// 0 * a = 0
		if (arg1->isConst() && arg1->getConstValue() == 0) {
			replaceReg(node->values[0]->value, new NumberLiteral(0));
			node->remove();
			module->changed = true;
			return;
		}

		// a * 0 = 0
		if (arg2->isConst() && arg2->getConstValue() == 0) {
			replaceReg(node->values[0]->value, new NumberLiteral(0));
			node->remove();
			module->changed = true;
			return;
		}

		// 1 * a = a
		if (arg1->isConst() && arg1->getConstValue() == 1) {
			replaceReg(node->values[0]->value, arg2);
			node->remove();
			module->changed = true;
			return;
		}

		// a * 1 = a
		if (arg2->isConst() && arg2->getConstValue() == 1) {
			replaceReg(node->values[0]->value, arg1);
			node->remove();
			module->changed = true;
			return;
		}

		// const * a = a * const
		if (arg1->isConst()) {
			swap(node->values[1], node->values[2]);
			node->relabel();
			module->changed = true;
			return;
		}

		// a * const1 * const2 = a * (const1 * const2)
		if (arg2->isConst()) {
			Inst *inst = getDefineInst(arg1);
			if (inst && inst->instType() == TMulInst) {
				Value *instArg2 = inst->values[2]->value;
				if (instArg2->isConst()) {
					NumberLiteral *value = new NumberLiteral(arg2->getConstValue() * instArg2->getConstValue());
					Value *reg = node->values[0]->value;
					node->replaceWith(new MulInst(node->type, reg, inst->values[1]->value, value));
					module->changed = true;
					return;
				}
			}
		}

		// (a + const1) * const2 = a * const2 + const1 * const2
		if (arg2->isConst()) {
			Inst *inst = getDefineInst(arg1);
			if (inst && inst->instType() == TAddInst) {
				Value *instArg2 = inst->values[2]->value;
				if (instArg2->isConst()) {
					NumberLiteral *value = new NumberLiteral(arg2->getConstValue() * instArg2->getConstValue());
					Value *reg = node->values[0]->value;
					Value *mulReg = new Value(node->type);
					node->insertBefore(new MulInst(node->type, mulReg, inst->values[1]->value, node->values[2]->value));
					node->replaceWith(new AddInst(node->type, reg, mulReg, value));
					module->changed = true;
					return;
				}
			}
		}
	}

	void visitSdivInst(SdivInst *node) {
		Value *arg1 = node->values[1]->value;
		Value *arg2 = node->values[2]->value;
		if (arg1->isConst() && arg2->isConst()) {
			NumberLiteral *value = new NumberLiteral(arg2->getConstValue() == 0 ? 0 : arg1->getConstValue() / arg2->getConstValue());
			replaceReg(node->values[0]->value, value);
			node->remove();
			module->changed = true;
			return;
		}

		// a / 1 = a
		if (arg2->isConst() && arg2->getConstValue() == 1) {
			replaceReg(node->values[0]->value, arg1);
			node->remove();
			module->changed = true;
			return;
		}

		// a / a = 1
		if (arg1->id == arg2->id) {
			NumberLiteral *value = new NumberLiteral(1);
			replaceReg(node->values[0]->value, value);
			node->remove();
			module->changed = true;
			return;
		}

		// a / const1 / const2 = a / (const1 * const2)
		if (arg2->isConst()) {
			Inst *inst = getDefineInst(arg1);
			if (inst && inst->instType() == TSdivInst) {
				Value *instArg2 = inst->values[2]->value;
				if (instArg2->isConst()) {
					NumberLiteral *value = new NumberLiteral(arg2->getConstValue() * instArg2->getConstValue());
					Value *reg = node->values[0]->value;
					node->replaceWith(new SdivInst(node->type, reg, inst->values[1]->value, value));
					module->changed = true;
					return;
				}
			}
		}
	}

	void visitSremInst(SremInst *node) {
		Value *arg1 = node->values[1]->value;
		Value *arg2 = node->values[2]->value;
		if (arg1->isConst() && arg2->isConst()) {
			NumberLiteral *value = new NumberLiteral(arg2->getConstValue() == 0 ? 0 : arg1->getConstValue() % arg2->getConstValue());
			replaceReg(node->values[0]->value, value);
			node->remove();
			module->changed = true;
			return;
		}

		// a % 1 = 0
		if (arg2->isConst() && arg2->getConstValue() == 1) {
			NumberLiteral *value = new NumberLiteral(0);
			replaceReg(node->values[0]->value, value);
			node->remove();
			module->changed = true;
			return;
		}
	}

	void visitIcmpInst(IcmpInst *node) {
		Value *arg1 = node->values[1]->value;
		Value *arg2 = node->values[2]->value;
		if (arg1->isConst() && arg2->isConst()) {
			int cond = 0;
			if (node->cond == CondEq) {
				cond = arg1->getConstValue() == arg2->getConstValue();
			}
			if (node->cond == CondNe) {
				cond = arg1->getConstValue() != arg2->getConstValue();
			}
			if (node->cond == CondSgt) {
				cond = arg1->getConstValue() > arg2->getConstValue();
			}
			if (node->cond == CondSge) {
				cond = arg1->getConstValue() >= arg2->getConstValue();
			}
			if (node->cond == CondSlt) {
				cond = arg1->getConstValue() < arg2->getConstValue();
			}
			if (node->cond == CondSle) {
				cond = arg1->getConstValue() <= arg2->getConstValue();
			}
			NumberLiteral *value = new NumberLiteral(cond);
			replaceReg(node->values[0]->value, value);
			node->remove();
			module->changed = true;
			return;
		}

		// %int = zext i1 %bool to i32
		// %cond = icmp ne i32 0, %int
		// => %cond = %bool
		if (node->cond == CondNe
			&& arg1->isConst()
			&& arg1->getConstValue() == 0
			&& !arg2->uses.empty()) {
			Inst *inst = getDefineInst(arg2);
			if (inst
				&& inst->instType() == TZextInst
				&& inst->type == TInt1
				&& inst->type2 == TInt32) {
				replaceReg(node->values[0]->value, inst->values[1]->value);
				inst->remove();
				node->remove();
				module->changed = true;
				return;
			}
		}

		// %int = zext i1 %bool to i32
		// %cond = icmp ne i32 %int, 0
		// => %cond = %bool
		if (node->cond == CondNe
			&& arg2->isConst()
			&& arg2->getConstValue() == 0
			&& !arg1->uses.empty()) {
			Inst *inst = getDefineInst(arg1);
			if (inst
				&& inst->instType() == TZextInst
				&& inst->type == TInt1
				&& inst->type2 == TInt32) {
				replaceReg(node->values[0]->value, inst->values[1]->value);
				inst->remove();
				node->remove();
				module->changed = true;
				return;
			}
		}
	}

	void visitGetPtrInst(GetPtrInst *node) {
		// merge gep inst with constant index
		bool isConstIndex = true;
		for (int i = 2; i < node->values.size(); i++) {
			if (!(*node)[i]->isConst()) {
				isConstIndex = false;
				break;
			}
		}
		if (isConstIndex) {
			Inst *defInst = getDefineInst((*node)[1]);
			if (defInst && defInst->instType() == TGetPtrInst) {
				GetPtrInst *gepInst = (GetPtrInst *)defInst;
				for (int i = 2; i < gepInst->values.size(); i++) {
					if (!(*gepInst)[i]->isConst()) {
						isConstIndex = false;
						break;
					}
				}
				if (isConstIndex) {
					vector <int> index;
					for (int i = 2; i < gepInst->values.size(); i++) {
						index.emplace_back((*gepInst)[i]->getConstValue());
					}
					index[(int)index.size() - 1] += (*node)[2]->getConstValue();
					for (int i = 3; i < node->values.size(); i++) {
						index.emplace_back((*node)[i]->getConstValue());
					}
					for (int i = 0; i < index.size(); i++) {
						if (i + 2 < node->values.size()) {
							node->setValue(i + 2, new NumberLiteral(index[i]));
						} else {
							node->appendValue(new NumberLiteral(index[i]));
						}
					}
					node->setValue(1, (*gepInst)[1]);
					node->type = gepInst->type;
					node->type2 = gepInst->type2;
					module->changed = true;
					return;
				}
			}
		}
	}

	void visitPhiInst(PhiInst *node) {
		if (node->block->jumpFrom.size() == 1) {
			replaceReg(node->values[0]->value, node->values[1]->value);
			node->remove();
			module->changed = true;
			return;
		}
		if (node->block->jumpFrom.size() > 1) {
			bool isSame = true;
			Value *def = node->values[0]->value;
			Value *value = node->values[1]->value;
			for (int i = 1; i < node->values.size(); i += 2) {
				Value *cur = node->values[i]->value;
				if (value == def) {
					value = cur;
					continue;
				}
				if (cur == def) {
					continue;
				}
				if (value->isConst() && cur->isConst()) {
					if (value->getConstValue() != cur->getConstValue()) {
						isSame = false;
						break;
					}
					continue;
				}
				if (value->isConst() || cur->isConst()) {
					isSame = false;
					break;
				}
				if (value->regId != cur->regId) {
					isSame = false;
					break;
				}
			}
			if (isSame) {
				replaceReg(node->values[0]->value, value);
				node->remove();
				module->changed = true;
				return;
			}
		}
	}

	void removePhiEntry(BasicBlock *block, BasicBlock *from) {
		for (Inst *inst : block->insts) {
			if (inst->instType() != TPhiInst) {
				break;
			}
			Use *value = nullptr;
			Use *label = nullptr;
			for (int i = 1; i < inst->values.size(); i += 2) {
				if (inst->values[i + 1]->value == from) {
					value = inst->values[i];
					label = inst->values[i + 1];
					break;
				}
			}
			inst->removeValue(value);
			inst->removeValue(label);
		}
	}

	void removeCFGEdge(BasicBlock *from, BasicBlock *to) {
		for (int i = 0; i < from->jumpTo.size(); i++)
		{
			if (from->jumpTo[i] == to) {
				from->jumpTo.erase(from->jumpTo.begin() + i);
				i--;
			}
		}
		for (int i = 0; i < to->jumpFrom.size(); i++)
		{
			if (to->jumpFrom[i] == from) {
				to->jumpFrom.erase(to->jumpFrom.begin() + i);
				i--;
			}
		}
	}

	void mergeBlock(BasicBlock *from, BasicBlock *to) {
		from->jumpTo = to->jumpTo;
		for (BasicBlock *block : to->jumpTo) {
			for (int i = 0; i < block->jumpFrom.size(); i++) {
				if (block->jumpFrom[i] == to) {
					block->jumpFrom[i] = from;
				}
			}
		}
		to->jumpFrom.clear();
		to->jumpTo.clear();
		Inst *br = from->insts.last();
		br->remove();
		replaceReg(to, from);
		Inst *lastInst = nullptr;
		for (Inst *inst : to->insts) {
			if (lastInst) {
				to->remove(lastInst);
				lastInst->block = from;
				from->append(lastInst);
			}
			lastInst = inst;
		}
		to->remove(lastInst);
		lastInst->block = from;
		from->append(lastInst);
		to->func->remove(to);
	}

	void visitBrInst(BrInst *node) {
		if (!node->hasCond) {
			BasicBlock *block = node->block->jumpTo[0];
			if (block->jumpFrom.size() == 1
				&& node->block != block
				&& block->insts.first()->instType() != TPhiInst) {
				mergeBlock(node->block, block);
				module->changed = true;
				return;
			}
			if (node->block != node->block->func->blocks.first()
				&& node->block->insts.first() == node
				&& block->insts.first()->instType() != TPhiInst) {
				replaceReg(node->block, block);
				removeCFGEdge(node->block, block);
				for (BasicBlock *from : node->block->jumpFrom) {
					for (int i = 0; i < from->jumpTo.size(); i++) {
						if (from->jumpTo[i] == node->block) {
							from->jumpTo[i] = block;
						}
					}
					block->jumpFrom.emplace_back(from);
				}
				node->block->jumpFrom.clear();
				node->block->jumpTo.clear();
				node->block->func->remove(node->block);
				module->changed = true;
				return;
			}
			return;
		}
		Value *cond = node->values[0]->value;
		Value *trueBranch = node->values[1]->value;
		Value *falseBranch = node->values[2]->value;
		if (trueBranch == falseBranch) {
			node->replaceWith(new BrInst(trueBranch));
			module->changed = true;
			return;
		}
		if (cond->isConst()) {
			if (cond->getConstValue()) {
				removePhiEntry((BasicBlock *)falseBranch, node->block);
				removeCFGEdge(node->block, (BasicBlock *)falseBranch);
				node->replaceWith(new BrInst(trueBranch));
			} else {
				removePhiEntry((BasicBlock *)trueBranch, node->block);
				removeCFGEdge(node->block, (BasicBlock *)trueBranch);
				node->replaceWith(new BrInst(falseBranch));
			}
			module->changed = true;
			return;
		}
	}

	void visitModule(Module *node) {
		node->accept(cfgBuilder);
		node->accept(regLabeller);

		module = node;
		for (Function *func : node->funcs) {
			func->accept(*this);
		}
	}
};

}

}

#endif