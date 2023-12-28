#ifndef __CPL_CONFIG_H__
#define __CPL_CONFIG_H__

#include <iostream>
#include <fstream>
#include <string>


#define CPL_Debug false

#define CPL_ErrorCode -1


#ifndef CPL_IO_UseStdin
	#define CPL_IO_UseStdin false
#endif
#ifndef CPL_IO_UseStdout
	#define CPL_IO_UseStdout false
#endif
#ifndef CPL_IO_UseStderr
	#define CPL_IO_UseStderr false
#endif

#define CPL_IO_InputFileName "testfile.txt"
#define CPL_IO_OutputFileName "mips.txt"
#define CPL_IO_ErrorFileName "error.txt"


#define CPL_Lexer_PrintTokens false


#define CPL_Parser_InsMissingToken true
#define CPL_Parser_PrintTokens false
#define CPL_Parser_RaiseUnexpToken true


#define CPL_IR_UseComputedValue true
#define CPL_IR_EnableLibsysy true
#define CPL_IR_PrintStrMinLength 2


#define CPL_Opt_EnableSSA true
#define CPL_Opt_IROptimizer true
#define CPL_Opt_MIPSOptimizer true

#define CPL_Opt_EnableAddrToReg true


#ifndef CPL_Gen_IR
	#define CPL_Gen_IR false
#endif
#ifndef CPL_Gen_MIPS
	#define CPL_Gen_MIPS true
#endif


#define CPL_Err_PlainText false
#define CPL_Err_ConsoleColor true


namespace IO {

using namespace std;


#if CPL_IO_UseStdin
	istream &in = cin;
#else
	ifstream in = ifstream(CPL_IO_InputFileName, ios::in);
#endif

#if CPL_IO_UseStdout
	ostream &out = cout;
#else
	ofstream out = ofstream(CPL_IO_OutputFileName, ios::out);
#endif

#if CPL_IO_UseStderr
	ostream &err = cerr;
#else
	ofstream err = ofstream(CPL_IO_ErrorFileName, ios::out);
#endif

}


#endif