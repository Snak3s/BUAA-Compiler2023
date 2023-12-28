/*
# local value numbering
=======================

this pass combines def instructions with same operands in the same basic block
*/

#ifndef __CPL_LVN_H__
#define __CPL_LVN_H__

#include <vector>
#include <map>
#include <algorithm>

#include "../ir.h"
#include "../exprhash.h"


namespace IR {

namespace Passes {

using namespace std;
using namespace IR;
using namespace ExprHash;


class LVN : public Pass {
public:
	Module *module;


	HashItem *R(Value *value) {
		if (value->isConst()) {
			return new HashConst(value->getConstValue());
		}
		return new HashReg(value->id);
	}

	HashItem *S(const vector <HashItem *> &items) {
		HashSet *hSet = new HashSet();
		for (HashItem *item : items) {
			hSet->items.emplace_back(item);
		}
		hSet->calcHashValue();
		return hSet;
	}

	HashItem *A(const vector <HashItem *> &items) {
		HashArray *hArray = new HashArray();
		for (HashItem *item : items) {
			hArray->items.emplace_back(item);
		}
		hArray->calcHashValue();
		return hArray;
	}

	HashItem *T(const Type &type) {
		return new HashConst(type.id);
	}


	map <int, vector <pair <HashItem *, Value *>>> hashs;

	void replaceReg(Value *oldReg, Value *newReg) {
		for (Use *use : oldReg->uses) {
			use->user->setValue(use->index, newReg);
		}
	}

	void setHash(vector <HashItem *> items, Value *node, Inst *inst) {
		for (HashItem *item : items) {
			if (hashs.count(item->hashValue)) {
				for (auto pair : hashs[item->hashValue]) {
					if (item->equals(pair.first)) {
						replaceReg(node, pair.second);
						inst->remove();
						module->changed = true;
						return;
					}
				}
			}
		}
		for (HashItem *item : items) {
			hashs[item->hashValue].emplace_back(make_pair(item, node));
		}
	}


	void visitBasicBlock(BasicBlock *node) {
		hashs.clear();
		HashSet::clear();
		HashArray::clear();
		HashConst::clear();
		HashReg::clear();
		for (Inst *inst : node->insts) {
			inst->accept(*this);
		}
	}

	void visitFunction(Function *node) {
		for (BasicBlock *block : node->blocks) {
			block->accept(*this);
		}
	}

	void visitAddInst(AddInst *node) {
		setHash({
			A({ T(TAddInst), S({ R((*node)[1]), R((*node)[2]) }) })
		}, (*node)[0], node);
	}

	void visitSubInst(SubInst *node) {
		setHash({
			A({ T(TSubInst), R((*node)[1]), R((*node)[2]) })
		}, (*node)[0], node);
	}

	void visitMulInst(MulInst *node) {
		setHash({
			A({ T(TMulInst), S({ R((*node)[1]), R((*node)[2]) }) })
		}, (*node)[0], node);
	}

	void visitSdivInst(SdivInst *node) {
		setHash({
			A({ T(TSdivInst), R((*node)[1]), R((*node)[2]) })
		}, (*node)[0], node);
	}

	void visitSremInst(SremInst *node) {
		setHash({
			A({ T(TSremInst), R((*node)[1]), R((*node)[2]) })
		}, (*node)[0], node);
	}

	void visitIcmpInst(IcmpInst *node) {
		setHash({
			A({ T(TIcmpInst), T(node->cond), R((*node)[1]), R((*node)[2]) })
		}, (*node)[0], node);
	}

	void visitGetPtrInst(GetPtrInst *node) {
		vector <HashItem *> items;
		for (int i = 1; i < node->values.size(); i++) {
			items.emplace_back(R((*node)[i]));
		}
		setHash({
			A({ T(TGetPtrInst), A(items) })
		}, (*node)[0], node);
	}

	void visitPhiInst(PhiInst *node) {
		vector <HashItem *> items;
		for (int i = 1; i < node->values.size(); i += 2) {
			items.emplace_back(A({ R((*node)[i]), R((*node)[i + 1]) }));
		}
		setHash({
			A({ T(TPhiInst), S(items) })
		}, (*node)[0], node);
	}

	void visitZextInst(ZextInst *node) {
		// assume i1 to i32
		setHash({
			A({ T(TZextInst), R((*node)[1]) })
		}, (*node)[0], node);
	}

	void visitTruncInst(TruncInst *node) {
		// assume i32 to i1
		setHash({
			A({ T(TTruncInst), R((*node)[1]) })
		}, (*node)[0], node);
	}

	void visitModule(Module *node) {
		module = node;
		for (Function *func : node->funcs) {
			func->accept(*this);
		}
	}
};

}

}

#endif