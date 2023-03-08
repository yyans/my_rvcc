#include "rvcc.h"

// 在解析时，全部的变量实例都被累加到这个列表里。
Obj *Locals;

// program = "{" compoundStmt
// compoundStmt = (declaration | stmt)* "}"
// declaration =
//    declspec (declarator ("=" expr)? ("," declarator ("=" expr)?)*)? ";"
// declspec = "int"
// declarator = "*"* ident
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
// unary = ("+" | "-" | "*" | "&") unary | primary
// primary = "(" expr ")" | ident args? | num
// args = "(" ")"
static Node *compoundStmt(Token **Rest, Token *Tok);
static Node *declaration(Token **Rest, Token *Tok);
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

// 新建一个变量节点
static Node *newVarNode(Obj *Var, Token *Tok) {
	Node *Nd = newNode(ND_VAR, Tok);
	Nd->Var = Var;
	return Nd;
}

// 在链表中新增一个变量
static Obj *newLVar(char *Name, Type *Ty) {
	Obj *Var = calloc(1, sizeof(Obj));
	Var->Name = Name;
	Var->Ty = Ty; // 设置变量类型
	// 将变量插入头部
	Var->Next = Locals;
	Locals = Var;
	return Var;
}

// 获取标识符
static char *getIdent(Token * Tok) {
	if (Tok->kind != TK_IDENT) {
		errorTok(Tok, "expected an identifier");
	}
	return strndup(Tok->Loc, Tok->len);
}

// declspec = "int"
// declarator specifier
static Type *declspec(Token **Rest, Token *Tok) {
	*Rest = skip(Tok, "int");
	return TyInt;
}

// declarator = "*"* ident
// 构造一个Type类型
// 主要就是设置类型和变量名
static Type *declarator(Token **Rest, Token *Tok, Type *Ty) {
	// "*"*
	// 构建所有的（多重）指针
	while (consume(&Tok, Tok, "*")) {
		Ty = pointerTo(Ty);
	}

	if (Tok->kind != TK_IDENT) {
		errorAt(Tok, "expected a variable name");
	}

	// ident
	// 变量名
	Ty->Name = Tok;
	*Rest = Tok->Next;
	return Ty;
}

// 解析复合语句
// compoundStmt = stmt* "}"
static Node *compoundStmt(Token **Rest, Token *Tok) {
	// 放在最前面 避免Tok改变
	Node *Nd = newNode(ND_BLOCK, Tok);

	Node Head = {};
	Node *Cur = &Head;
	// (declaration | stmt)* "}"
	while (!equal(Tok, "}")) {
		// declaration
		if (equal(Tok, "int")) {
			Cur->Next = declaration(&Tok, Tok);
		} else {
			// stmt
			Cur->Next = stmt(&Tok, Tok);
		}
		Cur = Cur->Next;
		addType(Cur);
	}

	// Body储存了{}内解析的所有语句
	Nd->Body = Head.Next;
	*Rest = Tok->Next;
	return Nd;
}

