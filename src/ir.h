#ifndef __CPL_IR_H__
#define __CPL_IR_H__

#include <vector>
#include <string>

#include "scope.h"
#include "symtypes.h"
#include "irpass.h"
#include "linkedlist.h"


namespace IR {

using namespace std;
using namespace SymTypes;
using namespace IR::Passes;
using namespace List;


const Type TAddInst    = Type("add");
const Type TSubInst    = Type("sub");
const Type TMulInst    = Type("mul");
const Type TSdivInst   = Type("sdiv");
const Type TSremInst   = Type("srem");
const Type TIcmpInst   = Type("icmp");
const Type TCallInst   = Type("call");
const Type TAllocaInst = Type("alloca");
const Type TLoadInst   = Type("load");
const Type TStoreInst  = Type("store");
const Type TGetPtrInst = Type("getelementptr");
const Type TPhiInst    = Type("phi");
const Type TZextInst   = Type("zext");
const Type TTruncInst  = Type("trunc");
const Type TBrInst     = Type("br");
const Type TRetInst    = Type("ret");

const Type CondEq = Type("eq");
const Type CondNe = Type("ne");
const Type CondSgt = Type("sgt");
const Type CondSge = Type("sge");
const Type CondSlt = Type("slt");
const Type CondSle = Type("sle");


class Value;
class Use;
class Function;
class User;
class Inst;


static int valueId = 0;

const int UNAVAILABLE = -1;

class Value {
public:
	int id;
	string name;
	SymType type, type2;
	LinkedList <Use> uses;
	int regId = 0;

	Value() { id = valueId++; }
	Value(const SymType &type) {
		id = valueId++;
		this->type = type;
		this->type.toIRType();
	}
	Value(const SymType &type, const string &name) {
		id = valueId++;
		this->type = type;
		this->type.toIRType();
		this->name = name;
	}

	void appendUser(Use *use) {
		uses.append(use);
	}

	void removeUser(Use *use) {
		uses.erase(use);
	}

	virtual void destroy() {}

	virtual bool isConst() { return false; }

	virtual int getConstValue() { return 0; }

	friend bool operator == (const Value &a, const Value &b) {
		return a.id == b.id;
	}
	friend bool operator != (const Value &a, const Value &b) {
		return a.id != b.id;
	}

	friend ostream &operator << (ostream &out, const Value &value) {
		value.print(out);
		return out;
	}
	friend ostream &operator << (ostream &out, const Value *value) {
		value->print(out);
		return out;
	}

	virtual void accept(Pass &visitor) {
		visitor.visitValue(this);
	}

	virtual void print(ostream &out) const {
		if (regId != UNAVAILABLE) {
			out << "%" << regId;
		} else if (name.length() > 0) {
			out << name;
		} else {
			out << "<value>";
		}
	}
};


class Use : public Value, public LinkedListItem {
public:
	User *user = nullptr;
	Value *value = nullptr;
	int index = 0;

	Use(User *user, Value *value, int index = 0) {
		this->user = user;
		this->value = value;
		this->index = index;
		value->appendUser(this);
	}

	void setValue(Value *value) {
		this->value->removeUser(this);
		this->value = value;
		value->appendUser(this);
	}

	void destroy() {
		if (!user || !value) {
			return;
		}
		value->removeUser(this);
		value = nullptr;
		user = nullptr;
	}

	void accept(Pass &visitor) {
		visitor.visitUse(this);
	}

	void print(ostream &out) const {
		out << value;
	}
};


class NumberLiteral : public Value {
public:
	int numVal;

	NumberLiteral(const int numVal) {
		type = Int32;
		this->numVal = numVal;
		regId = UNAVAILABLE;
	}

	bool isConst() { return true; }

	int getConstValue() { return numVal; }

	void accept(Pass &visitor) {
		visitor.visitNumberLiteral(this);
	}

	void print(ostream &out) const {
		out << numVal;
	}
};


class StringLiteral : public Value {
public:
	string strVal;

	StringLiteral(const string &strVal) {
		type = Int8;
		type.append(strVal.length() + 1);
		this->strVal = strVal;
		regId = UNAVAILABLE;
	}

	bool isConst() { return true; }

