#ifndef __CPL_PARSER_H__
#define __CPL_PARSER_H__

#include <iostream>
#include <map>
#include <vector>

#include "config.h"
#include "location.h"
#include "lexer.h"
#include "ast.h"
#include "symtypes.h"
#include "astvisitor.h"


namespace Parse {

using namespace std;
using namespace Loc;
using namespace Lex;
using namespace AST;
using namespace SymTypes;
using namespace IO;


static Token TokDefault = Token(DEFAULT);
static Token TokEndf = Token(ENDF);


class Parser {
public:
	vector <Token> tokens;
	Location lastLoc, loc;
	int cur, pos;
	bool skip = false;
	bool isError = false;

	Node *root;
	Err::Log *errors;

	Parser(Lexer &lexer, Err::Log *errors = new Err::Log()) {
		this->tokens = lexer.tokens;
		cur = pos = 0;
		lastLoc = loc = Location();
		this->errors = errors;
		run();
	}
	Parser(vector <Token> tokens, Err::Log *errors = new Err::Log()) {
		this->tokens = tokens;
		cur = pos = 0;
		lastLoc = loc = Location();
		this->errors = errors;
		run();
	}

	void raise(const Location &loc, const Type &type) {
		if (!skip) {
			errors->raise(loc, type);
		}
		skip = true;
	}

	Token &match(const Type &tokenType = DEFAULT) {
		if (cur < tokens.size() && (tokens[cur] == tokenType || tokenType == DEFAULT)) {
			if (CPL_Parser_PrintTokens) {
				out << tokens[cur] << endl;
			}
			skip = false;
			pos = cur++;
			lastLoc = loc;
			if (cur < tokens.size()) {
				loc = tokens[cur].loc;
			}
			return tokens[cur - 1];
		}

		if (CPL_Parser_InsMissingToken) {
			if (tokenType == SEMICN) {
				errors->raise(lastLoc, Err::NoSemicol);
				return TokDefault;
			}
			if (tokenType == RPARENT) {
				errors->raise(lastLoc, Err::NoRParent);
				return TokDefault;
			}
			if (tokenType == RBRACK) {
				errors->raise(lastLoc, Err::NoRBracket);
				return TokDefault;
			}
		}

		if (CPL_Parser_RaiseUnexpToken) {
			raise(loc, Err::UnexpToken);
			while (!lookaheadIn({SEMICN, RBRACE, ENDF, LBRACE})) {
				pos = cur++;
				lastLoc = loc;
				loc = tokens[cur].loc;
			}
		}

		isError = true;
		return TokDefault;
	}

	bool tryMatch(const Type &tokenType = DEFAULT) {
		if (pos < tokens.size() && (tokens[pos] == tokenType || tokenType == DEFAULT)) {
			pos++;
			return true;
		}
		return false;
	}

	bool tryMatchSeq(const vector <Type> &tokenTypes) {
		pos = cur;
		for (auto tokenType : tokenTypes) {
			if (!tryMatch(tokenType))
				return false;
		}
		return true;
	}

	Token &lookahead(int cnt = 0) {
		if (cur + cnt < tokens.size()) {
			return tokens[cur + cnt];
		}
		return TokEndf;
	}

	bool lookaheadIn(const vector <Type> &tokenTypes) {
		for (auto tokenType : tokenTypes) {
			if (lookahead() == tokenType) {
				return true;
			}
		}
		return false;
	}

	void run() {
		root = getCompUnit();
	}

	void accept(Visitor &visitor) {
		if (root) {
			root->accept(visitor);
		}
	}

	void startParse(Node *node) {
		node->firstLoc = loc;
	}

	void finishParse(Node *node) {
		node->lastLoc = lastLoc;
		if (skip) {
			node->dummy = true;
		}
		if (CPL_Parser_PrintTokens) {
			out << node << endl;
		}
	}

	Node *getCompUnit() {
		CompUnit *compUnit = new CompUnit();
		startParse(compUnit);
		Node *node = getDeclOrFuncDef();
		while (node && (node->type() == TConstDecl || node->type() == TVarDecl)) {
			compUnit->decl.emplace_back(node);
			node = getDeclOrFuncDef();
		}
		while (node && node->type() == TFuncDef) {
			compUnit->func.emplace_back(node);
			node = getDeclOrFuncDef();
		}
		if (node && node->type() == TMainFuncDef) {
			compUnit->mainFunc = node;
		}
		if (!compUnit->mainFunc) {
			isError = true;
		}
		finishParse(compUnit);
		return compUnit;
	}

