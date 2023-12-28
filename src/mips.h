#ifndef __CPL_MIPS_H__
#define __CPL_MIPS_H__

#include <vector>
#include <string>

#include "mipspass.h"
#include "linkedlist.h"
#include "registers.h"
#include "ir.h"
#include "scope.h"


namespace MIPS {

using namespace std;
using namespace SymTypes;
using namespace MIPS::Passes;
using namespace List;
using namespace Reg;


const Type TAddInst     = Type("add");
const Type TAdduInst    = Type("addu");
const Type TAddiuInst   = Type("addiu");
const Type TSubInst     = Type("sub");
const Type TSubuInst    = Type("subu");
const Type TMulInst     = Type("mul");
const Type TMultInst    = Type("mult");
const Type TDivInst     = Type("div");
const Type TExDivInst   = Type("div");
const Type TRemInst     = Type("rem");
const Type TSllInst     = Type("sll");
const Type TSrlInst     = Type("srl");
const Type TSraInst     = Type("sra");
const Type TMfhiInst    = Type("mfhi");
const Type TMfloInst    = Type("mflo");
const Type TSeqInst     = Type("seq");
const Type TSneInst     = Type("sne");
const Type TSgtInst     = Type("sgt");
const Type TSgeInst     = Type("sge");
const Type TSltInst     = Type("slt");
const Type TSltiInst    = Type("slti");
const Type TSleInst     = Type("sle");
const Type TXoriInst    = Type("xori");
const Type TJalInst     = Type("jal");
const Type TJrInst      = Type("jr");
const Type TJInst       = Type("j");
const Type TLaInst      = Type("la");
const Type TLiInst      = Type("li");
const Type TLwInst      = Type("lw");
const Type TSwInst      = Type("sw");
const Type TBeqInst     = Type("beq");
const Type TBneInst     = Type("bne");
const Type TBgezInst    = Type("bgez");
const Type TBgtzInst    = Type("bgtz");
const Type TBlezInst    = Type("blez");
const Type TBltzInst    = Type("bltz");
const Type TSysCallInst = Type("syscall");

const Type TPhiInst     = Type("phi");
const Type TPCopyInst   = Type("pc");


class MInst;
class MStack;


static int valueId = 0;

class MBasicType {
public:
	int id;
	string name;

	MBasicType() { id = valueId++; }

	virtual void accept(Pass &visitor) = 0;

	friend ostream &operator << (ostream &out, const MBasicType &mips) {
		mips.print(out);
		return out;
	}
	friend ostream &operator << (ostream &out, const MBasicType *mips) {
		mips->print(out);
		return out;
	}

	virtual void print(ostream &out) const = 0;
};


class MGlobalData : public MBasicType {
public:
	Register *label = nullptr;

	void accept(Pass &visitor) {
		visitor.visitMGlobalData(this);
	}
};


class MGlobalWord : public MGlobalData {
public:
	IR::GlobalVar *globalVar = nullptr;
	Scp::Variable *var = nullptr;

	MGlobalWord(Register *label, IR::GlobalVar *globalVar) {
		this->label = label;
		this->globalVar = globalVar;
		var = globalVar->var;
	}

	void accept(Pass &visitor) {
		visitor.visitMGlobalWord(this);
	}

	void printInitVal(ostream &out, Scp::Variable *var, int &cnt) const {
		if (var->symType.isArray) {
			for (int i = 0; i < var->symType[0]; i++) {
				printInitVal(out, (*var)[i], cnt);
			}
		} else {
			if (cnt > 0) {
				out << ", ";
			}
			out << var->get();
			cnt++;
		}
	}

	void print(ostream &out) const {
		if (var->isZeroInit || !var->init) {
			out << label << ": .space " << globalVar->type.getSize() << endl;
			return;
		}
		out << label << ": .word ";
		int cnt = 0;
		printInitVal(out, var, cnt);
		out << endl;
	}
};


class MGlobalAscii : public MGlobalData {
public:
	IR::GlobalVar *globalVar = nullptr;
	IR::StringLiteral *var = nullptr;

