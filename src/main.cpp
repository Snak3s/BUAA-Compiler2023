#include "config.h"
#include "errors.h"
#include "lexer.h"
#include "parser.h"
#include "scopebuilder.h"
#include "irgenerator.h"
#include "irpass/iroptimizer.h"
#include "mipsgenerator.h"
#include "mipspass/mipsoptimizer.h"

using namespace std;
using namespace IO;
using namespace AST;
using namespace IR;
using namespace MIPS;


Err::Log errors;

int main()
{
	Lex::Lexer tokens(&in, &errors);
	Parse::Parser ast(tokens, &errors);
	if (ast.isError) {
		errors.print();
		return CPL_ErrorCode;
	}

	ScopeBuilder scopeBuilder(&errors);
	ast.accept(scopeBuilder);
	if (errors.isError()) {
		errors.print();
		return CPL_ErrorCode;
	}

	IRGenerator irGenerator;
	ast.accept(irGenerator);

	Module *irModule = irGenerator.module;

	IROptimizer irOptimizer;
	irModule->accept(irOptimizer);

	if (CPL_Gen_IR) {
		out << irModule;
	}

	MIPSGenerator mipsGenerator;
	irModule->accept(mipsGenerator);

	MModule *mipsModule = mipsGenerator.module;
	MIPSOptimizer mipsOptimizer;
	mipsModule->accept(mipsOptimizer);

	if (CPL_Gen_MIPS) {
		out << mipsModule;
	}

	return 0;
}