	Node *getDecl() {
		if (lookahead() == CONSTTK) {
			return getConstDecl();
		}
		return getVarDecl();
	}

	Node *getDeclOrFuncDef() {
		if (lookahead() == CONSTTK) {
			return getConstDecl();
		}
		return getVarDeclOrFuncDef();
	}

	Node *getVarDeclOrFuncDef() {
		if (tryMatchSeq({INTTK, MAINTK, LPARENT})) {
			return getMainFuncDef();
		} else if (tryMatchSeq({VOIDTK}) || tryMatchSeq({INTTK, IDENFR, LPARENT})) {
			return getFuncDef();
		} else if (tryMatchSeq({INTTK, IDENFR})) {
			return getVarDecl();
		}
		return nullptr;
	}

	SymType getBType() {
		if (lookahead() == INTTK) {
			return SymType(match(INTTK));
		}
		return SymType();
	}

	Node *getConstDecl() {
		ConstDecl *constDecl = new ConstDecl();
		startParse(constDecl);
		match(CONSTTK);
		SymType bType = getBType();
		bType.isConst = true;
		constDecl->constDef.emplace_back(getConstDef(bType));
		while (lookahead() == COMMA) {
			match(COMMA);
			constDecl->constDef.emplace_back(getConstDef(bType));
		}
		match(SEMICN);
		finishParse(constDecl);
		return constDecl;
	}

	Node *getConstDef(SymType symType) {
		ConstDef *constDef = new ConstDef();
		startParse(constDef);
		constDef->ident = match(IDENFR);
		constDef->symType = symType;
		while (lookahead() == LBRACK) {
			match(LBRACK);
			constDef->dimExp.emplace_back(getConstExp());
			match(RBRACK);
		}
		match(ASSIGN);
		constDef->initVal = getConstInitVal();
		finishParse(constDef);
		return constDef;
	}

	Node *getConstInitVal() {
		ConstInitVal *constInitVal = new ConstInitVal();
		startParse(constInitVal);
		if (lookahead() == LBRACE) {
			match(LBRACE);
			constInitVal->isArray = true;
			constInitVal->initVal.emplace_back(getConstInitVal());
			while (lookahead() == COMMA) {
				match(COMMA);
				constInitVal->initVal.emplace_back(getConstInitVal());
			}
			match(RBRACE);
		} else {
			constInitVal->expr = getConstExp();
		}
		finishParse(constInitVal);
		return constInitVal;
	}

	Node *getVarDecl() {
		VarDecl *varDecl = new VarDecl();
		startParse(varDecl);
		SymType bType = getBType();
		varDecl->varDef.emplace_back(getVarDef(bType));
		while (lookahead() == COMMA) {
			match(COMMA);
			varDecl->varDef.emplace_back(getVarDef(bType));
		}
		match(SEMICN);
		finishParse(varDecl);
		return varDecl;
	}

	Node *getVarDef(SymType symType) {
		VarDef *varDef = new VarDef();
		startParse(varDef);
		varDef->ident = match(IDENFR);
		varDef->symType = symType;
		while (lookahead() == LBRACK) {
			match(LBRACK);
			varDef->dimExp.emplace_back(getConstExp());
			match(RBRACK);
		}
		if (lookahead() == ASSIGN) {
			match(ASSIGN);
			varDef->initVal = getInitVal();
		}
		finishParse(varDef);
		return varDef;
	}

	Node *getInitVal() {
		InitVal *initVal = new InitVal();
		startParse(initVal);
		if (lookahead() == LBRACE) {
			match(LBRACE);
			initVal->isArray = true;
			initVal->initVal.emplace_back(getInitVal());
			while (lookahead() == COMMA) {
				match(COMMA);
				initVal->initVal.emplace_back(getInitVal());
			}
			match(RBRACE);
		} else {
			initVal->expr = getExp();
		}
		finishParse(initVal);
		return initVal;
	}

