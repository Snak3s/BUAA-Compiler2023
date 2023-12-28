#ifndef __CPL_REGISTERS_H__
#define __CPL_REGISTERS_H__

#include "types.h"
#include "symtypes.h"
#include "ir.h"


namespace Reg {

using namespace std;
using namespace Types;
using namespace SymTypes;


const Type RZERO = Type("$zero");
const Type RAT   = Type("$at");
const Type RV0   = Type("$v0");
const Type RV1   = Type("$v1");
const Type RA0   = Type("$a0");
const Type RA1   = Type("$a1");
const Type RA2   = Type("$a2");
const Type RA3   = Type("$a3");
const Type RT0   = Type("$t0");
const Type RT1   = Type("$t1");
const Type RT2   = Type("$t2");
const Type RT3   = Type("$t3");
const Type RT4   = Type("$t4");
const Type RT5   = Type("$t5");
const Type RT6   = Type("$t6");
const Type RT7   = Type("$t7");
const Type RS0   = Type("$s0");
const Type RS1   = Type("$s1");
const Type RS2   = Type("$s2");
const Type RS3   = Type("$s3");
const Type RS4   = Type("$s4");
const Type RS5   = Type("$s5");
const Type RS6   = Type("$s6");
const Type RS7   = Type("$s7");
const Type RT8   = Type("$t8");
const Type RT9   = Type("$t9");
const Type RK0   = Type("$k0");
const Type RK1   = Type("$k1");
const Type RGP   = Type("$gp");
const Type RSP   = Type("$sp");
const Type RFP   = Type("$fp");
const Type RRA   = Type("$ra");

const Type RVirtual   = Type("$virtual");
const Type RImmediate = Type("$immediate");
const Type RLabel     = Type("$label");


struct Register {
	Type type = RVirtual;
	IR::Value *value;
	int immediate;
	string label;

	Register() {
		this->type = RVirtual;
		this->value = new IR::Value(Int32);
	}
	Register(const Type &type) {
		this->type = type;
	}
	Register(IR::Value *value) {
		this->type = RVirtual;
		this->value = value;
	}
	Register(int immediate) {
		this->type = RImmediate;
		this->immediate = immediate;
	}
	Register(const string &label) {
		this->type = RLabel;
		this->label = label;
	}

	bool isVirtual() { return type == RVirtual; }

	friend bool operator == (const Register &reg1, const Register &reg2) {
		if (reg1.type != reg2.type) {
			return false;
		}
		if (reg1.type == RVirtual) {
			return reg1.value == reg2.value;
		}
		if (reg1.type == RImmediate) {
			return reg1.immediate == reg2.immediate;
		}
		if (reg1.type == RLabel) {
			return reg1.label == reg2.label;
		}
		return true;
	}

	friend ostream &operator << (ostream &out, const Register &reg) {
		if (reg.type == RVirtual) {
			out << reg.type << reg.value->id;
		} else if (reg.type == RImmediate) {
			out << reg.immediate;
		} else if (reg.type == RLabel) {
			out << reg.label;
		} else {
			out << reg.type;
		}
		return out;
	}

	friend ostream &operator << (ostream &out, const Register *reg) {
		out << *reg;
		return out;
	}
};


struct RegisterPtrComp {
	bool operator () (Register *reg1, Register *reg2) const {
		if (reg1 == nullptr) {
			return reg2 != nullptr;
		}
		if (reg2 == nullptr) {
			return false;
		}
		if (reg1->type != reg2->type) {
			return reg1->type.id < reg2->type.id;
		}
		if (reg1->type == RVirtual) {
			return reg1->value->id < reg2->value->id;
		}
		if (reg1->type == RImmediate) {
			return reg1->immediate < reg2->immediate;
		}
		if (reg1->type == RLabel) {
			return reg1->label < reg2-> label;
		}
		return false;
	}
};


Register *ZERO = new Register(RZERO);
Register *AT   = new Register(RAT);
Register *V0   = new Register(RV0);
Register *V1   = new Register(RV1);
Register *A0   = new Register(RA0);
Register *A1   = new Register(RA1);
Register *A2   = new Register(RA2);
Register *A3   = new Register(RA3);
Register *T0   = new Register(RT0);
Register *T1   = new Register(RT1);
Register *T2   = new Register(RT2);
Register *T3   = new Register(RT3);
Register *T4   = new Register(RT4);
Register *T5   = new Register(RT5);
Register *T6   = new Register(RT6);
Register *T7   = new Register(RT7);
Register *S0   = new Register(RS0);
Register *S1   = new Register(RS1);
Register *S2   = new Register(RS2);
Register *S3   = new Register(RS3);
Register *S4   = new Register(RS4);
Register *S5   = new Register(RS5);
Register *S6   = new Register(RS6);
Register *S7   = new Register(RS7);
Register *T8   = new Register(RT8);
Register *T9   = new Register(RT9);
Register *K0   = new Register(RK0);
Register *K1   = new Register(RK1);
Register *GP   = new Register(RGP);
Register *SP   = new Register(RSP);
Register *FP   = new Register(RFP);
Register *RA   = new Register(RRA);

}

#endif