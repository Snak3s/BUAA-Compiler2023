#ifndef __CPL_SCOPE_H__
#define __CPL_SCOPE_H__

#include <iostream>
#include <map>
#include <vector>

#include "config.h"
#include "lexer.h"
#include "symtypes.h"


namespace IR {

class Value;
class Function;

}


namespace Scp {

using namespace std;
using namespace Lex;
using namespace SymTypes;


struct Scope;


struct Variable {
	Token ident;
	SymType symType;
	bool init = false;
	bool isZeroInit = false;
	int initVal = 0;
	vector <Variable *> initArray;
	Variable *zeroInit = nullptr;
	IR::Value *irValue = nullptr;

	Variable() {}
	Variable(const Token &ident, const SymType &symType) {
		this->ident = ident;
		this->symType = symType;
	}

	bool isConst() {
		return symType.isConst;
	}

	int get() {
		return initVal;
	}

	Variable *at(int dim) {
		if (dim < initArray.size()) {
			return initArray[dim];
		} else {
			if (!zeroInit) {
				zeroInit = new Variable(ident, symType);
				zeroInit->symType.pop();
				zeroInit->isZeroInit = true;
			}
			return zeroInit;
		}
	}

	Variable *operator [] (int dim) {
		return at(dim);
	}
};


struct Function {
	Token ident;
	SymType funcType;
	vector <Variable *> params;
	Scope *scope = nullptr;
	IR::Function *irFunc = nullptr;

	Function() {}
	Function(const Token &ident, const SymType &funcType) {
		this->ident = ident;
		this->funcType = funcType;
	}
};


static int scopeId = 0;

struct Scope {
	int id;
	Scope *parent = nullptr;
	map <string, Variable> vars;
	map <string, Function> funcs;

	Scope() { id = scopeId++; }
	Scope(Scope *parent) {
		this->parent = parent;
		id = scopeId++;
	}

	Scope *allocate() {
		Scope *scope = new Scope(this);
		return scope;
	}

	bool defineVar(const string &ident, const Variable &var) {
		if (vars.count(ident) || funcs.count(ident)) {
			return false;
		}
		vars[ident] = var;
		return true;
	}
	bool defineVar(const Variable &var) {
		return defineVar(var.ident.token, var);
	}

	bool defineFunc(const string &ident, const Function &func) {
		if (vars.count(ident) || funcs.count(ident)) {
			return false;
		}
		funcs[ident] = func;
		return true;
	}
	bool defineFunc(const Function &func) {
		return defineFunc(func.ident.token, func);
	}

	Variable *getVar(const string &ident) {
		if (vars.count(ident)) {
			return &vars[ident];
		}
		if (parent) {
			return parent->getVar(ident);
		}
		return nullptr;
	}
	Variable *getVar(const Token &token) {
		return getVar(token.token);
	}

	Function *getFunc(const string &ident) {
		if (funcs.count(ident)) {
			return &funcs[ident];
		}
		if (parent) {
			return parent->getFunc(ident);
		}
		return nullptr;
	}
	Function *getFunc(const Token &token) {
		return getFunc(token.token);
	}
};

}

#endif