	Node *getFuncDef() {
		FuncDef *funcDef = new FuncDef();
		startParse(funcDef);
		funcDef->funcType = getFuncType();
		funcDef->ident = match(IDENFR);
		match(LPARENT);
		if (!lookaheadIn({RPARENT, LBRACE})) {
			funcDef->params = getFuncFParams();
		}
		match(RPARENT);
		funcDef->block = getBlock();
		finishParse(funcDef);
		return funcDef;
	}

	Node *getMainFuncDef() {
		MainFuncDef *mainFuncDef = new MainFuncDef();
		startParse(mainFuncDef);
		match(INTTK);
		mainFuncDef->ident = match(MAINTK);
		match(LPARENT);
		match(RPARENT);
		mainFuncDef->block = getBlock();
		finishParse(mainFuncDef);
		return mainFuncDef;
	}

	Node *getFuncType() {
		FuncType *funcType = new FuncType();
		startParse(funcType);
		if (lookaheadIn({VOIDTK, INTTK})) {
			funcType->symType = SymType(match());
		}
		finishParse(funcType);
		return funcType;
	}

	Node *getFuncFParams() {
		FuncFParams *funcFParams = new FuncFParams();
		startParse(funcFParams);
		funcFParams->params.emplace_back(getFuncFParam());
		while (lookahead() == COMMA) {
			match(COMMA);
			funcFParams->params.emplace_back(getFuncFParam());
		}
		finishParse(funcFParams);
		return funcFParams;
	}

	Node *getFuncFParam() {
		FuncFParam *funcFParam = new FuncFParam();
		startParse(funcFParam);
		funcFParam->symType = getBType();
		funcFParam->ident = match(IDENFR);
		if (lookahead() == LBRACK) {
			funcFParam->isArray = true;
			match(LBRACK);
			match(RBRACK);
			while (lookahead() == LBRACK) {
				match(LBRACK);
				funcFParam->dimExp.emplace_back(getConstExp());
				match(RBRACK);
			}
		}
		finishParse(funcFParam);
		return funcFParam;
	}

	Node *getBlock() {
		Block *block = new Block();
		startParse(block);
		block->start = match(LBRACE);
		while (lookahead() != RBRACE && lookahead() != ENDF) {
			block->items.emplace_back(getBlockItem());
		}
		block->end = match(RBRACE);
		finishParse(block);
		return block;
	}

	Node *getBlockItem() {
		if (lookaheadIn({CONSTTK, INTTK})) {
			return getDecl();
		} else {
			return getStmt();
		}
	}

	Node *getStmt() {
		if (lookahead() == LBRACE) {
			return getStmtBlock();
		} else if (lookahead() == IFTK) {
			return getStmtIf();
		} else if (lookahead() == FORTK) {
			return getStmtFor();
		} else if (lookahead() == BREAKTK) {
			return getStmtBreak();
		} else if (lookahead() == CONTINUETK) {
			return getStmtContinue();
		} else if (lookahead() == RETURNTK) {
			return getStmtReturn();
		} else if (lookahead() == PRINTFTK) {
			return getStmtPrintf();
		}
		tryMatchSeq({});
		while (!tryMatch(SEMICN)) {
			if (tryMatch(ASSIGN)) {
				if (tryMatch(GETINTTK)) {
					return getStmtGetInt();
				} else {
					return getStmtAssign();
				}
			}
			if (!tryMatch()) {
				break;
			}
		}
		return getStmtExp();
	}

	Node *getStmtAssign() {
		StmtAssign *stmt = new StmtAssign();
		startParse(stmt);
		stmt->lVal = getLVal();
		match(ASSIGN);
		stmt->expr = getExp();
		match(SEMICN);
		finishParse(stmt);
		return stmt;
	}

	Node *getStmtExp() {
		StmtExp *stmt = new StmtExp();
		startParse(stmt);
		if (!lookaheadIn({SEMICN, RBRACE})) {
			stmt->expr = getExp();
		}
		match(SEMICN);
		finishParse(stmt);
		return stmt;
	}

	Node *getStmtBlock() {
		StmtBlock *stmt = new StmtBlock();
		startParse(stmt);
		stmt->block = getBlock();
		finishParse(stmt);
		return stmt;
	}