	void accept(Pass &visitor) {
		visitor.visitStringLiteral(this);
	}

	void print(ostream &out) const {
		out << "c\"";
		for (int i = 0; i < type[0]; i++) {
			char c = strVal[i];
			if (c == '\\') {
				out << "\\\\";
			} else if (c == '\n') {
				out << "\\0a";
			} else if (c == '\0') {
				out << "\\00";
			} else {
				out << c;
			}
		}
		out << "\"";
	}
};


class BasicBlock : public Value, public LinkedListItem {
public:
	Function *func = nullptr;
	vector <BasicBlock *> jumpFrom, jumpTo;
	LinkedList <Inst> insts;
	Value *label = new Value(TLabel);

	BasicBlock() { regId = UNAVAILABLE; }

	void prepend(Inst *inst) {
		insts.prepend(inst);
	}

	void append(Inst *inst) {
		insts.append(inst);
	}

	void remove(Inst *inst) {
		insts.erase(inst);
	}

	void insertBefore(Inst *target, Inst *inst) {
		insts.insertBefore(target, inst);
	}

	void insertAfter(Inst *target, Inst *inst) {
		insts.insertAfter(target, inst);
	}

	void destroy() {
		for (Inst *inst : insts) {
			Value *value = (Value *)inst;
			value->destroy();
			remove(inst);
		}
	}

	void accept(Pass &visitor) {
		visitor.visitBasicBlock(this);
	}

	void print(ostream &out) const {
		out << label->regId << ":" << endl;
		for (Inst *inst : insts) {
			out << "    " << (Value *)inst << endl;
		}
	}
};


class Function : public Value, public LinkedListItem {
public:
	bool reserved = false;
	vector <Function *> asCallee, asCaller;
	LinkedList <BasicBlock> blocks;
	Scp::Function *func = nullptr;
	vector <Value *> params;

	Function() { regId = UNAVAILABLE; }
	Function(const SymType &type, const string &name) {
		this->name = name;
		this->type = type;
		regId = UNAVAILABLE;
	}
	Function(Scp::Function *func) {
		this->func = func;
		name = "@" + func->ident.token;
		type = func->funcType;
		type.toIRType();
		func->irFunc = this;
		regId = UNAVAILABLE;
	}

	void appendParam(Value *param) {
		params.emplace_back(param);
	}

	BasicBlock *allocBasicBlock() {
		BasicBlock *block = new BasicBlock();
		block->func = this;
		blocks.append(block);
		return block;
	}

	BasicBlock *allocBasicBlockAfter(BasicBlock *target) {
		BasicBlock *block = new BasicBlock();
		block->func = this;
		blocks.insertAfter(target, block);
		return block;
	}

	void remove(BasicBlock *block) {
		blocks.erase(block);
	}

	void destroy() {
		for (BasicBlock *block : blocks) {
			block->destroy();
		}
	}

	void accept(Pass &visitor) {
		if (reserved) {
			return;
		}
		visitor.visitFunction(this);
	}

	void print(ostream &out) const {
		if (reserved) {
			return;
		}
		out << endl;
		out << "define dso_local " << type << " " << name << "(";
		for (int i = 0; i < params.size(); i++) {
			if (i > 0) {
				out << ", ";
			}
			out << params[i]->type << " " << params[i];
		}
		out << ") {" << endl;
		for (BasicBlock *block : blocks) {
			out << block;
		}
		out << "}" << endl;
	}
};


class User : public Value {
public:
	vector <Use *> values;

	void appendValue(Value *value) {
		if (value) {
			Use *use = new Use(this, value, values.size());
			values.emplace_back(use);
		} else {
			values.emplace_back(nullptr);
		}
	}

	void removeValue(Use *use) {
		for (int i = 0; i < values.size(); i++) {
			if (values[i] == use) {
				values[i]->destroy();
				values.erase(values.begin() + i);
				break;
			}
		}
		for (int i = 0; i < values.size(); i++) {
			values[i]->index = i;
		}
	}

	void setValue(int index, Value *value) {
		if (values[index]) {
			values[index]->destroy();
		}
		if (value) {
			Use *use = new Use(this, value, index);
			values[index] = use;
		} else {
			values[index] = nullptr;
		}
	}

