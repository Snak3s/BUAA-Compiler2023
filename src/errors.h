#ifndef __CPL_ERRORS_H__
#define __CPL_ERRORS_H__

#include <iostream>
#include <vector>
#include <algorithm>
#include <cstring>

#include "config.h"
#include "types.h"
#include "location.h"


namespace Err {

using namespace std;
using namespace Types;
using namespace Loc;


const Type IllegalSym   = Type("a", "illegal symbol");
const Type DupIdent     = Type("b", "duplicated identifier");
const Type UndefIdent   = Type("c", "undefined identifier");
const Type FuncArgsCnt  = Type("d", "function arguments count mismatched with definition");
const Type FuncArgType  = Type("e", "function argument type mismatched with definition");
const Type UnexpReturn  = Type("f", "unexpected return statement");
const Type NoReturn     = Type("g", "return statement expected");
const Type ConstAssign  = Type("h", "cannot assign to constant");
const Type NoSemicol    = Type("i", "semicolon token ';' expected");
const Type NoRParent    = Type("j", "right parenthesis token ')' expected");
const Type NoRBracket   = Type("k", "right bracket token ']' expected");
const Type PrintfArgs   = Type("l", "arguments mismatched with format string");
const Type BreakCont    = Type("m", "unexpected break or continue statement without loop");
const Type UnknownToken = Type("", "unknown token");
const Type UnexpToken   = Type("", "unexpected token");
const Type DivByZero    = Type("", "division by zero");
const Type NegArrLength = Type("", "array length should be non-negative integer");
const Type IndetArrLen  = Type("", "array length cannot be determined");
const Type InitValCnt   = Type("", "initial value mismatched with definition");
const Type IndetInitVal = Type("", "initial value cannot be determined");
const Type ArrayDim     = Type("", "array dimensions mismatched with definition");
const Type InvalOpTypes = Type("", "operation should be applied to 'int' type");
const Type IndexNotInt  = Type("", "array index should be 'int' type");
const Type CondNotInt   = Type("", "condition should be int type");
const Type RetValType   = Type("", "return value type mismatched");


using Error = pair <Location, Type>;

static bool compare (const Error &a, const Error &b) {
	if (a.first.line != b.first.line) {
		return a.first.line < b.first.line;
	}
	return a.first.col < b.first.col;
}


struct Log {
	vector <Error> log;

	Log() {}

	void raise(const Location &loc, const Type &type) {
		log.emplace_back(make_pair(loc, type));
	}

	void print() {
		sort(log.begin(), log.end(), compare);
		for (auto error : log) {
			if (CPL_Err_PlainText) {
				if (strlen(error.second.name) > 0) {
					IO::err << error.first.line << " " << error.second << endl;
				}
				continue;
			}
			if (CPL_Err_ConsoleColor) {
				IO::err << "\033[01;37m" << error.first << "\033[0m";
				IO::err << "\033[01;31m" << "error: " << "\033[0m";
				IO::err << "\033[01;37m" << error.second.value << "\033[0m";
				IO::err << endl;
			} else {
				IO::err << error.first << "error: " << error.second.value << endl;
			}
			error.first.locate(IO::err);
		}
	}

	bool isError() { return log.size() > 0; }
};

}

#endif