	Node *getStmtIf() {
		StmtIf *stmt = new StmtIf();
		startParse(stmt);
		match(IFTK);
		match(LPARENT);
		stmt->cond = getCond();
		match(RPARENT);
		stmt->thenStmt = getStmt();
		if (lookahead() == ELSETK) {
			match(ELSETK);
			stmt->elseStmt = getStmt();
		}
		finishParse(stmt);
		return stmt;
	}

	Node *getStmtFor() {
		StmtFor *stmt = new StmtFor();
		startParse(stmt);
		match(FORTK);
		match(LPARENT);
		if (lookahead() == IDENFR) {
			stmt->initStmt = getForStmt();
		}
		match(SEMICN);
		if (lookahead() != SEMICN) {
			stmt->cond = getCond();
		}
		match(SEMICN);
		if (lookahead() == IDENFR) {
			stmt->stepStmt = getForStmt();
		}
		match(RPARENT);
		stmt->stmt = getStmt();
		finishParse(stmt);
		return stmt;
	}

	Node *getStmtBreak() {
		StmtBreak *stmt = new StmtBreak();
		startParse(stmt);
		match(BREAKTK);
		match(SEMICN);
		finishParse(stmt);
		return stmt;
	}

	Node *getStmtContinue() {
		StmtContinue *stmt = new StmtContinue();
		startParse(stmt);
		match(CONTINUETK);
		match(SEMICN);
		finishParse(stmt);
		return stmt;
	}

	Node *getStmtReturn() {
		StmtReturn *stmt = new StmtReturn();
		startParse(stmt);
		match(RETURNTK);
		if (lookahead() != SEMICN) {
			stmt->expr = getExp();
		}
		match(SEMICN);
		finishParse(stmt);
		return stmt;
	}

	Node *getStmtGetInt() {
		StmtGetInt *stmt = new StmtGetInt();
		startParse(stmt);
		stmt->lVal = getLVal();
		match(ASSIGN);
		match(GETINTTK);
		match(LPARENT);
		match(RPARENT);
		match(SEMICN);
		finishParse(stmt);
		return stmt;
	}

	Node *getStmtPrintf() {
		StmtPrintf *stmt = new StmtPrintf();
		startParse(stmt);
		match(PRINTFTK);
		match(LPARENT);
		stmt->format = match(STRCON);
		while (lookahead() == COMMA) {
			match(COMMA);
			stmt->params.emplace_back(getExp());
		}
		match(RPARENT);
		match(SEMICN);
		finishParse(stmt);
		return stmt;
	}

	Node *getForStmt() {
		ForStmt *forStmt = new ForStmt();
		startParse(forStmt);
		forStmt->lVal = getLVal();
		match(ASSIGN);
		forStmt->expr = getExp();
		finishParse(forStmt);
		return forStmt;
	}

	Node *getExp() {
		Exp *expr = new Exp();
		startParse(expr);
		expr->expr = getAddExp();
		finishParse(expr);
		return expr;
	}

	Node *getCond() {
		Cond *cond = new Cond();
		startParse(cond);
		cond->expr = getLOrExp();
		finishParse(cond);
		return cond;
	}

	Node *getLVal() {
		LVal *lVal = new LVal();
		startParse(lVal);
		lVal->ident = match(IDENFR);
		while (lookahead() == LBRACK) {
			match(LBRACK);
			lVal->dimExp.emplace_back(getExp());
			match(RBRACK);
		}
		finishParse(lVal);
		return lVal;
	}

	Node *getPrimaryExp() {
		PrimaryExp *primaryExp = new PrimaryExp();
		startParse(primaryExp);
		if (lookahead() == LPARENT) {
			match(LPARENT);
			primaryExp->expr = getExp();
			match(RPARENT);
		} else if (lookahead() == INTCON) {
			primaryExp->number = getNumber();
		} else {
			primaryExp->lVal = getLVal();
		}
		finishParse(primaryExp);
		return primaryExp;
	}

	Node *getNumber() {
		Number *number = new Number();
		startParse(number);
		number->number = match(INTCON);
		finishParse(number);
		return number;
	}