	void relabel() {
		for (int i = 0; i < values.size(); i++) {
			values[i]->index = i;
		}
	}

	void destroy() {
		for (Use *use : values) {
			if (use) {
				use->destroy();
			}
		}
	}

	void accept(Pass &visitor) {
		visitor.visitUser(this);
	}
};


class GlobalVar : public User, public LinkedListItem {
public:
	Value *reg = nullptr;
	Scp::Variable *var = nullptr;

	GlobalVar(Scp::Variable *var, const string &name = string()) {
		this->var = var;
		if (name.length() == 0) {
			this->name = "@" + var->ident.token;
		} else {
			this->name = name;
		}
		type = var->symType;
		type.toIRType();
		reg = new Value(type, this->name);
		reg->regId = UNAVAILABLE;
		appendValue(reg);
		var->irValue = reg;
	}

	GlobalVar(Value *value) {
		name = value->name;
		type = value->type;
		type.toIRType();
		appendValue(value);
		reg = new Value(type, name);
		reg->regId = UNAVAILABLE;
		appendValue(reg);
	}

	void accept(Pass &visitor) {
		visitor.visitGlobalVar(this);
	}

	void printInitVal(ostream &out, Scp::Variable *var, int &cnt) const {
		var->symType.toIRType();
		out << var->symType << " ";
		if (var->isZeroInit || !var->init) {
			if (var->symType.isArray) {
				out << "zeroinitializer";
			} else {
				out << "0";
			}
			return;
		}
		if (var->symType.isArray) {
			out << "[";
			for (int i = 0; i < var->symType[0]; i++) {
				if (i > 0) {
					out << ", ";
				}
				printInitVal(out, (*var)[i], cnt);
			}
			out << "]";
		} else {
			out << values[cnt];
			cnt++;
		}
	}

	void print(ostream &out) const {
		out << name << " = dso_local global ";
		if (var) {
			int cnt = 1;
			printInitVal(out, var, cnt);
		} else {
			out << type << " " << values[0];
		}
		out << endl;
	}
};


class Inst : public User, public LinkedListItem {
public:
	bool terminate = false;
	bool noDef = false;
	BasicBlock *block;

	virtual Type instType() const = 0;

	void remove() {
		block->remove(this);
		destroy();
	}

	void replaceWith(Inst *inst) {
		inst->block = block;
		block->insertAfter(this, inst);
		block->remove(this);
		destroy();
	}

	void insertBefore(Inst *inst) {
		inst->block = block;
		block->insertBefore(this, inst);
	}

	void insertAfter(Inst *inst) {
		inst->block = block;
		block->insertAfter(this, inst);
	}

	void accept(Pass &visitor) {
		visitor.visitInst(this);
	}

	virtual Inst *copy() const = 0;

	Value *operator [] (int index) const {
		return values[index]->value;
	}
};


class AddInst : public Inst {
public:
	AddInst(const SymType &type, Value *res, Value *op1, Value *op2) {
		this->type = type;
		appendValue(res);
		appendValue(op1);
		appendValue(op2);
	}

	Type instType() const { return TAddInst; };

	void accept(Pass &visitor) {
		visitor.visitAddInst(this);
	}

	Inst *copy() const {
		return new AddInst(type, (*this)[0], (*this)[1], (*this)[2]);
	}

	void print(ostream &out) const {
		out << values[0] << " = add " << type << " " << values[1] << ", " << values[2];
	}
};


class SubInst : public Inst {
public:
	SubInst(const SymType &type, Value *res, Value *op1, Value *op2) {
		this->type = type;
		appendValue(res);
		appendValue(op1);
		appendValue(op2);
	}

	Type instType() const { return TSubInst; };

	void accept(Pass &visitor) {
		visitor.visitSubInst(this);
	}

	Inst *copy() const {
		return new SubInst(type, (*this)[0], (*this)[1], (*this)[2]);
	}

	void print(ostream &out) const {
		out << values[0] << " = sub " << type << " " << values[1] << ", " << values[2];
	}
};


class MulInst : public Inst {
public:
	MulInst(const SymType &type, Value *res, Value *op1, Value *op2) {
		this->type = type;
		appendValue(res);
		appendValue(op1);
		appendValue(op2);
	}

