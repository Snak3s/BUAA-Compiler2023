#ifndef __CPL_LOCATION_H__
#define __CPL_LOCATION_H__

#include <iostream>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>


#include "config.h"


namespace Loc {

using namespace std;


struct SourceCode {
	vector <string> src;
	int line;

	SourceCode() {
		line = 0;
		src.emplace_back("");
		newLine();
	}

	void append(char cur) {
		src[line] = src[line] + cur;
	}

	void newLine() {
		line++;
		src.emplace_back("");
	}

	string getLine(int line) { return src[line]; }
};


struct Location {
	SourceCode *src;
	int line, col;

	Location() {}
	Location(SourceCode *src, int line = 1, int col = 0) {
		this->src = src;
		this->line = line;
		this->col = col;
	}

	void append(char cur) {
		col++;
		src->append(cur);
	}

	void newLine() {
		line++;
		col = 0;
		src->newLine();
	}

	void locate(ostream &out) {
		if (CPL_Err_ConsoleColor) {
			out << "\033[01;30m";
		}
		int len = 1;
		for (int val = 1; line >= val * 10; len++, val *= 10);
		for (int i = len + 1; i <= 5; i++)
			out << " ";
		out << line << " | ";
		if (CPL_Err_ConsoleColor) {
			out << "\033[0m";
		}
		out << src->getLine(line) << endl;
		if (CPL_Err_ConsoleColor) {
			out << "\033[01;30m";
		}
		out << "      |";
		for (int i = 1; i <= col; i++)
			out << " ";
		if (CPL_Err_ConsoleColor) {
			out << "\033[01;36m";
		}
		out << "^" << endl;
		if (CPL_Err_ConsoleColor) {
			out << "\033[0m";
		}
	}

	friend ostream& operator << (ostream &out, const Location &loc) {
		out << loc.line << ":" << loc.col << ": ";
		return out;
	}
};

}

#endif