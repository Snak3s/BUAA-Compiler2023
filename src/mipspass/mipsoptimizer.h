#ifndef __CPL_MIPS_OPTIMIZER_H__
#define __CPL_MIPS_OPTIMIZER_H__

#include <vector>

#include "../config.h"
#include "../mips.h"
#include "phielimination.h"
#include "muldiv.h"
#include "lvn.h"
#include "peephole.h"
#include "dce.h"
#include "removefp.h"
#include "replacedivrem.h"
#include "blockrearrange.h"
#include "linearallocator.h"
#include "gcallocator.h"


namespace MIPS {

namespace Passes {

using namespace std;
using namespace MIPS;
using namespace MIPS::Allocator;


class MIPSOptimizer : public Pass {
public:
	PhiElimination phiElimination;
	MulDiv mulDiv;
	LVN lvn;
	Peephole peephole;
	DCE dce;
	RemoveFp removeFp;
	ReplaceDivRem replaceDivRem;
	BlockRearrange blockRearrange;
	GCAllocator gcAlloc;

	void visitMModule(MModule *node) {
		if (CPL_Opt_EnableSSA) {
			node->accept(phiElimination);
		}
		if (CPL_Opt_MIPSOptimizer) {
			do {
				node->changed = false;
				node->accept(lvn);
				node->accept(peephole);
				node->accept(dce);
				node->accept(mulDiv);
			} while (node->changed);
		}
		node->accept(gcAlloc);
		if (CPL_Opt_MIPSOptimizer) {
			node->accept(removeFp);
			node->accept(replaceDivRem);
			node->accept(blockRearrange);
			do {
				node->changed = false;
				node->accept(peephole);
			} while (node->changed);
		}
	}
};

}

}

#endif