	Type instType() const { return TMulInst; };

	void accept(Pass &visitor) {
		visitor.visitMulInst(this);
	}

	Inst *copy() const {
		return new MulInst(type, (*this)[0], (*this)[1], (*this)[2]);
	}

	void print(ostream &out) const {
		out << values[0] << " = mul " << type << " " << values[1] << ", " << values[2];
	}
};


class SdivInst : public Inst {
public:
	SdivInst(const SymType &type, Value *res, Value *op1, Value *op2) {
		this->type = type;
		appendValue(res);
		appendValue(op1);
		appendValue(op2);
	}

	Type instType() const { return TSdivInst; };

	void accept(Pass &visitor) {
		visitor.visitSdivInst(this);
	}

	Inst *copy() const {
		return new SdivInst(type, (*this)[0], (*this)[1], (*this)[2]);
	}

	void print(ostream &out) const {
		out << values[0] << " = sdiv " << type << " " << values[1] << ", " << values[2];
	}
};


class SremInst : public Inst {
public:
	SremInst(const SymType &type, Value *res, Value *op1, Value *op2) {
		this->type = type;
		appendValue(res);
		appendValue(op1);
		appendValue(op2);
	}

	Type instType() const { return TSremInst; };

	void accept(Pass &visitor) {
		visitor.visitSremInst(this);
	}

	Inst *copy() const {
		return new SremInst(type, (*this)[0], (*this)[1], (*this)[2]);
	}

	void print(ostream &out) const {
		out << values[0] << " = srem " << type << " " << values[1] << ", " << values[2];
	}
};


class IcmpInst : public Inst {
public:
	Type cond;

	IcmpInst(const SymType &type, const Type &cond, Value *res, Value *op1, Value *op2) {
		this->type = type;
		this->cond = cond;
		appendValue(res);
		appendValue(op1);
		appendValue(op2);
	}

	Type instType() const { return TIcmpInst; };

	void accept(Pass &visitor) {
		visitor.visitIcmpInst(this);
	}

	Inst *copy() const {
		return new IcmpInst(type, cond, (*this)[0], (*this)[1], (*this)[2]);
	}

	void print(ostream &out) const {
		out << values[0] << " = icmp " << cond << " " << type << " " << values[1] << ", " << values[2];
	}
};


class CallInst : public Inst {
public:
	CallInst(const SymType &type, Value *func) {
		this->type = type;
		appendValue(func);
		noDef = true;
	}

	CallInst(const SymType &type, Value *res, Value *func) {
		this->type = type;
		appendValue(res);
		appendValue(func);
	}

	Type instType() const { return TCallInst; };

	void accept(Pass &visitor) {
		visitor.visitCallInst(this);
	}

	Inst *copy() const {
		CallInst *inst = nullptr;
		if (this->noDef) {
			inst = new CallInst(type, (*this)[0]);
			for (int i = 1; i < values.size(); i++) {
				inst->appendValue((*this)[i]);
			}
		} else {
			inst = new CallInst(type, (*this)[0], (*this)[1]);
			for (int i = 2; i < values.size(); i++) {
				inst->appendValue((*this)[i]);
			}
		}
		return inst;
	}

	void print(ostream &out) const {
		int index = 0;
		if (!noDef) {
			out << values[0] << " = ";
			index = 1;
		}
		Function *func = (Function *)(values[index]->value);
		out << "call " << type << " " << func->name << "(";
		index++;
		for (int i = index; i < values.size(); i++) {
			if (i > index) {
				out << ", ";
			}
			out << func->params[i - index]->type << " " << values[i];
		}
		out << ")";
	}
};


class AllocaInst : public Inst {
public:
	Scp::Variable *var = nullptr;

	AllocaInst(const SymType &type, Value *res, Scp::Variable *var) {
		this->type = type;
		appendValue(res);
		this->var = var;
	}

	Type instType() const { return TAllocaInst; };

	void accept(Pass &visitor) {
		visitor.visitAllocaInst(this);
	}

	Inst *copy() const {
		return new AllocaInst(type, (*this)[0], var);
	}