	MGlobalAscii(Register *label, IR::GlobalVar *globalVar) {
		this->label = label;
		this->globalVar = globalVar;
		var = (IR::StringLiteral *)globalVar->values[0]->value;
	}

	void accept(Pass &visitor) {
		visitor.visitMGlobalAscii(this);
	}

	void print(ostream &out) const {
		out << label << ": .ascii ";
		out << "\"";
		for (int i = 0; i < globalVar->type[0]; i++) {
			char c = var->strVal[i];
			if (c == '\\') {
				out << "\\\\";
			} else if (c == '\n') {
				out << "\\n";
			} else if (c == '\0') {
				out << "\\0";
			} else {
				out << c;
			}
		}
		out << "\"" << endl;
	}
};


class MAddress : public MBasicType {
public:
	Register *reg = nullptr;
	int offset = 0;

	MAddress(Register *reg) {
		this->reg = reg;
	}
	MAddress(Register *reg, int offset) {
		this->reg = reg;
		this->offset = offset;
	}

	void accept(Pass &visitor) {
		visitor.visitMAddress(this);
	}

	void print(ostream &out) const {}
};


class MStack : public MBasicType {
public:
	int stackSize = 0;
	Register *stackPushSize = new Register(0);
	Register *stackPopSize = new Register(0);

	void allocSize(int size) {
		stackSize += size;
		stackPushSize->immediate -= size;
		stackPopSize->immediate += size;
	}

	MAddress *alloc(int size) {
		MAddress *addr = new MAddress(FP, stackSize);
		allocSize(size);
		return addr;
	}

	void accept(Pass &visitor) {
		visitor.visitMStack(this);
	}

	void print(ostream &out) const {}
};


class MBasicBlock : public MBasicType, public LinkedListItem {
public:
	MFunction *func = nullptr;
	LinkedList <MInst> insts;
	Register *label = nullptr;
	int loopDepth = 0;

	MBasicBlock() {}

	void prepend(MInst *inst) {
		insts.prepend(inst);
	}

	void append(MInst *inst) {
		insts.append(inst);
	}

	void remove(MInst *inst) {
		insts.erase(inst);
	}

	void insertBefore(MInst *target, MInst *inst) {
		insts.insertBefore(target, inst);
	}

	void insertAfter(MInst *target, MInst *inst) {
		insts.insertAfter(target, inst);
	}

	void accept(Pass &visitor) {
		visitor.visitMBasicBlock(this);
	}

	void print(ostream &out) const {
		out << endl;
		out << label << ":" << endl;
		for (MInst *inst : insts) {
			out << (MBasicType *)inst << endl;
		}
	}
};


class MFunction : public MBasicType, public LinkedListItem {
public:
	LinkedList <MBasicBlock> blocks;
	MStack *stack = new MStack();
	MAddress *retAddr = nullptr;

	MFunction() {}

	MBasicBlock *allocBasicBlock() {
		MBasicBlock *block = new MBasicBlock();
		block->func = this;
		blocks.append(block);
		return block;
	}

	void remove(MBasicBlock *block) {
		blocks.erase(block);
	}

	void accept(Pass &visitor) {
		visitor.visitMFunction(this);
	}

	void print(ostream &out) const {
		for (MBasicBlock *block : blocks) {
			out << block;
		}
	}
};


class MInst : public MBasicType, public LinkedListItem {
public:
	vector <Register *> operands;
	MBasicBlock *block;
	bool terminate = false;
	bool noDef = false;

	virtual Type instType() const = 0;

	void appendOperand(Register *operand) {
		operands.emplace_back(operand);
	}

	void setOperand(int index, Register *operand) {
		operands[index] = operand;
	}

	void remove() {
		block->remove(this);
	}

