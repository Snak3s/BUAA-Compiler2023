#ifndef __CPL_IR_OPTIMIZER_H__
#define __CPL_IR_OPTIMIZER_H__

#include <vector>

#include "../config.h"
#include "../ir.h"
#include "reglabeller.h"
#include "cfgbuilder.h"
#include "mem2reg.h"
#include "constoptimizer.h"
#include "inlinefunc.h"
#include "gvlocalizer.h"
#include "dce.h"
#include "aggressivedce.h"
#include "funceval.h"
#include "lvn.h"
#include "gvn.h"
#include "gcm.h"
#include "loopunroll.h"
#include "array2var.h"


namespace IR {

namespace Passes {

using namespace std;
using namespace IR;


class IROptimizer : public Pass {
public:
	RegLabeller regLabeller;
	CFGBuilder cfgBuilder;
	Mem2Reg mem2reg;
	ConstOptimizer constOptimizer;
	InlineFunc inlineFunc;
	GVLocalizer gvLocalizer;
	DCE dce;
	AggressiveDCE aggressiveDce;
	FuncEval funcEval;
	LVN lvn;
	GVN gvn;
	GCM gcm;
	LoopUnroll loopUnroll;
	Array2Var array2var;

	void visitModule(Module *node) {
		do {
			node->changed = false;
			if (CPL_Opt_EnableSSA) {
				node->accept(mem2reg);
			}
			if (CPL_Opt_IROptimizer) {
				node->accept(loopUnroll);
				node->accept(constOptimizer);
				node->accept(inlineFunc);
				node->accept(dce);
				node->accept(aggressiveDce);
				node->accept(funcEval);
				node->accept(gvLocalizer);
				node->accept(array2var);
				node->accept(lvn);
				node->accept(gvn);
				node->accept(gcm);
			}
		} while (node->changed);
		node->accept(cfgBuilder);
		node->accept(regLabeller);
	}
};

}

}

#endif