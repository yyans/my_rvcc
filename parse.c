#include "rvcc.h"

// 在解析时，全部的变量实例都被累加到这个列表里。
Obj *Locals;

// program = "{" compoundStmt
// compoundStmt = stmt* "}"
// stmt = "return" expr ";"
//        | "if" "(" expr ")" stmt ("else" stmt)?
//        | "for" "(" exprStmt expr? ";" expr? ")" stmt
//        | "while" "(" expr ")" stmt
//        | "{" compoundStmt
//        | exprStmt
// exprStmt = expr? ";"
// expr = assign
// assign = equality ("=" assign)
// equality = relational ("==" relational | "!=" relational)*
// relational = add ("<" add | "<=" add | ">" add | ">=" add)*
// add = mul ("+" mul | "-" mul)*
// mul = unary ("*" unary | "/" unary)*
// unary = ("+" | "-") unary | primary
// primary = "(" expr ")" | ident｜ num
static Node *compoundStmt(Token **Rest, Token *Tok);
static Node *stmt(Token **Rest, Token *Tok);
static Node *exprStmt(Token **Rest, Token *Tok);
static Node *expr(Token **Rest, Token *Tok);
static Node *assign(Token **Rest, Token *Tok);
static Node *equality(Token **Rest, Token *Tok);
static Node *relational(Token **Rest, Token *Tok);
static Node *add(Token **Rest, Token *Tok);
static Node *mul(Token **Rest, Token *Tok);
static Node *unary(Token **Rest, Token *Tok);
static Node *primary(Token **Rest, Token *Tok);

// 通过名称，查找一个本地变量
static Obj *findVar(Token *Tok) {
	for (Obj *Var = Locals; Var; Var = Var->Next) {
		// 查找Locals变量中是否存在同名变量
		if (strlen(Var->Name) == Tok->len && 
			!strncmp(Var->Name, Tok->Loc, Tok->len)) {
				return Var;
			}
	}
	return NULL;
}

// 新建一个节点
static Node *newNode(NodeKind kind, Token *Tok) {
	Node *Nd = calloc(1, sizeof(Node));
	Nd->Tok = Tok;
	Nd->Kind = kind;
	return Nd;
}

// 新建一个单叉树(节点)
static Node *newUnary(NodeKind kind, Node *Expr, Token* Tok) {
	Node *Nd = newNode(kind, Tok);
	Nd->LHS = Expr;
	return Nd;
}

// 新建一个二叉树节点
static Node *newBinary(NodeKind kind, Node *LHS, Node *RHS, Token *Tok) {
	Node *Nd = newNode(kind, Tok);
	Nd->LHS = LHS;
	Nd->RHS = RHS;
	return Nd;
}

// 新建一个数字节点
static Node *newNum(int Val, Token *Tok) {
	Node *Nd = newNode(ND_NUM, Tok);
	Nd->Val = Val;
	return Nd;
}

// 新建一个变量
static Node *newVarNode(Obj *Var, Token *Tok) {
	Node *Nd = newNode(ND_VAR, Tok);
	Nd->Var = Var;
	return Nd;
}

// 在链表中新增一个变量
static Obj *newLVar(char *Name) {
	Obj *Var = calloc(1, sizeof(Obj));
	Var->Name = Name;
	// 将变量插入头部
	Var->Next = Locals;
	Locals = Var;
	return Var;
}

// 解析语句
// stmt = "return" expr ";"
//        | "if" "(" expr ")" stmt ("else" stmt)?
//        | "for" "(" exprStmt expr? ";" expr? ")" stmt
//        | "while" "(" expr ")" stmt
//        | "{" compoundStmt
//        | exprStmt
static Node *stmt(Token **Rest, Token *Tok) { 
	// return expr ;
	if (equal(Tok, "return")) {
		Token *Start = Tok;
		Node *Nd = newUnary(ND_RETURN, expr(&Tok, Tok->Next), Start);
		*Rest = skip(Tok, ";");
		return Nd;
	}

	// "if" "(" expr ")" stmt ("else" stmt)?
	if (equal(Tok, "if")) {
		Node *Nd = newNode(ND_IF, Tok);
		// "(" expr ")"
		Tok = skip(Tok->Next, "(");
		Nd->Cond = expr(&Tok, Tok);
		Tok = skip(Tok, ")");
		// stmt
		Nd->Then = stmt(&Tok, Tok);
		// ("else" stmt)?
		if (equal(Tok, "else")) {
			Nd->Els = stmt(&Tok, Tok->Next);
		}
		*Rest = Tok;
		return Nd;
	}

	// "for" "(" exprStmt expr? ";" expr? ")" stmt
	if (equal(Tok, "for")) {
		// 建立一个for语句节点
		Node *Nd = newNode(ND_FOR, Tok);
		// "("
		Tok = skip(Tok->Next, "(");
		// exprStmt
		Nd->Init = exprStmt(&Tok, Tok);
		// expr? ";"
		if (!equal(Tok, ";")) {
			Nd->Cond = expr(&Tok, Tok);
		}
		Tok = skip(Tok, ";");
		// expr? ")"
		if (!equal(Tok, ")")) {
			Nd->Inc = expr(&Tok, Tok);
		}
		Tok = skip(Tok, ")");
		// stmt
		Nd->Then = stmt(&Tok, Tok);
		*Rest = Tok;
		return Nd;
	}

	// "while" "(" expr ")" stmt
	if (equal(Tok, "while")) {
		Node *Nd = newNode(ND_FOR, Tok);
		// "("
		Tok = skip(Tok->Next, "(");
		// expr
		Nd->Cond = expr(&Tok, Tok);
		// ")"
		Tok = skip(Tok, ")");
		// stmt
		Nd->Then = stmt(&Tok, Tok);
		*Rest = Tok;
		return Nd;
	}

	// "{" compoundStmt
	if (equal(Tok, "{")) {
		return compoundStmt(Rest, Tok->Next);
	}

	// exprStmt
	return exprStmt(Rest, Tok); 
}

