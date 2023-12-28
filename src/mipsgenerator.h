#ifndef __CPL_MIPS_GENERATOR_H__
#define __CPL_MIPS_GENERATOR_H__

#include <map>

#include "config.h"
#include "ir.h"
#include "irpass.h"
#include "mips.h"
#include "symtypes.h"
#include "irpass/loopanalyzer.h"


namespace IR {

namespace Passes {

using namespace std;
using namespace IR;
using namespace MIPS;
using namespace SymTypes;


class MIPSGenerator : public Pass {
public:
	Module *irModule;
	MModule *module;
	Function *irFunc = nullptr;
	Function *irMain = nullptr;
	MFunction *curFunc = nullptr;
	MBasicBlock *curBlock = nullptr;

	LoopAnalyzer loopAnalyzer;

	// IR::Value -> register
	map <int, Register *> valueMapping;
	map <int, Register *> constValueMapping;
	// IR::Value -> address stored
	map <int, MAddress *> valueAddress;
	// pointer typed IR::Value -> address pointed
	map <int, MAddress *> pointerMapping;
	map <int, MFunction *> funcs;

	bool hasCallInst = false;

	MIPSGenerator(MModule *module = new MModule()) {
		this->module = module;
	}


	void appendInst(MInst *inst) {
		curBlock->append(inst);
		inst->block = curBlock;
	}


	void setAddress(Value *value, MAddress *addr) {
		valueAddress[value->id] = addr;
	}

	MAddress *getAddress(Value *value) {
		if (valueAddress.count(value->id)) {
			return valueAddress[value->id];
		}
		return nullptr;
	}


	void setReg(Value *value, Register *reg) {
		valueMapping[value->id] = reg;
	}

	Register *getReg(Value *value) {
		if (valueMapping.count(value->id)) {
			return valueMapping[value->id];
		}
		Register *reg = nullptr;
		if (value->isConst()) {
			if (value->getConstValue() != 0) {
				reg = new Register();
				appendInst(new LiInst(reg, getValReg(value)));
			} else {
				reg = ZERO;
			}
		} else if (valueAddress.count(value->id)) {
			reg = new Register();
			appendInst(new LwInst(reg, getAddress(value)));
			return reg;
		} else if (pointerMapping.count(value->id)) {
			reg = new Register();
			appendInst(new LaInst(reg, pointerMapping[value->id]));
			return reg;
		} else {
			reg = new Register(value);
		}
		valueMapping[value->id] = reg;
		return reg;
	}

	Register *getValReg(Value *value) {
		if (constValueMapping.count(value->id)) {
			return constValueMapping[value->id];
		}
		if (value->isConst()) {
			Register *reg = nullptr;
			reg = new Register(value->getConstValue());
			constValueMapping[value->id] = reg;
			return reg;
		}
		return getReg(value);
	}


	void setPointer(Value *value, MAddress *addr) {
		pointerMapping[value->id] = addr;
		if (addr->offset == 0 && addr->reg->type == RVirtual) {
			valueMapping[value->id] = addr->reg;
		}
	}

	MAddress *getPointer(Value *value) {
		if (pointerMapping.count(value->id)) {
			return pointerMapping[value->id];
		}
		MAddress *addr = new MAddress(getReg(value));
		return addr;
	}


	bool is16Bits(int value) {
		return -32768 <= value && value < 32768;
	}

	void move(Register *dest, Register *src) {
		if (src->type == RImmediate && !is16Bits(src->immediate)) {
			appendInst(new LiInst(dest, src));
			return;
		}
		appendInst(new MIPS::AddInst(dest, ZERO, src));
	}


	void saveFuncArgs() {
		int cnt = 0;
		for (Value *param : irFunc->params) {
			if (cnt < 4) {
				if (valueMapping.count(param->id)) {
					Register *reg = valueMapping[param->id];
					if (reg == A0 || reg == A1 || reg == A2 || reg == A3) {
						Register *copy = new Register();
						move(copy, reg);
						setReg(param, copy);
					}
				}
			} else {
				Register *copy = new Register();
				appendInst(new LwInst(copy, getAddress(param)));
				setReg(param, copy);
			}
			cnt++;
		}
	}


