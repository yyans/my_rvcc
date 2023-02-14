#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdarg.h>
#include <string.h>
#include <stdbool.h>

// 每个Token都有自己的Kind
typedef enum {
	TK_PUNCT, // + -
	TK_NUM, // num
	TK_EOF, // 文件终结符
} TokenKind;

typedef struct Token Token;
struct Token {
	TokenKind kind;
	Token *Next; // 指向下一个Token
	int val;
	char *Loc; // 在解析的字符串内的位置
	int len; // 长度
};

// 输入的字符串
static char *CurrentInput;

// 输出错误信息
// static 限制文件内使用
// Fmt:传入的字符串, ...为可变参数,表示Fmt后面的参数
static void error(char *Fmt, ...) {
	// 使用可变参数的条件
	va_list VA;
	// VA获取Fmt后面的参数
	va_start(VA, Fmt);

	// 这里修改为stdout 即实现了println
	vfprintf(stderr, Fmt, VA);
	fprintf(stderr, "\n");

	// 清楚VA
	va_end(VA);
	exit(1);
}

// 输出错误出现的位置
static void verrorAt(char *Loc, char *Fmt, va_list VA) {
	// 输出源信息
	fprintf(stderr, "%s\n", CurrentInput);
   
    // 输出错误信息
	// 计算出错的位置
	int Pos = Loc - CurrentInput;
	// 填充Pos个空格
	fprintf(stderr, "%*s", Pos, "");
	fprintf(stderr, "^ ");
	vfprintf(stderr, Fmt, VA);
	fprintf(stderr, "\n");
  	va_end(VA);
}

// 字符串解析时出错
static void errorAt(char *Loc, char *Fmt, ...) {
	va_list VA;
	va_start(VA, Fmt);
	verrorAt(Loc, Fmt, VA);
	exit(1);
}

// Token解析时出错
static void errorTok(Token *token, char *Fmt, ...) {
	va_list VA;
	va_start(VA, Fmt);
	verrorAt(token->Loc, Fmt, VA);
	exit(1);
}

// 判断Tok的值是否等于指定值，没有用char，是为了后续拓展
static bool equal(Token *token, char *Str) {
	// 比较字符串LHS（左部），RHS（右部）的前N位，S2的长度应大于等于N.
	// 比较按照字典序，LHS<RHS回负值，LHS=RHS返回0，LHS>RHS返回正值
	// 同时确保，此处的Op位数=N
	return memcmp(token->Loc, Str, token->len) == 0 && Str[token->len] == '\0';
}
// 跳过指定的Str
static Token *skip(Token *token, char *Str) {
	if (!equal(token, Str)) {
		errorTok(token, "expect '%s'", Str);
	}
	return token->Next;
}

// 返回TK_NUM的值
static int getNumber(Token *token) {
	if (token->kind != TK_NUM) {
		errorTok(token, "expect a number");
	}
	return token->val;
}

// 生成一个Token
// 这个函数设计的还挺秒！！！
static Token *newToken(TokenKind Kind, char *Start, char *End) {
	// 这里不free
	// 1. 编译的生命周期不长
	// 2. 减少复杂度
	Token *token = calloc(1, sizeof(Token));
	token->kind = Kind;
	token->Loc = Start;
	token->len = End - Start;
	return token;
}

static Token *tokenize() {
	char *P = CurrentInput;
	Token Head = {};
	Token *Cur = &Head;

	while (*P) {
		// 跳过所有空白 回车等
		if (isspace(*P)) {
			++P;
			continue;
		}
		// 解析数字
		if (isdigit(*P)) {
			Cur->Next = newToken(TK_NUM, P, P);
			Cur = Cur->Next;

			const char *OldPtr = P;
			Cur->val = strtoul(P, &P, 10);
			Cur->len = P - OldPtr;

			continue;
		}

		// 解析操作符
		if (*P == '+' || *P == '-') {
			Cur->Next = newToken(TK_PUNCT, P, P + 1);
			Cur = Cur->Next;
			++P;
			continue;
		}

		errorAt(P, "invalid token");
	} 

	Cur->Next = newToken(TK_EOF, P, P);

	return Head.Next;
}

int main(int Argc, char **Argv) {
	if (Argc != 2) {
		// 异常处理
		error("%s: invalid number of arguments", Argv[0]);
	}

	// CurrentInput指向输入算式的str
	CurrentInput = Argv[1];
	// 解析 Argv[1]
	Token *token = tokenize();

	// 声明一个全局main段，同时也是程序入口段
	printf("  .globl main\n");
	printf("main:\n");

	/// 以下我们将算式分解为 num (op num)(op num)..
	
	// str -- 要转换为长整数的字符串。
	// endptr -- 对类型为 char* 的对象的引用，其值由函数设置为 str 中数值后的下一个字符。
	// base -- 基数，必须介于 2 和 36（包含）之间，或者是特殊值 0。
	printf("  li a0, %d\n", getNumber(token));
	token = token->Next;

	while (token->kind != TK_EOF) {
		if (equal(token, "+")) {
			token = token->Next;
			printf("  addi a0, a0, %d\n", getNumber(token));
			token = token->Next;
			continue;
		}
		// 不是+ 则是-
		token = skip(token, "-");
		printf("  addi a0, a0, -%d\n", getNumber(token));
		token = token->Next;
	}

	// ret为jalr x0, x1, 0别名指令，用于返回子程序
  	// 返回的为a0的值
	printf("  ret\n");

	return 0;
}