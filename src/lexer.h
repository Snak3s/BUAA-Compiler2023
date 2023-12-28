#ifndef __CPL_LEXER_H__
#define __CPL_LEXER_H__

#include <iostream>
#include <map>
#include <vector>

#include "config.h"
#include "types.h"
#include "location.h"
#include "errors.h"


namespace Lex {

using namespace std;
using namespace Types;
using namespace Loc;
using namespace IO;


const Type DEFAULT = Type();

const Type IDENFR = Type("IDENFR");
const Type INTCON = Type("INTCON");
const Type STRCON = Type("STRCON");

const Type MAINTK     = Type("MAINTK", "main");
const Type CONSTTK    = Type("CONSTTK", "const");
const Type INTTK      = Type("INTTK", "int");
const Type BREAKTK    = Type("BREAKTK", "break");
const Type CONTINUETK = Type("CONTINUETK", "continue");
const Type IFTK       = Type("IFTK", "if");
const Type ELSETK     = Type("ELSETK", "else");
const Type FORTK      = Type("FORTK", "for");
const Type GETINTTK   = Type("GETINTTK", "getint");
const Type PRINTFTK   = Type("PRINTFTK", "printf");
const Type RETURNTK   = Type("RETURNTK", "return");
const Type VOIDTK     = Type("VOIDTK", "void");

const Type NOT = Type("NOT", "!");
const Type AND = Type("AND", "&&");
const Type OR  = Type("OR", "||");

const Type PLUS = Type("PLUS", "+");
const Type MINU = Type("MINU", "-");
const Type MULT = Type("MULT", "*");
const Type DIV  = Type("DIV", "/");
const Type MOD  = Type("MOD", "%");

const Type LSS = Type("LSS", "<");
const Type LEQ = Type("LEQ", "<=");
const Type GRE = Type("GRE", ">");
const Type GEQ = Type("GEQ", ">=");
const Type EQL = Type("EQL", "==");
const Type NEQ = Type("NEQ", "!=");

const Type ASSIGN  = Type("ASSIGN", "=");
const Type SEMICN  = Type("SEMICN", ";");
const Type COMMA   = Type("COMMA", ",");
const Type LPARENT = Type("LPARENT", "(");
const Type RPARENT = Type("RPARENT", ")");
const Type LBRACK  = Type("LBRACK", "[");
const Type RBRACK  = Type("RBRACK", "]");
const Type LBRACE  = Type("LBRACE", "{");
const Type RBRACE  = Type("RBRACE", "}");

const Type COMT      = Type("COMT", "//");
const Type BLOCKCOMT = Type("BLOCKCOMT", "/*");

const Type ENDF = Type();


static bool isDigit(char c) {
	return '0' <= c && c <= '9';
}
static bool isAlpha(char c) {
	return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z');
}
static bool isBlank(char c) {
	return c <= 32;
}
static bool isSpace(char c) {
	return c == ' ' || c == '\t';
}


struct Token {
	Location loc;
	Type type;
	string token;
	int numVal;
	string strVal;

	Token() {
		this->type = DEFAULT;
	}
	Token(const Type &type) {
		this->type = type;
	}
	Token(const Location &loc, const Type &type) {
		this->loc = loc;
		this->type = type;
	}
	Token(const Location &loc, const Type &type, const string &token) {
		this->loc = loc;
		this->type = type;
		this->token = token;
	}
	Token(const Location &loc, const Type &type, const string &token, int value) {
		this->loc = loc;
		this->type = type;
		this->token = token;
		this->numVal = value;
	}
	Token(const Location &loc, const Type &type, const string &token, const string &value) {
		this->loc = loc;
		this->type = type;
		this->token = token;
		this->strVal = value;
	}

	friend ostream& operator << (ostream &out, const Token &token) {
		out << token.type.name << " " << token.token;
		return out;
	}

	friend bool operator == (const Token &a, const Type &b) {
		return a.type == b;
	}
	friend bool operator != (const Token &a, const Type &b) {
		return a.type != b;
	}
};


class Lexer {
public:
	istream *in;
	SourceCode src;
	Location loc;
	char cur;

	map <string, Type> kwTable;
	vector <string> kwPrefix;
	vector <Token> tokens;
	Err::Log *errors;

	Lexer(istream *in = &cin, Err::Log *errors = new Err::Log()) {
		this->in = in;
		cur = 0;
		src = SourceCode();
		loc = Location(&src);
		this->errors = errors;
		run();
	}