	void visitBasicBlock(BasicBlock *node) {
		curBlock = curFunc->allocBasicBlock();
		curBlock->label = getReg(node);
		curBlock->loopDepth = loopAnalyzer.loopDepth[node];
		for (Inst *inst : node->insts) {
			inst->accept(*this);
		}
	}

	void visitFunction(Function *node) {
		irFunc = node;
		curFunc = new MFunction();
		curFunc->name = node->name;
		module->appendFunc(curFunc);
		funcs[node->id] = curFunc;
		hasCallInst = false;
		for (Function *callee : node->asCaller) {
			if (!callee->reserved) {
				hasCallInst = true;
				break;
			}
		}

		int cnt = 0;
		for (Value *param : node->params) {
			setAddress(param, curFunc->stack->alloc(4));
			if (cnt < 4) {
				static Register *paramReg[4] = {A0, A1, A2, A3};
				setReg(param, paramReg[cnt]);
			}
			cnt++;
		}

		Register *entry = new Register(node->name.substr(1) + "_entry");
		curBlock = curFunc->allocBasicBlock();
		curBlock->label = entry;
		setReg(node, entry);
		move(FP, SP);
		if (hasCallInst && irFunc != irMain) {
			curFunc->retAddr = curFunc->stack->alloc(4);
			appendInst(new SwInst(RA, curFunc->retAddr));
		}
		saveFuncArgs();

		for (BasicBlock *block : node->blocks) {
			Register *label = new Register(node->name.substr(1) + "_block_" + to_string(block->id));
			setReg(block, label);
			setReg(block->label, label);
		}

		appendInst(new JInst(getReg(node->blocks.first())));

		for (BasicBlock *block : node->blocks) {
			block->accept(*this);
		}
	}

	void visitGlobalVar(GlobalVar *node) {
		string label = node->name.substr(1) + "_global";
		bool topLevel = true;
		for (int i = 0; i < label.length(); i++) {
			if (label[i] == '.') {
				label[i] = '_';
				topLevel = false;
			}
		}
		if (topLevel) {
			label = "_toplevel_" + label;
		}

		Register *reg = new Register(label);
		if (node->type.baseType() == Int8) {
			MGlobalAscii *ascii = new MGlobalAscii(reg, node);
			ascii->name = node->name;
			module->appendGlobalData(ascii);
			setReg(node, reg);
			setReg(node->reg, reg);
			return;
		}
		if (node->type.baseType() == Int32) {
			MGlobalWord *word = new MGlobalWord(reg, node);
			word->name = node->name;
			module->appendGlobalData(word);
			setReg(node, reg);
			setReg(node->reg, reg);
			return;
		}
	}

	void visitAddInst(AddInst *node) {
		Value *dest = (*node)[0];
		Value *op1 = (*node)[1];
		Value *op2 = (*node)[2];
		appendInst(new MIPS::AdduInst(getReg(dest), getReg(op1), getValReg(op2)));
	}

	void visitSubInst(SubInst *node) {
		Value *dest = (*node)[0];
		Value *op1 = (*node)[1];
		Value *op2 = (*node)[2];
		appendInst(new MIPS::SubuInst(getReg(dest), getReg(op1), getValReg(op2)));
	}

	void visitMulInst(MulInst *node) {
		Value *dest = (*node)[0];
		Value *op1 = (*node)[1];
		Value *op2 = (*node)[2];
		appendInst(new MIPS::MulInst(getReg(dest), getReg(op1), getValReg(op2)));
	}

	void visitSdivInst(SdivInst *node) {
		Value *dest = (*node)[0];
		Value *op1 = (*node)[1];
		Value *op2 = (*node)[2];
		appendInst(new DivInst(getReg(dest), getReg(op1), getValReg(op2)));
	}

