#ifndef __CPL_SYM_TYPES_H__
#define __CPL_SYM_TYPES_H__

#include <vector>

#include "types.h"
#include "lexer.h"


namespace SymTypes {

using namespace std;
using namespace Types;


const Type TInt = Type("int");
const Type TVoid = Type("void");

const Type TInt1 = Type("i1");
const Type TInt8 = Type("i8");
const Type TInt32 = Type("i32");
const Type TLabel = Type("label");


struct SymType {
	Type type = TVoid;
	bool isArray = false;
	bool isConst = false;
	bool isPointer = false;
	bool isIRType = false;
	vector <int> dim;
	int dimLen = 0, dimOffset = 0;

	SymType() {}
	SymType(const Type &type) {
		this->type = type;
	}
	SymType(const Type &type, bool isPointer) {
		this->type = type;
		this->isPointer = isPointer;
	}
	SymType(const Lex::Token &token, bool isConst = false) {
		if (token == Lex::INTTK) {
			type = TInt;
		}
		if (token == Lex::VOIDTK) {
			type = TVoid;
		}
		this->isConst = isConst;
	}

	void toIRType() {
		if (type == TInt) {
			type = TInt32;
		}
		isIRType = true;
	}

	SymType baseType() const {
		return SymType(type);
	}

	int getSize() const {
		int size = 0;
		if (type == TInt1 || type == TInt8) {
			size = 1;
		}
		if (type == TInt32) {
			size = 4;
		}
		for (int i = 0; i < dimLen; i++) {
			size *= at(i);
		}
		return size;
	}

	SymType toPointer() const {
		SymType pointer = *this;
		if (!pointer.isPointer) {
			pointer.prepend(0);
			pointer.isPointer = true;
		}
		return pointer;
	}

	void prepend(int dim) {
		this->dim.insert(this->dim.begin(), dim);
		isArray = true;
		dimLen++;
	}

	void append(int dim) {
		this->dim.emplace_back(dim);
		isArray = true;
		dimLen++;
	}

	void pop() {
		if (isArray) {
			dimLen--;
			dimOffset++;
		}
		if (dimLen == 0) {
			isArray = false;
		}
		isPointer = false;
	}

	int at(int dim) const {
		if (dim < 0 || dim >= dimLen) {
			return -1;
		}
		return this->dim[dimOffset + dim];
	}

	int operator [] (int dim) const {
		return at(dim);
	}

	friend ostream& operator << (ostream &out, const SymType &symType) {
		if (symType.isIRType || symType.type == TInt1 || symType.type == TInt8 || symType.type == TInt32) {
			int firstDim = symType.isPointer ? 1 : 0;
			for (int i = firstDim; i < symType.dimLen; i++) {
				out << "[" << symType[i] << " x ";
			}
			out << symType.type;
			for (int i = firstDim; i < symType.dimLen; i++) {
				out << "]";
			}
			if (symType.isPointer) {
				out << "*";
			}
		} else {
			out << symType.type;
			if (symType.isArray) {
				int firstDim = 0;
				if (symType.isPointer) {
					out << "[]";
					firstDim = 1;
				}
				for (int i = firstDim; i < symType.dimLen; i++) {
					out << "[" << symType[i] << "]";
				}
			}
		}
		return out;
	}

	friend bool operator == (const SymType &a, const SymType &b) {
		if (a.type != b.type) {
			return false;
		}
		if (a.dimLen != b.dimLen) {
			return false;
		}
		int firstDim = 0;
		if (a.isPointer || b.isPointer) {
			firstDim = 1;
		}
		for (int i = firstDim; i < a.dimLen; i++) {
			if (a[i] != b[i]) {
				return false;
			}
		}
		return true;
	}
	friend bool operator != (const SymType &a, const SymType &b) {
		return !(a == b);
	}
	friend bool operator <= (const SymType &a, const SymType &b) {
		if (a.type != b.type) {
			return false;
		}
		if (a.dimLen != b.dimLen) {
			return false;
		}
		for (int i = 0; i < a.dimLen; i++) {
			if (a[i] < 0 || b[i] <= a[i]) {
				return false;
			}
		}
		return true;
	}
};


const SymType Void = SymType(TVoid);
const SymType Int1 = SymType(TInt1);
const SymType Int8 = SymType(TInt8);
const SymType Int32 = SymType(TInt32);
const SymType Int8Ptr = SymType(TInt8, true);
const SymType Int32Ptr = SymType(TInt32, true);

}

#endif