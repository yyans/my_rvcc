#include "rvcc.h"

int main(int Argc, char **Argv) {
	if (Argc != 2) {
		// 异常处理
		error("%s: invalid number of arguments", Argv[0]);
	}

	// 解析 Argv[1]
	Token *Tok = tokenize(Argv[1]);

	// 解析终结字符流 生成AST
	Function *Prog = parse(Tok);

	// 根据AST生成代码
	codegen(Prog);

	return 0;
}