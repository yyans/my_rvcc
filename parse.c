#include "rvcc.h"

// expr = equality
// equality = relational ("==" relational | "!=" relational)*
// relational = add ("<" add | "<=" add | ">" add | ">=" add)*
// add = mul ("+" mul | "-" mul)*
// mul = unary ("*" unary | "/" unary)*
// unary = ("+" | "-") unary | primary
// primary = "(" expr ")" | num
static Node *expr(Token **Rest, Token *Tok);
static Node *equality(Token **Rest, Token *Tok);
static Node *relational(Token **Rest, Token *Tok);
static Node *add(Token **Rest, Token *Tok);
static Node *mul(Token **Rest, Token *Tok);
static Node *unary(Token **Rest, Token *Tok);
static Node *primary(Token **Rest, Token *Tok);

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

// 解析表达式
// expr = equality
static Node *expr(Token **Rest, Token *Tok) { return equality(Rest, Tok); }

// 解析相等性
// equality = relational ("==" relational | "!=" relational)*
static Node *equality(Token **Rest, Token *Tok) {
	// relational
	Node *Nd = relational(&Tok, Tok);

	// ("==" relational | "!=" relational)*
	while (true) {
		// == relational
		if (equal(Tok, "==")) {
			Nd = newBinary(ND_EQ, Nd, relational(&Tok, Tok->Next));
			continue;
		}

		// != relational
		if (equal(Tok, "!=")) {
			Nd = newBinary(ND_NE, Nd, relational(&Tok, Tok->Next));
			continue;
		}

		*Rest = Tok;
		return Nd;
	}
}

// 解析比较关系
// relational = add ("<" add | "<=" add | ">" add | ">=" add)*
static Node *relational(Token **Rest, Token *Tok) {
	// add
	Node *Nd = add(&Tok, Tok);

	// ("<" add | "<=" add | ">" add | ">=" add)*
	while (true) {
		// < add
		if (equal(Tok, "<")) {
			Nd = newBinary(ND_LT, Nd, add(&Tok, Tok->Next));
			continue;
		}

		// <= add
		if (equal(Tok, "<=")) {
			Nd = newBinary(ND_LE, Nd, add(&Tok, Tok->Next));
			continue;
		}

		// > add
		// x > y  等价于  y < x
		if (equal(Tok, ">")) {
			Nd = newBinary(ND_LT, add(&Tok, Tok->Next), Nd);
			continue;
		}

		// >= add
		// x >= y  等价于  y <= x
		if (equal(Tok, ">=")) {
			Nd = newBinary(ND_LE, add(&Tok, Tok->Next), Nd);
		}

		*Rest = Tok;
		return Nd;
	}
}

// 解析加减
// add = mul ("+" mul | "-" mul)*
static Node *add(Token **Rest, Token *Tok) {
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

// 语法解析入口函数
Node *parse(Token *Tok) {
    Node *Nd = expr(&Tok, Tok);
    if (Tok->kind != TK_EOF) {
		errorTok(Tok, "extra Token");
	}
    return Nd;
}