	void replaceWith(MInst *inst) {
		inst->block = block;
		block->insertAfter(this, inst);
		block->remove(this);
	}

	void insertBefore(MInst *inst) {
		inst->block = block;
		block->insertBefore(this, inst);
	}

	void insertAfter(MInst *inst) {
		inst->block = block;
		block->insertAfter(this, inst);
	}

	void accept(Pass &visitor) {
		visitor.visitMInst(this);
	}

	Register *operator [] (int index) {
		return operands[index];
	}

	void print(ostream &out) const {
		out << instType();
		int index = 0;
		for (Register *operand : operands) {
			if (index > 0) {
				out << ",";
			}
			out << " " << operand;
			index++;
		}
	}
};


class AddInst : public MInst {
public:
	AddInst(Register *dest, Register *op1, Register *op2) {
		appendOperand(dest);
		appendOperand(op1);
		appendOperand(op2);
	}

	Type instType() const { return TAddInst; }

	void accept(Pass &visitor) {
		visitor.visitAddInst(this);
	}
};


class AdduInst : public MInst {
public:
	AdduInst(Register *dest, Register *op1, Register *op2) {
		appendOperand(dest);
		appendOperand(op1);
		appendOperand(op2);
	}

	Type instType() const { return TAdduInst; }

	void accept(Pass &visitor) {
		visitor.visitAdduInst(this);
	}
};


class AddiuInst : public MInst {
public:
	AddiuInst(Register *dest, Register *op1, Register *op2) {
		appendOperand(dest);
		appendOperand(op1);
		appendOperand(op2);
	}

	Type instType() const { return TAddiuInst; }

	void accept(Pass &visitor) {
		visitor.visitAddiuInst(this);
	}
};


class SubInst : public MInst {
public:
	SubInst(Register *dest, Register *op1, Register *op2) {
		appendOperand(dest);
		appendOperand(op1);
		appendOperand(op2);
	}

	Type instType() const { return TSubInst; }

	void accept(Pass &visitor) {
		visitor.visitSubInst(this);
	}
};


class SubuInst : public MInst {
public:
	SubuInst(Register *dest, Register *op1, Register *op2) {
		appendOperand(dest);
		appendOperand(op1);
		appendOperand(op2);
	}

	Type instType() const { return TSubuInst; }

	void accept(Pass &visitor) {
		visitor.visitSubuInst(this);
	}
};


class MulInst : public MInst {
public:
	MulInst(Register *dest, Register *op1, Register *op2) {
		appendOperand(dest);
		appendOperand(op1);
		appendOperand(op2);
	}

	Type instType() const { return TMulInst; }

	void accept(Pass &visitor) {
		visitor.visitMulInst(this);
	}
};


class MultInst : public MInst {
public:
	MultInst(Register *op1, Register *op2) {
		appendOperand(op1);
		appendOperand(op2);
		noDef = true;
	}

	Type instType() const { return TMultInst; }

	void accept(Pass &visitor) {
		visitor.visitMultInst(this);
	}
};


class DivInst : public MInst {
public:
	DivInst(Register *dest, Register *op1, Register *op2) {
		appendOperand(dest);
		appendOperand(op1);
		appendOperand(op2);
	}

	Type instType() const { return TDivInst; }

	void accept(Pass &visitor) {
		visitor.visitDivInst(this);
	}
};


class ExDivInst : public MInst {
public:
	ExDivInst(Register *op1, Register *op2) {
		appendOperand(op1);
		appendOperand(op2);
	}

	Type instType() const { return TExDivInst; }

	void accept(Pass &visitor) {
		visitor.visitExDivInst(this);
	}
};


class RemInst : public MInst {
public:
	RemInst(Register *dest, Register *op1, Register *op2) {
		appendOperand(dest);
		appendOperand(op1);
		appendOperand(op2);
	}

	Type instType() const { return TRemInst; }