	void visitSremInst(SremInst *node) {
		Value *dest = (*node)[0];
		Value *op1 = (*node)[1];
		Value *op2 = (*node)[2];
		appendInst(new RemInst(getReg(dest), getReg(op1), getValReg(op2)));
	}

	void visitIcmpInst(IcmpInst *node) {
		Value *dest = (*node)[0];
		Value *op1 = (*node)[1];
		Value *op2 = (*node)[2];
		if (node->cond == CondEq) {
			appendInst(new SeqInst(getReg(dest), getReg(op1), getValReg(op2)));
		} else if (node->cond == CondNe) {
			appendInst(new SneInst(getReg(dest), getReg(op1), getValReg(op2)));
		} else if (node->cond == CondSgt) {
			appendInst(new SgtInst(getReg(dest), getReg(op1), getValReg(op2)));
		} else if (node->cond == CondSge) {
			appendInst(new SgeInst(getReg(dest), getReg(op1), getValReg(op2)));
		} else if (node->cond == CondSlt) {
			// note that slt instruction has no extension / pseudo instructions
			// use slti instructions for 16-bit immediate
			if (op2->isConst() && is16Bits(op2->getConstValue())) {
				appendInst(new SltiInst(getReg(dest), getReg(op1), getValReg(op2)));
			} else {
				appendInst(new SltInst(getReg(dest), getReg(op1), getReg(op2)));
			}
		} else if (node->cond == CondSle) {
			appendInst(new SleInst(getReg(dest), getReg(op1), getValReg(op2)));
		}
	}

	void handleSysCallGetInt(CallInst *node) {
		move(V0, new Register(5));
		appendInst(new SysCallInst());
		move(getReg((*node)[0]), V0);
	}

	void handleSysCallPutInt(CallInst *node) {
		move(V0, new Register(1));
		move(A0, getValReg((*node)[1]));
		appendInst(new SysCallInst());
	}

	void handleSysCallPutCh(CallInst *node) {
		move(V0, new Register(11));
		move(A0, getValReg((*node)[1]));
		appendInst(new SysCallInst());
	}

	void handleSysCallPutStr(CallInst *node) {
		move(V0, new Register(4));
		move(A0, getValReg((*node)[1]));
		appendInst(new SysCallInst());
	}

	void visitCallInst(CallInst *node) {
		int offset = 2;
		if (node->noDef) {
			offset = 1;
		}
		Value *func = (*node)[offset - 1];
		if (func == irModule->getint) {
			handleSysCallGetInt(node);
			return;
		}
		if (func == irModule->putint) {
			handleSysCallPutInt(node);
			return;
		}
		if (func == irModule->putch) {
			handleSysCallPutCh(node);
			return;
		}
		if (func == irModule->putstr) {
			handleSysCallPutStr(node);
			return;
		}
		int paramCnt = node->values.size() - offset;
		vector <Register *> params;
		for (int i = 0; i < paramCnt; i++) {
			Register *param = nullptr;
			if (i < 4) {
				// the param will be moved to register directly
				param = getValReg((*node)[i + offset]);
			} else {
				// the param will be stored in stack
				param = getReg((*node)[i + offset]);
			}
			params.emplace_back(param);
		}
		appendInst(new MIPS::AddInst(SP, SP, funcs[func->id]->stack->stackPushSize));
		for (int i = 0; i < paramCnt; i++) {
			Register *param = params[i];
			if (i < 4) {
				static Register *paramReg[4] = {A0, A1, A2, A3};
				move(paramReg[i], param);
			} else {
				appendInst(new SwInst(param, new MAddress(SP, 4 * i)));
			}
		}
		appendInst(new JalInst(getReg(func)));
		appendInst(new MIPS::AddInst(SP, SP, funcs[func->id]->stack->stackPopSize));
		move(FP, SP);
		if (!node->noDef) {
			move(getReg((*node)[0]), V0);
		}
	}