	void print(ostream &out) const {
		out << values[0] << " = alloca " << type;
	}
};


class LoadInst : public Inst {
public:
	LoadInst(const SymType &type, Value *res, Value *addr) {
		this->type = type;
		appendValue(res);
		appendValue(addr);
	}

	Type instType() const { return TLoadInst; };

	void accept(Pass &visitor) {
		visitor.visitLoadInst(this);
	}

	Inst *copy() const {
		return new LoadInst(type, (*this)[0], (*this)[1]);
	}

	void print(ostream &out) const {
		out << values[0] << " = load " << type << ", " << type << "* " << values[1];
	}
};


class StoreInst : public Inst {
public:
	StoreInst(const SymType &type, Value *res, Value *addr) {
		this->type = type;
		appendValue(res);
		appendValue(addr);
		noDef = true;
	}

	Type instType() const { return TStoreInst; };

	void accept(Pass &visitor) {
		visitor.visitStoreInst(this);
	}

	Inst *copy() const {
		return new StoreInst(type, (*this)[0], (*this)[1]);
	}

	void print(ostream &out) const {
		out << "store " << type << " " << values[0] << ", " << type << "* " << values[1];
	}
};


class GetPtrInst : public Inst {
public:
	GetPtrInst(const SymType &type, Value *res, Value *addr, bool refine = true) {
		this->type = type;
		appendValue(res);
		appendValue(addr);
		if (refine) {
			if (this->type.isPointer) {
				this->type.pop();
			} else {
				appendValue(new NumberLiteral(0));
			}
		}
		type2 = this->type.toPointer();
	}

	Type instType() const { return TGetPtrInst; };

	void accept(Pass &visitor) {
		visitor.visitGetPtrInst(this);
	}

	Inst *copy() const {
		GetPtrInst *inst = new GetPtrInst(type, (*this)[0], (*this)[1], false);
		for (int i = 2; i < values.size(); i++) {
			inst->appendValue((*this)[i]);
		}
		return inst;
	}

	void print(ostream &out) const {
		out << values[0] << " = getelementptr " << type << ", " << type2 << " " << values[1];
		for (int i = 2; i < values.size(); i++) {
			out << ", " << values[i]->value->type << " " << values[i];
		}
	}
};


class PhiInst : public Inst {
public:
	Scp::Variable *var = nullptr;

	PhiInst(const SymType &type, Value *res, Scp::Variable *var = nullptr) {
		this->type = type;
		appendValue(res);
		this->var = var;
	}

	Type instType() const { return TPhiInst; };

	void accept(Pass &visitor) {
		visitor.visitPhiInst(this);
	}

	Inst *copy() const {
		PhiInst *inst = new PhiInst(type, (*this)[0], var);
		for (int i = 1; i < values.size(); i++) {
			inst->appendValue((*this)[i]);
		}
		return inst;
	}

	void print(ostream &out) const {
		out << values[0] << " = phi " << type;
		for (int i = 1; i < values.size(); i += 2) {
			if (i > 1) {
				out << ",";
			}
			out << " [" << values[i] << ", " << ((BasicBlock *)values[i + 1]->value)->label << "]";
		}
	}
};


class ZextInst : public Inst {
public:
	ZextInst(const SymType &type, const SymType &type2, Value *res, Value *op) {
		this->type = type;
		this->type2 = type2;
		appendValue(res);
		appendValue(op);
	}

	Type instType() const { return TZextInst; };

	void accept(Pass &visitor) {
		visitor.visitZextInst(this);
	}

	Inst *copy() const {
		return new ZextInst(type, type2, (*this)[0], (*this)[1]);
	}

	void print(ostream &out) const {
		out << values[0] << " = zext " << type << " " << values[1] << " to " << type2;
	}
};


class TruncInst : public Inst {
public:
	TruncInst(const SymType &type, const SymType &type2, Value *res, Value *op) {
		this->type = type;
		this->type2 = type2;
		appendValue(res);
		appendValue(op);
	}

	Type instType() const { return TTruncInst; };

	void accept(Pass &visitor) {
		visitor.visitTruncInst(this);
	}

	Inst *copy() const {
		return new TruncInst(type, type2, (*this)[0], (*this)[1]);
	}

