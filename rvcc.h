#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdarg.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>


//
// 共用头文件，定义了多个文件间共同使用的函数和数据
//


//
// 终结符分析，词法分析
//

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

// 去除了static用以在多个文件间访问
// 报错函数
void error(char *Fmt, ...);
void errorAt(char *Loc, char *Fmt, ...);
void errorTok(Token *Tok, char *Fmt, ...);
// 判断Token与Str的关系
bool equal(Token *Tok, char *Str);
Token *skip(Token *Tok, char *Str);
// 词法分析
Token *tokenize(char *Input);


//
// 生成AST（抽象语法树），语法解析
//

// AST的节点种类
typedef enum {
	ND_ADD,
	ND_SUB,
	ND_MUL,
	ND_DIV,
	ND_NEG, // 负号
	ND_EQ,
	ND_NE,
	ND_LT,
	ND_LE,
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

// 语法分析入口函数
Node *parse(Token *Tok);

//
// 语义分析与代码生成
//

// 代码生成入口函数
void codegen(Node *Nd);