	void visitAllocaInst(AllocaInst *node) {
		SymType type = node->type;
		int size = type.getSize();
		setPointer((*node)[0], curFunc->stack->alloc(size));
	}

	void visitLoadInst(LoadInst *node) {
		appendInst(new LwInst(getReg((*node)[0]), getPointer((*node)[1])));
	}

	void visitStoreInst(StoreInst *node) {
		appendInst(new SwInst(getReg((*node)[0]), getPointer((*node)[1])));
	}

	void visitGetPtrInst(GetPtrInst *node) {
		SymType type = node->type;
		MAddress *addr = getPointer((*node)[1]);
		addr = new MAddress(addr->reg, addr->offset);
		for (int i = 2; i < node->values.size(); i++) {
			Value *value = (*node)[i];
			if (value->isConst()) {
				addr->offset += value->getConstValue() * type.getSize();
			}
			type.pop();
		}
		type = node->type;
		Register *reg = nullptr;
		for (int i = 2; i < node->values.size(); i++) {
			Value *value = (*node)[i];
			if (value->isConst()) {
				type.pop();
				continue;
			}
			if (reg == nullptr) {
				reg = new Register();
				appendInst(new LaInst(reg, addr));
			}
			Register *size = new Register();
			appendInst(new MIPS::MulInst(size, getReg(value), new Register(type.getSize())));
			Register *newReg = new Register();
			appendInst(new MIPS::AdduInst(newReg, reg, size));
			reg = newReg;
			type.pop();
		}
		if (reg) {
			addr = new MAddress(reg, 0);
		}

		// load addr into reg
		if (CPL_Opt_EnableAddrToReg) {
			if (addr->reg->type == RLabel) {
				Value *def = (*node)[0];
				int instCount = 0;
				for (Use *use : def->uses) {
					Inst *inst = (Inst *)use->user;
					if (!inst->noDef && use->index == 0) {
						continue;
					}
					if (inst->instType() == TGetPtrInst) {
						continue;
					}
					instCount += 1 << (2 * min(5, loopAnalyzer.loopDepth[inst->block] - loopAnalyzer.loopDepth[node->block]));
				}
				if (instCount >= 8) {
					reg = new Register();
					appendInst(new LaInst(reg, addr));
					addr = new MAddress(reg, 0);
				}
			}
		}

		setPointer((*node)[0], addr);
	}

	void visitPhiInst(PhiInst *node) {
		MIPS::PhiInst *phi = new MIPS::PhiInst(getReg((*node)[0]));
		for (int i = 1; i < node->values.size(); i++) {
			phi->appendOperand(getValReg((*node)[i]));
		}
		appendInst(phi);
	}

	void visitZextInst(ZextInst *node) {
		setReg((*node)[0], getReg((*node)[1]));
	}

	void visitTruncInst(TruncInst *node) {
		setReg((*node)[0], getReg((*node)[1]));
	}

	void visitBrInst(BrInst *node) {
		if (!node->hasCond) {
			appendInst(new JInst(getReg((*node)[0])));
			return;
		}
		appendInst(new BeqInst(getReg((*node)[0]), ZERO, getReg((*node)[2])));
		appendInst(new JInst(getReg((*node)[1])));
	}

	void visitRetInst(RetInst *node) {
		// halt in main
		if (irFunc == irMain) {
			move(V0, new Register(10));
			appendInst(new SysCallInst());
		}

		if (hasCallInst && irFunc != irMain) {
			appendInst(new LwInst(RA, curFunc->retAddr));
		}
		if (node->type != SymType(TVoid)) {
			move(V0, getValReg((*node)[0]));
		}
		appendInst(new JrInst(RA));
	}

	void visitModule(Module *node) {
		irModule = node;
		irMain = node->funcs.last();
		for (GlobalVar *globalVar : node->globalVars) {
			globalVar->accept(*this);
		}
		for (Function *func : node->funcs) {
			func->accept(*this);
		}
	}
};

}

}

#endif