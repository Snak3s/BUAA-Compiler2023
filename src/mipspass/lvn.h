#ifndef __CPL_MIPS_LVN_H__
#define __CPL_MIPS_LVN_H__

#include <vector>
#include <map>
#include <algorithm>

#include "../mips.h"
#include "../exprhash.h"


namespace MIPS {

namespace Passes {

using namespace std;
using namespace MIPS;
using namespace ExprHash;


class LVN : public Pass {
public:
	MModule *module;


	HashItem *R(Register *reg) {
		if (reg->type == RImmediate) {
			return new HashConst(reg->immediate);
		}
		if (reg->type == RVirtual) {
			return new HashReg(reg->value->id);
		}
		return new HashReg(-reg->type.id);
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


	map <int, vector <pair <HashItem *, Register *>>> hashs;

	void setHash(vector <HashItem *> items, Register *node, MInst *inst) {
		for (HashItem *item : items) {
			if (hashs.count(item->hashValue)) {
				for (auto pair : hashs[item->hashValue]) {
					if (item->equals(pair.first)) {
						inst->replaceWith(new AddInst(node, ZERO, pair.second));
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


	void visitMBasicBlock(MBasicBlock *node) {
		hashs.clear();
		HashSet::clear();
		HashArray::clear();
		HashConst::clear();
		HashReg::clear();
		for (MInst *inst : node->insts) {
			inst->accept(*this);
		}
	}

	void visitMFunction(MFunction *node) {
		for (MBasicBlock *block : node->blocks) {
			block->accept(*this);
		}
	}

	void visitAdduInst(AdduInst *node) {
		if ((*node)[1]->type != RVirtual && (*node)[1]->type != RImmediate) {
			return;
		}
		if ((*node)[2]->type != RVirtual && (*node)[2]->type != RImmediate) {
			return;
		}
		setHash({
			A({ T(TAdduInst), S({ R((*node)[1]), R((*node)[2]) }) })
		}, (*node)[0], node);
	}

	void visitSubuInst(SubuInst *node) {
		if ((*node)[1]->type != RVirtual && (*node)[1]->type != RImmediate) {
			return;
		}
		if ((*node)[2]->type != RVirtual && (*node)[2]->type != RImmediate) {
			return;
		}
		setHash({
			A({ T(TSubuInst), R((*node)[1]), R((*node)[2]) })
		}, (*node)[0], node);
	}

	void visitMulInst(MulInst *node) {
		if ((*node)[1]->type != RVirtual && (*node)[1]->type != RImmediate) {
			return;
		}
		if ((*node)[2]->type != RVirtual && (*node)[2]->type != RImmediate) {
			return;
		}
		setHash({
			A({ T(TMulInst), S({ R((*node)[1]), R((*node)[2]) }) })
		}, (*node)[0], node);
	}

	void visitDivInst(DivInst *node) {
		if ((*node)[1]->type != RVirtual && (*node)[1]->type != RImmediate) {
			return;
		}
		if ((*node)[2]->type != RVirtual && (*node)[2]->type != RImmediate) {
			return;
		}
		setHash({
			A({ T(TDivInst), R((*node)[1]), R((*node)[2]) })
		}, (*node)[0], node);
	}

	void visitRemInst(RemInst *node) {
		if ((*node)[1]->type != RVirtual && (*node)[1]->type != RImmediate) {
			return;
		}
		if ((*node)[2]->type != RVirtual && (*node)[2]->type != RImmediate) {
			return;
		}
		setHash({
			A({ T(TRemInst), R((*node)[1]), R((*node)[2]) })
		}, (*node)[0], node);
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