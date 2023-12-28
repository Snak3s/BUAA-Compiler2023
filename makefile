.PHONY: clean mips ir

mips:
	clang++ src/main.cpp -o main -Wall -O2 -DCPL_Gen_IR=false -DCPL_Gen_MIPS=true -DCPL_IO_UseStdin=true -DCPL_IO_UseStdout=true -DCPL_IO_UseStderr=true

ir:
	clang++ src/main.cpp -o main -Wall -O2 -DCPL_Gen_IR=true -DCPL_Gen_MIPS=false -DCPL_IO_UseStdin=true -DCPL_IO_UseStdout=true -DCPL_IO_UseStderr=true

clean:
	rm main