	void print(ostream &out) const {
		out << values[0] << " = trunc " << type << " " << values[1] << " to " << type2;
	}
};


class BrInst : public Inst {
public:
	bool hasCond = false;

	BrInst(Value *target) {
		type = Int1;
		terminate = true;
		appendValue(target);
	}

	BrInst(Value *cond, Value *trueBranch, Value *falseBranch) {
		type = Int1;
		terminate = true;
		if (trueBranch == falseBranch) {
			appendValue(trueBranch);
			return;
		}
		hasCond = true;
		appendValue(cond);
		appendValue(trueBranch);
		appendValue(falseBranch);
	}

	Type instType() const { return TBrInst; };

	void accept(Pass &visitor) {
		visitor.visitBrInst(this);
	}

	Inst *copy() const {
		BrInst *inst = nullptr;
		if (hasCond) {
			inst = new BrInst((*this)[0], (*this)[1], (*this)[2]);
		} else {
			inst = new BrInst((*this)[0]);
		}
		return inst;
	}

	void print(ostream &out) const {
		if (hasCond) {
			out << "br " << type << " " << values[0] << ", label " << ((BasicBlock *)values[1]->value)->label << ", label " << ((BasicBlock *)values[2]->value)->label;
		} else {
			out << "br label " << ((BasicBlock *)values[0]->value)->label;
		}
	}
};


class RetInst : public Inst {
public:
	RetInst(const SymType &type = SymType(TVoid)) {
		this->type = type;
		terminate = true;
	}
	RetInst(const SymType &type, Value *ret) {
		this->type = type;
		terminate = true;
		appendValue(ret);
	}

	Type instType() const { return TRetInst; };

	void accept(Pass &visitor) {
		visitor.visitRetInst(this);
	}

	Inst *copy() const {
		RetInst *inst = nullptr;
		if (type == SymType(TVoid)) {
			inst = new RetInst();
		} else {
			inst = new RetInst(type, (*this)[0]);
		}
		return inst;
	}

	void print(ostream &out) const {
		out << "ret " << type;
		if (type != SymType(TVoid)) {
			out << " " << values[0];
		}
	}
};


class Module {
public:
	LinkedList <GlobalVar> globalVars;
	LinkedList <Function> funcs;

	bool changed = false;

	Function *getint = nullptr;
	Function *putint = nullptr;
	Function *putch = nullptr;
	Function *putstr = nullptr;

	Module() {
		getint = new Function(Int32, "@getint");
		putint = new Function(Int32, "@putint");
		putint->appendParam(new Value(Int32));
		putch = new Function(Int32, "@putch");
		putch->appendParam(new Value(Int32));
		putstr = new Function(Void, "@putstr");
		putstr->appendParam(new Value(Int8Ptr));
		getint->reserved = true;
		putint->reserved = true;
		putch->reserved = true;
		putstr->reserved = true;
		if (CPL_IR_EnableLibsysy) {
			appendFunc(getint);
			appendFunc(putint);
			appendFunc(putch);
			appendFunc(putstr);
		}
	}

	void appendFunc(Function *func) {
		funcs.append(func);
	}

	void removeFunc(Function *func) {
		funcs.erase(func);
	}

	void appendGlobalVar(GlobalVar *globalVar) {
		globalVars.append(globalVar);
	}

	void removeGlobalVar(GlobalVar *globalVar) {
		globalVars.erase(globalVar);
	}

	void accept(Pass &visitor) {
		visitor.visitModule(this);
	}

	friend ostream& operator << (ostream &out, const Module &module) {
		module.print(out);
		return out;
	}
	friend ostream& operator << (ostream &out, const Module *module) {
		module->print(out);
		return out;
	}

	void print(ostream &out) const {
		out << "; IR Module";
		if (CPL_IR_EnableLibsysy) {
			out << endl;
			out << "declare i32 @getint()" << endl;
			out << "declare void @putint(i32)" << endl;
			out << "declare void @putch(i32)" << endl;
			out << "declare void @putstr(i8*)" << endl;
		}
		if (globalVars.size() > 0) {
			out << endl;
			for (GlobalVar *globalVar : globalVars) {
				out << globalVar;
			}
		}
		for (Function *func : funcs) {
			out << func;
		}
	}
};

}

#endif