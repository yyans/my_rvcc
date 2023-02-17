#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdarg.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>

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
		if (ispunct) {
			Cur->Next = newToken(TK_PUNCT, P, P + 1);
			Cur = Cur->Next;
			++P;
			continue;
		}

		errorAt(P, "invalid token");
	} 
	// 解析结束 增加一个EOF 表示结束
	Cur->Next = newToken(TK_EOF, P, P);
	// Head 无内容
	return Head.Next;
}

/**
 * 接下来语法分析
 * 生成AST(抽象语法树) !!!
*/

// AST的节点种类
typedef enum {
	ND_ADD,
	ND_SUB,
	ND_MUL,
	ND_DIV,
	ND_NEG, // 负号
	ND_NUM, // 整形
} NodeKind;

// AST中二叉树节点
typedef struct Node Node;
struct Node {
	NodeKind Kind; // 节点类型
	Node *LHS; // 左节点
	Node *RHS; // 右节点
	int Val; // 储存ND_NUM种类的值
};

// 新建一个节点
static Node *newNode(NodeKind kind) {
	Node *Nd = calloc(1, sizeof(Node));
	Nd->Kind = kind;
	return Nd;
}

// 新建一个单叉树(节点)
static Node *newUnary(NodeKind kind, Node *Expr) {
	Node *Nd = newNode(kind);
	Nd->LHS = Expr;
	return Nd;
}

// 新建一个二叉树节点
static Node *newBinary(NodeKind kind, Node *LHS, Node *RHS) {
	Node *Nd = newNode(kind);
	Nd->LHS = LHS;
	Nd->RHS = RHS;
	return Nd;
}

// 新建一个数字节点
static Node *newNum(int Val) {
	Node *Nd = newNode(ND_NUM);
	Nd->Val = Val;
	return Nd;
}

// expr = mul ("+" mul | "-" mul)*
// mul = unary ("*" unary | "/" unary)*
// unary = ("+" | "-") unary | primary
// primary = "(" expr ")" | num
static Node *expr(Token **Rest, Token *Tok);
static Node *mul(Token **Rest, Token *Tok);
static Node *unary(Token **Rest, Token *Tok);
static Node *primary(Token **Rest, Token *Tok);

// 解析加减
static Node *expr(Token **Rest, Token *Tok) {
	// mul
	Node *Nd = mul(&Tok, Tok);

	while (true) {
		// + mul
		if (equal(Tok, "+")) {
			Nd = newBinary(ND_ADD, Nd, mul(&Tok, Tok->Next));
			continue;
		}

		// - mul
		if (equal(Tok, "-")) {
			Nd = newBinary(ND_SUB, Nd, mul(&Tok, Tok->Next));
			continue;
		}

		*Rest = Tok;
		return Nd;
	}
}

// 解析乘除
static Node *mul(Token **Rest, Token *Tok) {
	// primary
	Node *Nd = unary(&Tok, Tok);

	while (true) {
		// * primary
		if (equal(Tok, "*")) {
			Nd = newBinary(ND_MUL, Nd, unary(&Tok, Tok->Next));
			continue;
		}

		// / primary
		if (equal(Tok, "/")) {
			Nd = newBinary(ND_DIV, Nd, unary(&Tok, Tok->Next));
			continue;
		}

		*Rest = Tok;
		return Nd;
	}
}

// 解析 正负号
static Node *unary(Token **Rest, Token *Tok) {
	// + unary
	if (equal(Tok, "+")) {
		return unary(Rest, Tok->Next);
	}

	// - unary
	if (equal(Tok, "-")) {
		return newUnary(ND_NEG, unary(Rest, Tok->Next));
	}

	return primary(Rest, Tok);
}

// 解析括号、数字
static Node *primary(Token **Rest, Token *Tok) {
	// "("  expr  ")"
	if (equal(Tok, "(")) {
		Node *Nd = expr(&Tok, Tok->Next);
		*Rest = skip(Tok, ")");
		return Nd;
	}

	if (Tok->kind == TK_NUM) {
		Node *Nd = newNum(Tok->val);
		*Rest = Tok->Next;
		return Nd;
	}

	errorTok(Tok, "expected expression");
	return NULL;
}

/**
 * 接下来是语义分析部分
 * 可以理解为后端部分
*/

// 记录栈的深度
static int Depth;

// 压栈
static void push() {
	printf("  addi sp, sp, -8\n");
	printf("  sd a0, 0(sp)\n");
	Depth++;
}

// 弹栈
static void pop(char *Reg) {
	printf("  ld %s, 0(sp)\n", Reg);
	printf("  addi sp, sp, 8\n");
	Depth--;
}

// 生成表达式
static void genExpr(Node *Nd) {
	switch (Nd->Kind) {
	// 加载数字到a0
	case ND_NUM:
		printf("  li a0, %d\n", Nd->Val);
		return ;
	// 对寄存器取反
	case ND_NEG:
		genExpr(Nd->LHS);
		printf("  neg a0, a0\n");
		return ;
	default:
		break;
	}

	// 递归到最右边
	genExpr(Nd->RHS);
	// 压栈
	push();
	// 递归到左边
	genExpr(Nd->LHS);
	// 弹栈
	pop("a1");

	// 根据各二叉树节点生成汇编
	switch (Nd->Kind) {
		case ND_ADD:
			printf("add a0, a0, a1\n");
			return ;
		case ND_SUB:
			printf("sub a0, a0, a1\n");
			return ;
		case ND_MUL:
			printf("mul a0, a0, a1\n");
			return ;
		case ND_DIV:
			printf("div a0, a0, a1\n");
			return ;
		default:
			break;
	}

	error("invalid expression");
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

	// 解析终结字符流 生成AST
	Node *Nd = expr(&token, token);

	if (token->kind != TK_EOF) {
		errorTok(token, "extra Token");
	}

	// 声明一个全局main段，同时也是程序入口段
	printf("  .globl main\n");
	printf("main:\n");

	// 遍历AST树生成汇编
	genExpr(Nd);

	// ret为jalr x0, x1, 0别名指令，用于返回子程序
  	// 返回的为a0的值
	printf("  ret\n");

	// 如果栈未清空则报错
  	assert(Depth == 0);

	return 0;
}