// declaration =
//    declspec (declarator ("=" expr)? ("," declarator ("=" expr)?)*)? ";"
static Node *declaration(Token **Rest, Token *Tok) {
	// declspec
  	// 声明的 基础类型
	Type *Basety = declspec(&Tok, Tok);

	Node Head = {};
	Node *Cur = &Head;
	int I = 0;

	while (!equal(Tok, ";")) {
		// 第一个变量不匹配 ","
		if (I++ > 0) {
			Tok = skip(Tok, ",");
		}
		
		// declarator
    	// 声明获取到变量类型，包括变量名
		Type *Ty = declarator(&Tok, Tok, Basety);
		Obj *Var = newLVar(getIdent(Ty->Name), Ty);

		// 如果不存在"="后续可以不生成
		if (!equal(Tok, "=")) {
			continue;
		}
		
		// 构造变量节点
		Node *LHS = newVarNode(Var, Ty->Name);
		// 构造右边表达式
		Node *RHS = assign(&Tok, Tok->Next);
		Node *Node = newBinary(ND_ASSIGN, LHS, RHS, Tok);
		// 存放在表达式语句中
		Cur->Next = newUnary(ND_EXPR_STMT, Node, Tok);
		Cur = Cur->Next;
	}

	Node *Nd = newNode(ND_BLOCK, Tok);
	Nd->Body = Head.Next;
	*Rest = Tok->Next;
	return Nd;
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
	// equality
	Node *Nd = equality(&Tok, Tok);

	// 可能存在递归赋值 但是原程序没写 我就没写
	// ("=" assign)
	if (equal(Tok, "=")) {
		Nd = newBinary(ND_ASSIGN, Nd, assign(&Tok, Tok->Next), Tok);
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

static Node *newAdd(Node *LHS, Node *RHS, Token *Tok) {
	// 为左右节点添加类型
	addType(LHS);
	addType(RHS);

	// num + num 
	if (isInteger(LHS->Ty) && isInteger(RHS->Ty)) {
		return newBinary(ND_ADD, LHS, RHS, Tok);
	}
	// ptr  ptr 
	if (LHS->Ty->Base && RHS->Ty->Base) {
		errorTok(Tok, "invalid operands");
	}
	// num + ptr -> ptr + num
	if (!LHS->Ty->Base && RHS->Ty->Base) {
		Node *Temp = LHS;
		LHS = RHS;
		RHS = Temp;
	}
	// ptr + num
	// 指针加1*8
	RHS = newBinary(ND_MUL, RHS, newNum(8, Tok), Tok);
	return newBinary(ND_ADD, LHS, RHS, Tok);
}

static Node *newSub(Node *LHS, Node *RHS, Token *Tok) {
	// 为左右节点添加类型
	addType(LHS);
	addType(RHS);

	// num - num
	if (isInteger(LHS->Ty) && isInteger(RHS->Ty)) {
		return newBinary(ND_SUB, LHS, RHS, Tok);
	}

	// ptr - num
	if (LHS->Ty->Base && isInteger(RHS->Ty)) {
		RHS = newBinary(ND_MUL, RHS, newNum(8, Tok), Tok);
		addType(RHS); // 原项目加的 但我觉得加不加无所谓
		Node *Nd = newBinary(ND_SUB, LHS, RHS, Tok);
		// 节点类型为指针
		Nd->Ty = LHS->Ty;
		return Nd;
	}

	// ptr - ptr
	if (LHS->Ty->Base && RHS->Ty->Base) {
		Node *Nd = newBinary(ND_SUB, LHS, RHS, Tok);
		Nd->Ty = TyInt; 
		return newBinary(ND_DIV, Nd, newNode(8, Tok), Tok);
	}

	errorTok(Tok, "invalid operands");
	return NULL;
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
			Nd = newAdd(Nd, mul(&Tok, Tok->Next), Tok);
			continue;
		}
		
		// - mul
		if (equal(Tok, "-")) {
			Nd = newSub(Nd, mul(&Tok, Tok->Next), Tok);
			continue;
		}

		*Rest = Tok;
		return Nd;
	}
}

// 解析乘除
// mul = unary ("*" unary | "/" unary)*
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
// unary = ("+" | "-" | "*" | "&") unary | primary
static Node *unary(Token **Rest, Token *Tok) {
	// + unary
	if (equal(Tok, "+")) {
		return unary(Rest, Tok->Next);
	}

	// - unary
	if (equal(Tok, "-")) {
		return newUnary(ND_NEG, unary(Rest, Tok->Next), Tok);
	}

	// * unary
	if (equal(Tok, "*")) {
		return newUnary(ND_DEREF, unary(Rest, Tok->Next), Tok);
	}

	// & unary
	if (equal(Tok, "&")) {
		return newUnary(ND_ADDR, unary(Rest, Tok->Next), Tok);
	}

	return primary(Rest, Tok);
}

// 解析括号、数字
// primary = "(" expr ")" | ident args? | num
// args = "(" ")"
static Node *primary(Token **Rest, Token *Tok) {
	// "("  expr  ")"
	if (equal(Tok, "(")) {
		Node *Nd = expr(&Tok, Tok->Next);
		*Rest = skip(Tok, ")");
		return Nd;
	}

	// ident args?
	if (Tok->kind == TK_IDENT) {
		// 函数调用
		// args = "(" ")"
		if (equal(Tok->Next, "(")) {
			Node *Nd = newNode(ND_FUNCALL, Tok);
			// ident
			Nd->FuncName = strndup(Tok->Loc, Tok->len);
			*Rest = skip(Tok->Next->Next, ")");
			return Nd;
		}

		// indent
		// 查找一个变量
		Obj *Var = findVar(Tok);
		// 如果变量不存在 就在链表中新增一个变量
		if (!Var) {
			errorTok(Tok, "undefined variable");
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