	void accept(Pass &visitor) {
		visitor.visitRemInst(this);
	}
};


class SllInst : public MInst {
public:
	SllInst(Register *dest, Register *op1, Register *op2) {
		appendOperand(dest);
		appendOperand(op1);
		appendOperand(op2);
	}

	Type instType() const { return TSllInst; }

	void accept(Pass &visitor) {
		visitor.visitSllInst(this);
	}
};


class SrlInst : public MInst {
public:
	SrlInst(Register *dest, Register *op1, Register *op2) {
		appendOperand(dest);
		appendOperand(op1);
		appendOperand(op2);
	}

	Type instType() const { return TSrlInst; }

	void accept(Pass &visitor) {
		visitor.visitSrlInst(this);
	}
};


class SraInst : public MInst {
public:
	SraInst(Register *dest, Register *op1, Register *op2) {
		appendOperand(dest);
		appendOperand(op1);
		appendOperand(op2);
	}

	Type instType() const { return TSraInst; }

	void accept(Pass &visitor) {
		visitor.visitSraInst(this);
	}
};


class MfhiInst : public MInst {
public:
	MfhiInst(Register *dest) {
		appendOperand(dest);
	}

	Type instType() const { return TMfhiInst; }

	void accept(Pass &visitor) {
		visitor.visitMfhiInst(this);
	}
};


class MfloInst : public MInst {
public:
	MfloInst(Register *dest) {
		appendOperand(dest);
	}

	Type instType() const { return TMfloInst; }

	void accept(Pass &visitor) {
		visitor.visitMfloInst(this);
	}
};


class SeqInst : public MInst {
public:
	SeqInst(Register *dest, Register *op1, Register *op2) {
		appendOperand(dest);
		appendOperand(op1);
		appendOperand(op2);
	}

	Type instType() const { return TSeqInst; }

	void accept(Pass &visitor) {
		visitor.visitSeqInst(this);
	}
};


class SneInst : public MInst {
public:
	SneInst(Register *dest, Register *op1, Register *op2) {
		appendOperand(dest);
		appendOperand(op1);
		appendOperand(op2);
	}

	Type instType() const { return TSneInst; }

	void accept(Pass &visitor) {
		visitor.visitSneInst(this);
	}
};


class SgtInst : public MInst {
public:
	SgtInst(Register *dest, Register *op1, Register *op2) {
		appendOperand(dest);
		appendOperand(op1);
		appendOperand(op2);
	}

	Type instType() const { return TSgtInst; }

	void accept(Pass &visitor) {
		visitor.visitSgtInst(this);
	}
};


class SgeInst : public MInst {
public:
	SgeInst(Register *dest, Register *op1, Register *op2) {
		appendOperand(dest);
		appendOperand(op1);
		appendOperand(op2);
	}

	Type instType() const { return TSgeInst; }

	void accept(Pass &visitor) {
		visitor.visitSgeInst(this);
	}
};


class SltInst : public MInst {
public:
	SltInst(Register *dest, Register *op1, Register *op2) {
		appendOperand(dest);
		appendOperand(op1);
		appendOperand(op2);
	}

	Type instType() const { return TSltInst; }

	void accept(Pass &visitor) {
		visitor.visitSltInst(this);
	}
};


class SltiInst : public MInst {
public:
	SltiInst(Register *dest, Register *op1, Register *op2) {
		appendOperand(dest);
		appendOperand(op1);
		appendOperand(op2);
	}

	Type instType() const { return TSltiInst; }

	void accept(Pass &visitor) {
		visitor.visitSltiInst(this);
	}
};


class SleInst : public MInst {
public:
	SleInst(Register *dest, Register *op1, Register *op2) {
		appendOperand(dest);
		appendOperand(op1);
		appendOperand(op2);
	}

	Type instType() const { return TSleInst; }

	void accept(Pass &visitor) {
		visitor.visitSleInst(this);
	}
};


class XoriInst : public MInst {
public:
	XoriInst(Register *dest, Register *op1, Register *op2) {
		appendOperand(dest);
		appendOperand(op1);
		appendOperand(op2);
	}

