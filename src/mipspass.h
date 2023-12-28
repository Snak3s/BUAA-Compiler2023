#ifndef __CPL_MIPS_PASS_H__
#define __CPL_MIPS_PASS_H__

#include "mips.h"


namespace MIPS {
	
class MBasicBlock;

class MGlobalData;
class MGlobalWord;
class MGlobalAscii;

class MAddress;
class MStack;

class MFunction;
class MInst;

class AddInst;
class AdduInst;
class AddiuInst;
class SubInst;
class SubuInst;
class MulInst;
class MultInst;
class DivInst;
class ExDivInst;
class RemInst;
class SllInst;
class SrlInst;
class SraInst;
class MfhiInst;
class MfloInst;
class SeqInst;
class SneInst;
class SgtInst;
class SgeInst;
class SltInst;
class SltiInst;
class SleInst;
class XoriInst;
class JalInst;
class JrInst;
class JInst;
class LaInst;
class LiInst;
class LwInst;
class SwInst;
class BeqInst;
class BneInst;
class BgezInst;
class BgtzInst;
class BlezInst;
class BltzInst;
class SysCallInst;

class PhiInst;
class PCopyInst;

class MModule;


namespace Passes {

using namespace std;
using namespace MIPS;


class Pass {
public:
	virtual void visitMBasicBlock(MBasicBlock *node) {};
	virtual void visitMGlobalData(MGlobalData *node) {};
	virtual void visitMGlobalWord(MGlobalWord *node) {};
	virtual void visitMGlobalAscii(MGlobalAscii *node) {};
	virtual void visitMAddress(MAddress *node) {};
	virtual void visitMStack(MStack *node) {};
	virtual void visitMFunction(MFunction *node) {};
	virtual void visitMInst(MInst *node) {};
	virtual void visitAddInst(AddInst *node) {};
	virtual void visitAdduInst(AdduInst *node) {};
	virtual void visitAddiuInst(AddiuInst *node) {};
	virtual void visitSubInst(SubInst *node) {};
	virtual void visitSubuInst(SubuInst *node) {};
	virtual void visitMulInst(MulInst *node) {};
	virtual void visitMultInst(MultInst *node) {};
	virtual void visitDivInst(DivInst *node) {};
	virtual void visitExDivInst(ExDivInst *node) {};
	virtual void visitRemInst(RemInst *node) {};
	virtual void visitSllInst(SllInst *node) {};
	virtual void visitSrlInst(SrlInst *node) {};
	virtual void visitSraInst(SraInst *node) {};
	virtual void visitMfhiInst(MfhiInst *node) {};
	virtual void visitMfloInst(MfloInst *node) {};
	virtual void visitSeqInst(SeqInst *node) {};
	virtual void visitSneInst(SneInst *node) {};
	virtual void visitSgtInst(SgtInst *node) {};
	virtual void visitSgeInst(SgeInst *node) {};
	virtual void visitSltInst(SltInst *node) {};
	virtual void visitSltiInst(SltiInst *node) {};
	virtual void visitSleInst(SleInst *node) {};
	virtual void visitXoriInst(XoriInst *node) {};
	virtual void visitJalInst(JalInst *node) {};
	virtual void visitJrInst(JrInst *node) {};
	virtual void visitJInst(JInst *node) {};
	virtual void visitLaInst(LaInst *node) {};
	virtual void visitLiInst(LiInst *node) {};
	virtual void visitLwInst(LwInst *node) {};
	virtual void visitSwInst(SwInst *node) {};
	virtual void visitBeqInst(BeqInst *node) {};
	virtual void visitBneInst(BneInst *node) {};
	virtual void visitBgezInst(BgezInst *node) {};
	virtual void visitBgtzInst(BgtzInst *node) {};
	virtual void visitBlezInst(BlezInst *node) {};
	virtual void visitBltzInst(BltzInst *node) {};
	virtual void visitSysCallInst(SysCallInst *node) {};
	virtual void visitPhiInst(PhiInst *node) {};
	virtual void visitPCopyInst(PCopyInst *node) {};
	virtual void visitMModule(MModule *node) {};
};

}

}

#endif