#ifndef __CPL_IR_PASS_H__
#define __CPL_IR_PASS_H__

#include "ir.h"


namespace IR {

class Value;
class Use;
class NumberLiteral;
class StringLiteral;
class BasicBlock;
class Function;
class User;
class GlobalVar;
class Inst;

class AddInst;
class SubInst;
class MulInst;
class SdivInst;
class SremInst;
class IcmpInst;
class CallInst;
class AllocaInst;
class LoadInst;
class StoreInst;
class GetPtrInst;
class PhiInst;
class ZextInst;
class TruncInst;
class BrInst;
class RetInst;

class Module;


namespace Passes {

using namespace std;
using namespace IR;


class Pass {
public:
	virtual void visitValue(Value *node) {};
	virtual void visitUse(Use *node) {};
	virtual void visitNumberLiteral(NumberLiteral *node) {};
	virtual void visitStringLiteral(StringLiteral *node) {};
	virtual void visitBasicBlock(BasicBlock *node) {};
	virtual void visitFunction(Function *node) {};
	virtual void visitUser(User *node) {};
	virtual void visitGlobalVar(GlobalVar *node) {};
	virtual void visitInst(Inst *node) {};
	virtual void visitAddInst(AddInst *node) {};
	virtual void visitSubInst(SubInst *node) {};
	virtual void visitMulInst(MulInst *node) {};
	virtual void visitSdivInst(SdivInst *node) {};
	virtual void visitSremInst(SremInst *node) {};
	virtual void visitIcmpInst(IcmpInst *node) {};
	virtual void visitCallInst(CallInst *node) {};
	virtual void visitAllocaInst(AllocaInst *node) {};
	virtual void visitLoadInst(LoadInst *node) {};
	virtual void visitStoreInst(StoreInst *node) {};
	virtual void visitGetPtrInst(GetPtrInst *node) {};
	virtual void visitPhiInst(PhiInst *node) {};
	virtual void visitZextInst(ZextInst *node) {};
	virtual void visitTruncInst(TruncInst *node) {};
	virtual void visitBrInst(BrInst *node) {};
	virtual void visitRetInst(RetInst *node) {};
	virtual void visitModule(Module *node) {};
};

}

}

#endif