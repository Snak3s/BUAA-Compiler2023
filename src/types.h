#ifndef __CPL_TYPES_H__
#define __CPL_TYPES_H__

#include <iostream>


namespace Types {

using namespace std;


static int typeId = 0;

struct Type {
	int id;
	const char *name = nullptr;
	const char *value = nullptr;

	Type() { id = typeId++; }
	Type(const char *name) {
		id = typeId++;
		this->name = name;
	}
	Type(const char *name, const char *value) {
		id = typeId++;
		this->name = name;
		this->value = value;
	}

	friend ostream& operator << (ostream &out, const Type &type) {
		if (type.name) {
			out << type.name;
		}
		return out;
	}

	friend bool operator == (const Type &a, const Type &b) {
		return a.id == b.id;
	}
	friend bool operator != (const Type &a, const Type &b) {
		return a.id != b.id;
	}
};

}

#endif