#include "rvcc.h"

// 输入的字符串
static char *CurrentInput;

// 输出错误信息
// Fmt:传入的字符串, ...为可变参数,表示Fmt后面的参数
void error(char *Fmt, ...) {
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
void errorAt(char *Loc, char *Fmt, ...) {
	va_list VA;
	va_start(VA, Fmt);
	verrorAt(Loc, Fmt, VA);
	exit(1);
}

// Token解析时出错
void errorTok(Token *token, char *Fmt, ...) {
	va_list VA;
	va_start(VA, Fmt);
	verrorAt(token->Loc, Fmt, VA);
	exit(1);
}

// 判断Tok的值是否等于指定值，没有用char，是为了后续拓展
bool equal(Token *token, char *Str) {
	// 比较字符串LHS（左部），RHS（右部）的前N位，S2的长度应大于等于N.
	// 比较按照字典序，LHS<RHS回负值，LHS=RHS返回0，LHS>RHS返回正值
	// 同时确保，此处的Op位数=N
	return memcmp(token->Loc, Str, token->len) == 0 && Str[token->len] == '\0';
}

// 跳过指定的Str
Token *skip(Token *token, char *Str) {
	if (!equal(token, Str)) {
		errorTok(token, "expect '%s'", Str);
	}
	return token->Next;
}

// 消耗掉指定Token
bool consume(Token **Rest, Token *Tok, char *Str) {
	// 存在
	if (equal(Tok, Str)) {
		*Rest = Tok->Next;
		return true;
	}
	// 不存在
	*Rest = Tok;
	return false;
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

// 判断Str是否以SubStr开头
static bool startsWith(char *Str, char *SubStr) {
	// 判断LHS RHS的N字符是否相等
	return strncmp(Str, SubStr, strlen(SubStr)) == 0;
}

// 读操作符
static int readPunct(char *Ptr) {
	if (startsWith(Ptr, "==") || startsWith(Ptr, "!=") || startsWith(Ptr, "<=") ||
	startsWith(Ptr, ">=")) {
		return 2;
	}

	// 判断1字节的操作符
	return ispunct(*Ptr) ? 1 : 0;
}

// 判断标记符的首字母规则
// [a-zA-Z_]
static bool isIdent1(char C) {
  // a-z与A-Z在ASCII中不相连，所以需要分别判断
  return ('a' <= C && C <= 'z') || ('A' <= C && C <= 'Z') || C == '_';
}

static bool isIdent2(char C) {
	return isIdent1(C) || ('0' <= C && C <= '9');
}

static bool isKeyword(Token *Tok) {
	// 关键字
	static char *Kw[] = {"return", "if", "else", "for", "while"};

	// 遍历关键字列表
	// 指针数组（存放的指针个数）除以指针大小
	for (int I = 0; I < sizeof(Kw) / sizeof(*Kw); I++) {
		if (equal(Tok, Kw[I])) {
			return true;
		}
	}
	return false;
}

// 将关键字终结符转化为对应类型
static void convertKeywords(Token *Tok) {
	for (Token *T = Tok; T->kind != TK_EOF; T=T->Next) {
		if (isKeyword(Tok)) {
			T->kind = TK_KEYWORD;
		}
	}
}

// 生成终结符流
Token *tokenize(char *P) {
	CurrentInput = P;
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

		// 解析标记符
    	// [a-zA-Z_][a-zA-Z0-9_]*
		if (isIdent1(*P)) {
			char *Start = P;
			do {
				++P;
			} while(isIdent2(*P));
			Cur->Next = newToken(TK_IDENT, Start, P);
			Cur = Cur->Next;
			continue;
		}

		// 解析操作符
		int PunctLen = readPunct(P);
		if (PunctLen) {
			Cur->Next = newToken(TK_PUNCT, P, P + PunctLen);
			Cur = Cur->Next;
			P += PunctLen;
			continue;
		}

		errorAt(P, "invalid token");
	} 
	// 解析结束 增加一个EOF 表示结束
	Cur->Next = newToken(TK_EOF, P, P);
	// 处理关键字终结符
	convertKeywords(Head.Next);
	// Head 无内容
	return Head.Next;
}