// 解析复合语句
// compoundStmt = stmt* "}"
static Node *compoundStmt(Token **Rest, Token *Tok) {
	// 放在最前面 避免Tok改变
	Node *Nd = newNode(ND_BLOCK, Tok);

	Node Head = {};
	Node *Cur = &Head;
	// stmt* "}"
	while (!equal(Tok, "}")) {
		Cur->Next = stmt(&Tok, Tok);
		Cur = Cur->Next;
	}

	// Body储存了{}内解析的所有语句
	Nd->Body = Head.Next;
	*Rest = Tok->Next;
	return Nd;
}

// 解析表达式语句
// exprStmt = expr? ";"
static Node *exprStmt(Token **Rest, Token *Tok) {
	Token *Start = Tok;

	if (equal(Tok, ";")) {
		*Rest = Tok->Next;
		return newNode(ND_BLOCK, Start);
	}

	Node *Nd = newUnary(ND_EXPR_STMT, expr(&Tok, Tok), Start);
	*Rest = skip(Tok, ";");
	return Nd;
}

// 解析表达式
// expr = equality
static Node *expr(Token **Rest, Token *Tok) { return assign(Rest, Tok); }

// 解析赋值
// assign = equality ("=" assign)
static Node *assign(Token **Rest, Token *Tok) {
	Token *Start = Tok;
	// equality
	Node *Nd = equality(&Tok, Tok);

	// 可能存在递归赋值 但是原程序没写 我就没写
	// ("=" assign)
	if (equal(Tok, "=")) {
		Nd = newBinary(ND_ASSIGN, Nd, assign(&Tok, Tok->Next), Start);
	}
	*Rest = Tok;
	return Nd;
}

// 解析相等性
// equality = relational ("==" relational | "!=" relational)*
static Node *equality(Token **Rest, Token *Tok) {
	// relational
	Node *Nd = relational(&Tok, Tok);

	// ("==" relational | "!=" relational)*
	while (true) {
		Token *Start = Tok;
		// == relational
		if (equal(Tok, "==")) {
			Nd = newBinary(ND_EQ, Nd, relational(&Tok, Tok->Next), Start);
			continue;
		}

		// != relational
		if (equal(Tok, "!=")) {
			Nd = newBinary(ND_NE, Nd, relational(&Tok, Tok->Next), Start);
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
		Token *Start = Tok;
		// < add
		if (equal(Tok, "<")) {
			Nd = newBinary(ND_LT, Nd, add(&Tok, Tok->Next), Start);
			continue;
		}

		// <= add
		if (equal(Tok, "<=")) {
			Nd = newBinary(ND_LE, Nd, add(&Tok, Tok->Next), Start);
			continue;
		}

		// > add
		// x > y  等价于  y < x
		if (equal(Tok, ">")) {
			Nd = newBinary(ND_LT, add(&Tok, Tok->Next), Nd, Start);
			continue;
		}

		// >= add
		// x >= y  等价于  y <= x
		if (equal(Tok, ">=")) {
			Nd = newBinary(ND_LE, add(&Tok, Tok->Next), Nd, Start);
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
		Token *Start = Tok;
		// + mul
		if (equal(Tok, "+")) {
			Nd = newBinary(ND_ADD, Nd, mul(&Tok, Tok->Next), Tok);
			continue;
		}
		
		// - mul
		if (equal(Tok, "-")) {
			Nd = newBinary(ND_SUB, Nd, mul(&Tok, Tok->Next), Tok);
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
		Token *Start = Tok;
		// * primary
		if (equal(Tok, "*")) {
			Nd = newBinary(ND_MUL, Nd, unary(&Tok, Tok->Next), Tok);
			continue;
		}

		// / primary
		if (equal(Tok, "/")) {
			Nd = newBinary(ND_DIV, Nd, unary(&Tok, Tok->Next), Tok);
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
		return newUnary(ND_NEG, unary(Rest, Tok->Next), Tok->Next);
	}

	return primary(Rest, Tok);
}

// 解析括号、数字
// primary = "(" expr ")" | ident｜ num
static Node *primary(Token **Rest, Token *Tok) {
	// "("  expr  ")"
	if (equal(Tok, "(")) {
		Node *Nd = expr(&Tok, Tok->Next);
		*Rest = skip(Tok, ")");
		return Nd;
	}

	// ident
	if (Tok->kind == TK_IDENT) {
		// 查找一个变量
		Obj *Var = findVar(Tok);
		// 如果变量不存在 就在链表中新增一个变量
		if (!Var) {
			// strndup 复制n个字符
			Var = newLVar(strndup(Tok->Loc, Tok->len));
		}
		*Rest = Tok->Next;
		return newVarNode(Var, Tok);
	}

	// num
	if (Tok->kind == TK_NUM) {
		Node *Nd = newNum(Tok->val, Tok);
		*Rest = Tok->Next;
		return Nd;
	}

	errorTok(Tok, "expected expression");
	return NULL;
}

// 语法解析入口函数
// program = "{" compoundStmt
Function *parse(Token *Tok) {
	// "{}"
	Tok = skip(Tok, "{");
    
	// 函数体存储语句的AST，Locals存储变量
	Function *Prog = calloc(1, sizeof(Function));
	Prog->Body = compoundStmt(&Tok, Tok);
	Prog->Locals = Locals;
	return Prog;
}