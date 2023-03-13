// 使用POSIX.1标准
// 使用了strndup函数
#define _POSIX_C_SOURCE 200809L


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
	TK_IDENT, // 变量标识符、函数名等
	TK_PUNCT, // + -
	TK_KEYWORD, // 关键字
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
bool consume(Token **Rest, Token *Tok, char *Str);
// 词法分析
Token *tokenize(char *Input);


//
// 生成AST（抽象语法树），语法解析
//

typedef struct Type Type;
typedef struct Node Node;

// 本地变量
typedef struct Obj Obj;
struct Obj {
	Obj *Next; // 指向下一个对象
	char *Name; // 变量名
	Type *Ty; // 变量类型
	int offset; // 变量在栈中的偏移量
};

// 函数
typedef struct Function Function;
struct Function {
	Function *Next; // 下一个函数
	char *Name; // 函数名
	Obj *Params; // 形参

	Node *Body; // 函数体
	Obj *Locals; // 本地变量
	int StackSize; // 存储变量的栈大小
};

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
	ND_ASSIGN, // 赋值
	ND_DEREF, // 解引用*
	ND_ADDR,   // 取地址&
	ND_RETURN, // 返回
	ND_IF,     // if语句
	ND_FOR,    // for语句
	ND_BLOCK, // {...} 代码块
	ND_FUNCALL, // 函数调用
	ND_EXPR_STMT, // 表达式语句
	ND_VAR, // 变量类型
	ND_NUM, // 整形
} NodeKind;

struct Node {
	NodeKind Kind; // 节点类型
	Node *Next; // 下一个表达式
	Token *Tok; // 节点对应的终结符
	Type *Ty;   // 节点类型

	Node *LHS; // 左节点
	Node *RHS; // 右节点

	// if语句 | for语句 | while语句
	Node *Cond; // 判断语句
	Node *Then; // 符合条件的语句块
	Node *Els; // 不符合条件的语句块
	Node *Init; // 初始状态语句
	Node *Inc; // 自增语句

	// 代码块
	Node *Body;

	// 函数调用
	char *FuncName; // 函数名
	Node *Args;     // 函数参数

	Obj *Var; // 存储ND_VAR种类的变量
	int Val; // 储存ND_NUM种类的值
};

// 语法分析入口函数
Function *parse(Token *Tok);

/**
 * 类型系统
*/

typedef enum {
	TY_INT, // int类型
	TY_PTR, // 指针
	TY_FUNC, // 函数
	TY_ARRAY, // 数组
} TypeKind;

struct Type {
	TypeKind Kind; // 种类
	int Size;    // 大小 , sizeof的返回值

	// 指针
	Type *Base; // 指向的基类

	// 类型对应名称  如变量名 函数名
	Token *Name;

	// 数组
	int ArrayLen;   // 数组长度，元素个数

	// 函数类型
	Type *ReturnTy; // 函数返回类型
	Type *Params;   // 形参
	Type *Next;     // 下一个参数
};

// 声明一个全局变量，定义在type.c中。
extern Type *TyInt;

// 判断是否为整型
bool isInteger(Type *TY);
// 为节点内的所有节点添加类型
void addType(Node *Nd);
// 指针类型   指向基类Base
Type *pointerTo(Type *Base);
// 函数类型
Type *funcType(Type *ReturnTy);
// 复制类型
Type *copyType(Type *Ty);
// 数组类型
Type *arrayOf(Type *Base, int Len);

//
// 语义分析与代码生成
//

// 代码生成入口函数
void codegen(Function *Prog);