	Type instType() const { return TXoriInst; }

	void accept(Pass &visitor) {
		visitor.visitXoriInst(this);
	}
};


class JalInst : public MInst {
public:
	JalInst(Register *tar) {
		appendOperand(tar);
		noDef = true;
	}

	Type instType() const { return TJalInst; }

	void accept(Pass &visitor) {
		visitor.visitJalInst(this);
	}
};


class JrInst : public MInst {
public:
	JrInst(Register *tar) {
		appendOperand(tar);
		terminate = true;
		noDef = true;
	}

	Type instType() const { return TJrInst; }

	void accept(Pass &visitor) {
		visitor.visitJrInst(this);
	}
};


class JInst : public MInst {
public:
	JInst(Register *tar) {
		appendOperand(tar);
		terminate = true;
		noDef = true;
	}

	Type instType() const { return TJInst; }

	void accept(Pass &visitor) {
		visitor.visitJInst(this);
	}
};


class LaInst : public MInst {
public:
	LaInst(Register *dest, Register *offset, Register *label) {
		appendOperand(dest);
		appendOperand(offset);
		appendOperand(label);
	}
	LaInst(Register *dest, MAddress *addr) {
		appendOperand(dest);
		appendOperand(new Register(addr->offset));
		appendOperand(addr->reg);
	}

	Type instType() const { return TLaInst; }

	void accept(Pass &visitor) {
		visitor.visitLaInst(this);
	}

	void print(ostream &out) const {
		out << instType() << " ";
		out << operands[0] << ", ";
		if (operands[2]->type == RLabel) {
			out << operands[2];
			if (operands[1]->immediate != 0) {
				out << " + " << operands[1];
			}
		} else {
			out << operands[1] << "(" << operands[2] << ")";
		}
	}
};


class LiInst : public MInst {
public:
	LiInst(Register *dest, Register *op) {
		appendOperand(dest);
		appendOperand(op);
	}

	Type instType() const { return TLiInst; }

	void accept(Pass &visitor) {
		visitor.visitLiInst(this);
	}
};


class LwInst : public MInst {
public:
	LwInst(Register *dest, Register *offset, Register *label) {
		appendOperand(dest);
		appendOperand(offset);
		appendOperand(label);
	}
	LwInst(Register *dest, MAddress *addr) {
		appendOperand(dest);
		appendOperand(new Register(addr->offset));
		appendOperand(addr->reg);
	}

	Type instType() const { return TLwInst; }

	void accept(Pass &visitor) {
		visitor.visitLwInst(this);
	}

	void print(ostream &out) const {
		out << instType() << " ";
		out << operands[0] << ", ";
		if (operands[2]->type == RLabel) {
			out << operands[2];
			if (operands[1]->immediate != 0) {
				out << " + " << operands[1];
			}
		} else {
			out << operands[1] << "(" << operands[2] << ")";
		}
	}
};


class SwInst : public MInst {
public:
	SwInst(Register *op, Register *offset, Register *label) {
		appendOperand(op);
		appendOperand(offset);
		appendOperand(label);
		noDef = true;
	}
	SwInst(Register *dest, MAddress *addr) {
		appendOperand(dest);
		appendOperand(new Register(addr->offset));
		appendOperand(addr->reg);
		noDef = true;
	}

	Type instType() const { return TSwInst; }

	void accept(Pass &visitor) {
		visitor.visitSwInst(this);
	}

	void print(ostream &out) const {
		out << instType() << " ";
		out << operands[0] << ", ";
		if (operands[2]->type == RLabel) {
			out << operands[2];
			if (operands[1]->immediate != 0) {
				out << " + " << operands[1];
			}
		} else {
			out << operands[1] << "(" << operands[2] << ")";
		}
	}
};


class BeqInst : public MInst {
public:
	BeqInst(Register *op1, Register *op2, Register *tar) {
		appendOperand(op1);
		appendOperand(op2);
		appendOperand(tar);
		terminate = true;
		noDef = true;
	}