	void registerKeyword(const Type &type) {
		string keyword = string(type.value);
		kwTable[keyword] = type;
		for (int i = 1; i <= keyword.size(); i++) {
			kwPrefix.emplace_back(keyword.substr(0, i));
		}
	}

	void initKeyword() {
		registerKeyword(MAINTK);
		registerKeyword(CONSTTK);
		registerKeyword(INTTK);
		registerKeyword(BREAKTK);
		registerKeyword(CONTINUETK);
		registerKeyword(IFTK);
		registerKeyword(ELSETK);
		registerKeyword(FORTK);
		registerKeyword(GETINTTK);
		registerKeyword(PRINTFTK);
		registerKeyword(RETURNTK);
		registerKeyword(VOIDTK);

		registerKeyword(NOT);
		registerKeyword(AND);
		registerKeyword(OR);

		registerKeyword(PLUS);
		registerKeyword(MINU);
		registerKeyword(MULT);
		registerKeyword(DIV);
		registerKeyword(MOD);

		registerKeyword(LSS);
		registerKeyword(LEQ);
		registerKeyword(GRE);
		registerKeyword(GEQ);
		registerKeyword(EQL);
		registerKeyword(NEQ);

		registerKeyword(ASSIGN);
		registerKeyword(SEMICN);
		registerKeyword(COMMA);
		registerKeyword(LPARENT);
		registerKeyword(RPARENT);
		registerKeyword(LBRACK);
		registerKeyword(RBRACK);
		registerKeyword(LBRACE);
		registerKeyword(RBRACE);

		registerKeyword(COMT);
		registerKeyword(BLOCKCOMT);
	}

	void read() {
		cur = in->get();
		if (cur == '\n') {
			loc.newLine();
			return;
		}
		if (isSpace(cur)) {
			loc.append(' ');
		} else if (!isBlank(cur)) {
			loc.append(cur);
		}
	}

	Token getNumberLiteral() {
		int value = 0;
		Location loc = this->loc;
		string token = "";
		if (cur == '0') {
			token = token + cur;
			read();
			return Token(loc, INTCON, token, value);
		}
		while (isDigit(cur)) {
			value = value * 10 + (cur - '0');
			token = token + cur;
			read();
		}
		return Token(loc, INTCON, token, value);
	}

	Token getIdentifier() {
		string token = "";
		Location loc = this->loc;
		while (isAlpha(cur) || isDigit(cur) || cur == '_') {
			token = token + cur;
			read();
		}
		if (kwTable.count(token)) {
			return Token(loc, kwTable[token], token);
		}
		return Token(loc, IDENFR, token);
	}

	Token getStringLiteral() {
		string token = "";
		string value = "";
		Location loc = this->loc;
		token = token + cur;
		read();
		while (cur != '"') {
			token = token + cur;
			value = value + cur;
			read();
		}
		token = token + cur;
		read();
		return Token(loc, STRCON, token, value);
	}

	Token getKeyword() {
		string token = "";
		Location loc = this->loc;
		while (find(kwPrefix.begin(), kwPrefix.end(), token + cur) != kwPrefix.end()) {
			token = token + cur;
			read();
		}
		if (!kwTable.count(token)) {
			errors->raise(loc, Err::UnknownToken);
			read();
			return nextToken();
		}
		return Token(loc, kwTable[token], token);
	}

	Token nextToken() {
		while (isBlank(cur)) {
			if (cur == EOF) {
				return Token(loc, ENDF);
			}
			read();
		}
		if (isDigit(cur)) {
			return getNumberLiteral();
		}
		if (isAlpha(cur) || cur == '_') {
			return getIdentifier();
		}
		if (cur == '"') {
			return getStringLiteral();
		}
		return getKeyword();
	}

	Token next() {
		Token token = nextToken();
		while (token.type == COMT || token.type == BLOCKCOMT) {
			if (token.type == COMT) {
				while (cur != '\n' && cur != EOF) {
					read();
				}
				token = nextToken();
				continue;
			}
			if (token.type == BLOCKCOMT) {
				while (cur != EOF) {
					while (cur != '*' && cur != EOF) {
						read();
					}
					if (cur == EOF) {
						break;
					}
					read();
					if (cur == '/') {
						read();
						break;
					}
				}
				token = nextToken();
				continue;
			}
		}
		return token;
	}

	void run() {
		initKeyword();
		Token token = next();
		while (token.type != ENDF) {
			tokens.emplace_back(token);
			token = next();
		}

		if (CPL_Lexer_PrintTokens) {
			for (auto token : tokens) {
				out << token << endl;
			}
		}
	}
};

}

#endif