#include <stdio.h>
#include <stdlib.h>


int main(int Argc, char **Argv) {
	if (Argc != 2) {
			// 异常处理
			fprintf(stderr, "%s: invalid number of args\n", Argv[0]);
			return 1;
	}

	// p指向输入算式的str
	char *P = Argv[1];

	// 声明一个全局main段，同时也是程序入口段
	printf("  .globl main\n");
	printf("main:\n");

	/// 以下我们将算式分解为 num (op num)(op num)..
	
	// str -- 要转换为长整数的字符串。
	// endptr -- 对类型为 char* 的对象的引用，其值由函数设置为 str 中数值后的下一个字符。
	// base -- 基数，必须介于 2 和 36（包含）之间，或者是特殊值 0。
	printf("  li a0, %ld\n", strtol(P, &P, 10));

	while (*P) {
		if (*P == '+') {
			++P; // 跳过op
			printf("  addi a0, a0, %ld\n", strtol(P, &P, 10));
			continue;
		}
		if (*P == '-') {
			++P;
			printf("  addi a0, a0, -%ld\n", strtol(P, &P, 10));
			continue;
		}
		// 遇到未解析的字符时
		fprintf(stderr, "unexepected character: %c\n", *P);
		return 1;
	}

	printf("  ret\n");

	return 0;
}