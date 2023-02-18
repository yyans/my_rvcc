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

void codegen(Node *Nd) {
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
}