	Type instType() const { return TBeqInst; }

	void accept(Pass &visitor) {
		visitor.visitBeqInst(this);
	}
};


class BneInst : public MInst {
public:
	BneInst(Register *op1, Register *op2, Register *tar) {
		appendOperand(op1);
		appendOperand(op2);
		appendOperand(tar);
		terminate = true;
		noDef = true;
	}

	Type instType() const { return TBneInst; }

	void accept(Pass &visitor) {
		visitor.visitBneInst(this);
	}
};


class BgezInst : public MInst {
public:
	BgezInst(Register *op, Register *tar) {
		appendOperand(op);
		appendOperand(tar);
		terminate = true;
		noDef = true;
	}

	Type instType() const { return TBgezInst; }

	void accept(Pass &visitor) {
		visitor.visitBgezInst(this);
	}
};


class BgtzInst : public MInst {
public:
	BgtzInst(Register *op, Register *tar) {
		appendOperand(op);
		appendOperand(tar);
		terminate = true;
		noDef = true;
	}

	Type instType() const { return TBgtzInst; }

	void accept(Pass &visitor) {
		visitor.visitBgtzInst(this);
	}
};


class BlezInst : public MInst {
public:
	BlezInst(Register *op, Register *tar) {
		appendOperand(op);
		appendOperand(tar);
		terminate = true;
		noDef = true;
	}

	Type instType() const { return TBlezInst; }

	void accept(Pass &visitor) {
		visitor.visitBlezInst(this);
	}
};


class BltzInst : public MInst {
public:
	BltzInst(Register *op, Register *tar) {
		appendOperand(op);
		appendOperand(tar);
		terminate = true;
		noDef = true;
	}

	Type instType() const { return TBltzInst; }

	void accept(Pass &visitor) {
		visitor.visitBltzInst(this);
	}
};


class SysCallInst : public MInst {
public:
	SysCallInst() {
		noDef = true;
	}

	Type instType() const { return TSysCallInst; }

	void accept(Pass &visitor) {
		visitor.visitSysCallInst(this);
	}
};


// abstract instructions

class PhiInst : public MInst {
public:
	PhiInst(Register *dest) {
		appendOperand(dest);
	}

	Type instType() const { return TPhiInst; }

	void accept(Pass &visitor) {
		visitor.visitPhiInst(this);
	}
};


class PCopyInst : public MInst {
public:
	PCopyInst(Register *dest, Register *src) {
		appendOperand(dest);
		appendOperand(src);
	}

	Type instType() const { return TPCopyInst; }

	void accept(Pass &visitor) {
		visitor.visitPCopyInst(this);
	}
};


class MModule {
public:
	vector <MGlobalData *> datas;
	LinkedList <MFunction> funcs;

	bool changed = false;

	MModule() {}

	void appendFunc(MFunction *func) {
		funcs.append(func);
	}

	void appendGlobalData(MGlobalData *data) {
		datas.emplace_back(data);
	}

	void accept(Pass &visitor) {
		visitor.visitMModule(this);
	}

	friend ostream& operator << (ostream &out, const MModule &module) {
		module.print(out);
		return out;
	}
	friend ostream& operator << (ostream &out, const MModule *module) {
		module->print(out);
		return out;
	}

	void print(ostream &out) const {
		out << ".data" << endl;
		for (MGlobalData *data : datas) {
			out << data;
		}
		out << endl;
		out << ".text" << endl;
		out << "libmain:" << endl;
		if (funcs.last()->stack->stackPushSize->immediate != 0) {
			out << "add $sp, $sp, " << funcs.last()->stack->stackPushSize << endl;
		}
		MFunction *main = funcs.last();
		out << main;
		for (MFunction *func : funcs) {
			if (func != main) {
				out << func;
			}
		}
	}
};

}

#endif