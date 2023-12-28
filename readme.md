### About

这是一个由我编写的简单的 SysY 语言到 LLVM IR / MIPS 的编译器，SysY 语言在某种意义上是 C 语言的子集，但似乎在某些规定上有所不同。

### Build

`make` 将会生成目标语言为 MIPS 的可执行程序 `main`。

`make ir` 将会生成目标语言为 LLVM IR 的可执行程序 `main`。

`make clean` 将会清空已生成的 `main`。

### Run

可执行程序 `main` 将会从 `stdin` 中读取 SysY 源代码，并将目标代码输出至 `stdout`，将错误信息输出至 `stderr`。

可以使用类似 `./main < code.c > code.asm 2> error` 的重定向调用 `main`，当然也可以修改配置文件中的设置。

LLVM IR 代码运行需要 LLVM 组件，MIPS 代码运行需要 MIPS 模拟器。