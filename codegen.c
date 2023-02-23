#include "rvcc.h"

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

// 对齐到Align的整数倍
static int alignTo(int N, int Align) {
  // (0,Align]返回Align
  return (N + Align - 1) / Align * Align;
}

// 计算给定节点的绝对地址
static void genAddr(Node *Nd) {
	if (Nd->Kind == ND_VAR) {
		// 相对于fp而言
		printf("  addi a0, fp, %d\n", Nd->Var->offset);
		return;
	}
	error("not an lvalue");
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
	case ND_VAR:
		// 计算出变量的地址，然后存入a0
		genAddr(Nd);
		// 访问a0地址中存储的数据，存入到a0当中
		printf("  ld a0, 0(a0)\n");
		return ;
	case ND_ASSIGN:
		// 加载地址到a0
		genAddr(Nd->LHS);
		// 推入栈中
		push();
		genExpr(Nd->RHS);
		pop("a1");
		// 将右部的值加载到a0地址处
		printf("  sd a0, 0(a1)\n");
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
			printf("  add a0, a0, a1\n");
			return ;
		case ND_SUB:
			printf("  sub a0, a0, a1\n");
			return ;
		case ND_MUL:
			printf("  mul a0, a0, a1\n");
			return ;
		case ND_DIV:
			printf("  div a0, a0, a1\n");
			return ;
		case ND_EQ:
		case ND_NE:
			// a0=a0^a1，异或指令
    		printf("  xor a0, a0, a1\n");
			if (Nd->Kind == ND_EQ) {
				// a0 == a1
				// a0 = a0^a1, sltiu a0, a0, 1
				// 等于0 a0置1
				printf("  seqz a0, a0\n");
			} else {
				// a0!=a1
				// a0=a0^a1, sltu a0, x0, a0
				// 不等于0则置1
				printf("  snez a0, a0\n");
			}
			return ;
		case ND_LT:
			printf("  slt a0, a0, a1\n");
    		return;
		case ND_LE:
			// a0<=a1等价于
    		// a0=a1<a0, a0=a1^1
			printf("  slt a0, a1, a0\n");
			printf("  xori a0, a0, 1\n");
			return;
		default:
			break;
	}

	error("invalid expression");
}

// 生成语句
static void genStmt(Node *Nd) {
	if (Nd->Kind == ND_EXPR_STMT) {
		genExpr(Nd->LHS);
		return ;
	}
	error("invalid expression");
}

// 根据变量的链表计算出偏移量
static void assignLVarOffsets(Function *Prog) {
	int Offset = 0;
	// 读取所有变量
	for (Obj *Var = Prog->Locals; Var; Var = Var->Next) {
		// 每个变量分配8字节
		Offset += 8;
		// 为每个变量赋一个偏移量，或者说是栈中地址
		Var->offset = -Offset;
	}
	// 将栈对齐到16字节
	Prog->StackSize = alignTo(Offset, 16);
}

void codegen(Function *Prog) {
	assignLVarOffsets(Prog);
    // 声明一个全局main段，同时也是程序入口段
	printf("  .globl main\n");
	printf("main:\n");

	// 栈布局
	//-------------------------------// sp
	//              fp
	//-------------------------------// fp = sp-8
	//             变量
	//-------------------------------// sp = sp-8-StackSize
	//           表达式计算
	//-------------------------------//

	// Prologue, 前言
	// 将fp压入栈中，保存fp的值
	printf("  addi sp, sp, -8\n");
	printf("  sd fp, 0(sp)\n");
	// 将sp写入fp
	printf("  mv fp, sp\n");

	// 偏移量为实际变量所用的栈大小
  	printf("  addi sp, sp, -%d\n", Prog->StackSize);

	// 遍历所有语句节点
	for (Node *N = Prog->Body; N; N = N->Next) {
		genStmt(N);
		assert(Depth == 0);
	}

	// Epilogue，后语
	// 将fp的值改写回sp
	printf("  mv sp, fp\n");
	// 将最早fp保存的值弹栈，恢复fp。
	printf("  ld fp, 0(sp)\n");
	printf("  addi sp, sp, 8\n");
	// 返回
	printf("  ret\n");
}