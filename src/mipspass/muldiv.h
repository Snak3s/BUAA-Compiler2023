/*
# multiply / division optimizer
===============================

*/

#ifndef __CPL_MUL_DIV_H__
#define __CPL_MUL_DIV_H__

#include <algorithm>

#include "../mips.h"


namespace MIPS {

namespace Passes {

using namespace std;
using namespace MIPS;


class MulDiv : public Pass {
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

	void visitMulInst(MulInst *node) {
		Register *reg = (*node)[2];
		if (reg->type != RImmediate) {
			return;
		}
		int immediate = reg->immediate;
		unsigned int absImmediate = abs(immediate);
		if (__builtin_popcount(absImmediate) == 1) {
			node->insertBefore(new SllInst((*node)[0], (*node)[1], new Register(__builtin_ctz(absImmediate))));
			if (immediate < 0) {
				node->insertBefore(new SubuInst((*node)[0], ZERO, (*node)[0]));
			}
			node->remove();
			module->changed = true;
			return;
		}
		if (__builtin_popcount(immediate) == 2) {
			Register *sll1 = new Register();
			node->insertBefore(new SllInst(sll1, (*node)[1], new Register(__builtin_ctz(immediate))));
			immediate ^= 1 << __builtin_ctz(immediate);
			Register *sll2 = new Register();
			node->insertBefore(new SllInst(sll2, (*node)[1], new Register(__builtin_ctz(immediate))));
			node->insertBefore(new AdduInst((*node)[0], sll2, sll1));
			node->remove();
			module->changed = true;
			return;
		}
		if (__builtin_popcount(immediate ^ (immediate >> 1)) == 1) {
			immediate ^= immediate >> 1;
			Register *sll = new Register();
			node->insertBefore(new SllInst(sll, (*node)[1], new Register(__builtin_ctz(immediate) + 1)));
			node->insertBefore(new SubuInst((*node)[0], sll, (*node)[1]));
			node->remove();
			module->changed = true;
			return;
		}
		if (__builtin_popcount(immediate ^ (immediate >> 1)) == 2) {
			immediate ^= immediate >> 1;
			Register *sll1 = new Register();
			node->insertBefore(new SllInst(sll1, (*node)[1], new Register(__builtin_ctz(immediate) + 1)));
			immediate ^= 1 << __builtin_ctz(immediate);
			Register *sll2 = new Register();
			node->insertBefore(new SllInst(sll2, (*node)[1], new Register(__builtin_ctz(immediate) + 1)));
			node->insertBefore(new SubuInst((*node)[0], sll2, sll1));
			node->remove();
			module->changed = true;
			return;
		}
	}

	void visitDivInst(DivInst *node) {
		Register *reg = (*node)[2];
		if (reg->type != RImmediate) {
			return;
		}
		int immediate = reg->immediate;
		unsigned int absImmediate = abs(immediate);

		// choose multiplier
		int len = 1;
		long long mult = 0;
		while (len < 31) {
			mult = ((1ll << (31 + len)) + (1ll << len)) / absImmediate;
			if (mult * absImmediate >= (1ll << (31 + len))) {
				break;
			}
			len++;
		}

		if (absImmediate == 1) {
			node->insertBefore(new AdduInst((*node)[0], ZERO, (*node)[1]));
		} else if (__builtin_popcount(absImmediate) == 1) {
			len = __builtin_ctz(absImmediate);
			node->insertBefore(new SraInst(AT, (*node)[1], new Register(len - 1)));
			node->insertBefore(new SrlInst(AT, AT, new Register(32 - len)));
			node->insertBefore(new AdduInst(AT, AT, (*node)[1]));
			node->insertBefore(new SraInst((*node)[0], AT, new Register(len)));
		} else if (mult < (1ll << 31)) {
			Register *temp = new Register();
			node->insertBefore(new LiInst(AT, new Register(mult)));
			node->insertBefore(new MultInst((*node)[1], AT));
			node->insertBefore(new MfhiInst(temp));
			if (len > 1) {
				Register *newTemp = new Register();
				node->insertBefore(new SraInst(newTemp, temp, new Register(len - 1)));
				temp = newTemp;
			}
			node->insertBefore(new SrlInst(AT, temp, new Register(31)));
			node->insertBefore(new AdduInst((*node)[0], AT, temp));
		} else {
			Register *temp = new Register();
			node->insertBefore(new LiInst(AT, new Register(mult - (1ll << 32))));
			node->insertBefore(new MultInst((*node)[1], AT));
			node->insertBefore(new MfhiInst(AT));
			node->insertBefore(new AdduInst(temp, AT, (*node)[1]));
			if (len > 1) {
				Register *newTemp = new Register();
				node->insertBefore(new SraInst(newTemp, temp, new Register(len - 1)));
				temp = newTemp;
			}
			node->insertBefore(new SrlInst(AT, temp, new Register(31)));
			node->insertBefore(new AdduInst((*node)[0], AT, temp));
		}
		if (immediate < 0) {
			node->insertBefore(new SubuInst((*node)[0], ZERO, (*node)[0]));
		}
		node->remove();
		module->changed = true;
		return;
	}

	void visitRemInst(RemInst *node) {
		Register *reg = (*node)[2];
		if (reg->type != RImmediate) {
			return;
		}
		Register *temp = new Register();
		node->insertBefore(new DivInst(temp, (*node)[1], reg));
		node->insertBefore(new MulInst(temp, temp, reg));
		node->insertBefore(new SubuInst((*node)[0], (*node)[1], temp));
		node->remove();
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