	Node *getUnaryExp() {
		UnaryExp *unaryExp = new UnaryExp();
		startParse(unaryExp);
		unaryExp->op = getUnaryOp();
		if (unaryExp->op) {
			unaryExp->expr = getUnaryExp();
		} else if (tryMatchSeq({IDENFR, LPARENT})) {
			unaryExp->funcCall = true;
			unaryExp->ident = match(IDENFR);
			match(LPARENT);
			if (lookahead() != RPARENT) {
				unaryExp->params = getFuncRParams();
			}
			match(RPARENT);
		} else {
			unaryExp->expr = getPrimaryExp();
		}
		finishParse(unaryExp);
		return unaryExp;
	}

	Node *getUnaryOp() {
		UnaryOp *unaryOp = new UnaryOp();
		startParse(unaryOp);
		if (!lookaheadIn({PLUS, MINU, NOT})) {
			return nullptr;
		}
		unaryOp->op = match();
		finishParse(unaryOp);
		return unaryOp;
	}

	Node *getFuncRParams() {
		FuncRParams *funcRParams = new FuncRParams();
		startParse(funcRParams);
		funcRParams->params.emplace_back(getExp());
		while (lookahead() == COMMA) {
			match(COMMA);
			funcRParams->params.emplace_back(getExp());
		}
		finishParse(funcRParams);
		return funcRParams;
	}

	Node *getMulExp() {
		MulExp *mulExp = new MulExp();
		startParse(mulExp);
		mulExp->exprL = getUnaryExp();
		while (lookaheadIn({MULT, DIV, MOD})) {
			finishParse(mulExp);
			MulExp *expr = mulExp;
			mulExp = new MulExp();
			mulExp->exprL = expr;
			mulExp->op = match();
			mulExp->exprR = getUnaryExp();
		}
		finishParse(mulExp);
		return mulExp;
	}

	Node *getAddExp() {
		AddExp *addExp = new AddExp();
		startParse(addExp);
		addExp->exprL = getMulExp();
		while (lookaheadIn({PLUS, MINU})) {
			finishParse(addExp);
			AddExp *expr = addExp;
			addExp = new AddExp();
			addExp->exprL = expr;
			addExp->op = match();
			addExp->exprR = getMulExp();
		}
		finishParse(addExp);
		return addExp;
	}

	Node *getRelExp() {
		RelExp *relExp = new RelExp();
		startParse(relExp);
		relExp->exprL = getAddExp();
		while (lookaheadIn({LSS, GRE, LEQ, GEQ})) {
			finishParse(relExp);
			RelExp *expr = relExp;
			relExp = new RelExp();
			relExp->exprL = expr;
			relExp->op = match();
			relExp->exprR = getAddExp();
		}
		finishParse(relExp);
		return relExp;
	}

	Node *getEqExp() {
		EqExp *eqExp = new EqExp();
		startParse(eqExp);
		eqExp->exprL = getRelExp();
		while (lookaheadIn({EQL, NEQ})) {
			finishParse(eqExp);
			EqExp *expr = eqExp;
			eqExp = new EqExp();
			eqExp->exprL = expr;
			eqExp->op = match();
			eqExp->exprR = getRelExp();
		}
		finishParse(eqExp);
		return eqExp;
	}

	Node *getLAndExp() {
		LAndExp *lAndExp = new LAndExp();
		startParse(lAndExp);
		lAndExp->exprL = getEqExp();
		while (lookahead() == AND) {
			finishParse(lAndExp);
			LAndExp *expr = lAndExp;
			lAndExp = new LAndExp();
			lAndExp->exprL = expr;
			lAndExp->op = match();
			lAndExp->exprR = getEqExp();
		}
		finishParse(lAndExp);
		return lAndExp;
	}

	Node *getLOrExp() {
		LOrExp *lOrExp = new LOrExp();
		startParse(lOrExp);
		lOrExp->exprL = getLAndExp();
		while (lookahead() == OR) {
			finishParse(lOrExp);
			LOrExp *expr = lOrExp;
			lOrExp = new LOrExp();
			lOrExp->exprL = expr;
			lOrExp->op = match();
			lOrExp->exprR = getLAndExp();
		}
		finishParse(lOrExp);
		return lOrExp;
	}

	Node *getConstExp() {
		ConstExp *constExp = new ConstExp();
		startParse(constExp);
		constExp->expr = getAddExp();
		finishParse(constExp);
		return constExp;
	}
};

}

#endif