#include "rvcc.h"

// 记录栈的深度
static int Depth;

// if语句嵌入深度计数
static int count(void) {
  static int I = 1;
  return I++;
}

// 压栈
static void push() {
	printf("  # 将a0的值压栈\n");
	printf("  addi sp, sp, -8\n");
	printf("  sd a0, 0(sp)\n");
	Depth++;
}

// 弹栈
static void pop(char *Reg) {
	printf("  # 弹栈， 将栈顶的值压入%s\n", Reg);
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
	if (Nd->Var) {
		printf("  # 获取变量%s的偏移地址%d并存入a0\n", Nd->Var->Name, Nd->Var->offset);
	}
	if (Nd->Kind == ND_VAR) {
		// 相对于fp而言
		printf("  addi a0, fp, %d\n", Nd->Var->offset);
		return;
	}
	errorTok(Nd->Tok, "not an lvalue");
}

// 生成表达式
static void genExpr(Node *Nd) {
	switch (Nd->Kind) {
	// 加载数字到a0
	case ND_NUM:
		printf("  # 将数字%d加载到a0\n", Nd->Val);
		printf("  li a0, %d\n", Nd->Val);
		return ;
	// 对寄存器取反
	case ND_NEG:
		printf("  # 对a0的值取反\n");
		genExpr(Nd->LHS);
		printf("  neg a0, a0\n");
		return ;
	case ND_VAR:
		// 计算出变量的地址，然后存入a0
		genAddr(Nd);
		printf("  # 读取a0中的地址 将值存入a0\n"); 
		printf("  ld a0, 0(a0)\n");
		return ;
	case ND_ASSIGN:
		// 加载地址到a0
		genAddr(Nd->LHS);
		// 推入栈中
		push();
		genExpr(Nd->RHS);
		pop("a1");
		printf("  # 将a0的值存入到a1中的地址处\n");
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
			printf("  # a0+a1，结果写入a0\n");
			printf("  add a0, a0, a1\n");
			return ;
		case ND_SUB:
			printf("  # a0-a1，结果写入a0\n");
			printf("  sub a0, a0, a1\n");
			return ;
		case ND_MUL:
			printf("  # a0×a1，结果写入a0\n");
			printf("  mul a0, a0, a1\n");
			return ;
		case ND_DIV:
			printf("  # a0÷a1，结果写入a0\n");
			printf("  div a0, a0, a1\n");
			return ;
		case ND_EQ:
		case ND_NE:
			// a0=a0^a1，异或指令
			printf("  # 判断是否a0%sa1\n", Nd->Kind == ND_EQ ? "==" : "!=");
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
			printf("  # 判断a0<a1\n");
			printf("  slt a0, a0, a1\n");
    		return;
		case ND_LE:
			// a0<=a1等价于
    		// a0=a1<a0, a0=a1^1
			printf("  # 判断是否a0≤a1\n");
			printf("  slt a0, a1, a0\n");
			printf("  xori a0, a0, 1\n");
			return;
		default:
			break;
	}

	errorTok(Nd->Tok, "invalid expression");
}

// 生成语句
static void genStmt(Node *Nd) {
	switch (Nd->Kind) {
		//  生成for语句或者while语句
		case ND_FOR: {
			int C = count();
			printf("\n# =====循环语句%d===============\n", C);
			// 生成条件内语句
			if (Nd->Init) {
				printf("\n# Init语句%d\n", C);
				genStmt(Nd->Init);
			}
			printf("\n# 循环%d的.L.begin.%d段标签\n", C, C);
			printf(".L.Begin.%d:\n", C);
			printf("# Cond表达式%d\n", C);
			if (Nd->Cond) {
				genExpr(Nd->Cond);
				printf("  # 若a0为0，则跳转到循环%d的.L.end.%d段\n", C, C);
				printf("  beqz a0, .L.End.%d\n", C);
			}
			// for块内语句
			printf("\n# Then语句%d\n", C);
			genStmt(Nd->Then);
			if (Nd->Inc) {
				printf("\n# Inc语句%d\n", C);
				genExpr(Nd->Inc);
			}
			printf("  # 跳转到循环%d的.L.begin.%d段\n", C, C);
			printf("  j .L.Begin.%d\n", C);
			printf("\n# 循环%d的.L.end.%d段标签\n", C, C);
			printf(".L.End.%d:\n", C);
			return ;
		}
		//  生成if语句
		case ND_IF: {
			// if嵌入深度
			int C = count();
			printf("\n# =====分支语句%d==============\n", C);
			// 生成条件内语句
			printf("\n# Cond表达式%d\n", C);
			genExpr(Nd->Cond);
			// 若a0为0跳转到else 不为0跳转到Then
			printf("  # 若a0为0，则跳转到分支%d的.L.else.%d段\n", C, C);
			printf("  beqz a0, .L.else.%d\n", C);
			// 生成Then块语句
			printf("\n# Then语句%d\n", C);
			genStmt(Nd->Then);
			// 执行完Then跳出
			printf("  # 跳转到分支%d的.L.end.%d段\n", C, C);
			printf("  j .L.end.%d\n", C);
			// else块标志
			printf("\n# Else语句%d\n", C);
    		printf("# 分支%d的.L.else.%d段标签\n", C, C);
			printf(".L.else.%d:\n", C);
			// 生成Else块语句
			if (Nd->Els) {
				genStmt(Nd->Els);
			}
			// if语句结束标志
			printf("\n# 分支%d的.L.end.%d段标签\n", C, C);
			printf(".L.end.%d:\n", C);
			return ;
		}
		// 遍历代码块
		case ND_BLOCK:
			for (Node *N = Nd->Body; N; N = N->Next) {
				genStmt(N);
			}
			return ;
		// 生成return语句
		case ND_RETURN:
			printf("# 返回语句\n");
			genExpr(Nd->LHS);
			// 无条件跳转语句，跳转到.L.return段
    		// j offset是 jal x0, offset的别名指令
			printf("  # 跳转到.L.return段\n");
			printf("  j .L.return\n");
			return;
		// 生成表达式语句
		case ND_EXPR_STMT:
			genExpr(Nd->LHS);
			return ;
		default:
			break;
	}
	errorTok(Nd->Tok, "invalid expression");
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
	printf("  # 定义全局main段\n");
	printf("  .globl main\n");
	printf("\n# =====程序开始===============\n");
	printf("# main段标签，也是程序入口段\n");
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
	printf("  # 将fp压栈,fp属于“被调用者保存”的寄存器,需要恢复原值\n");
	printf("  addi sp, sp, -8\n");
	printf("  sd fp, 0(sp)\n");
	// 将sp写入fp
	printf("  # 将sp的值写入fp\n");
	printf("  mv fp, sp\n");

	// 偏移量为实际变量所用的栈大小
	printf("  # sp腾出StackSize大小的栈空间\n");
  	printf("  addi sp, sp, -%d\n", Prog->StackSize);

	// 生成语句链表的代码
	printf("\n# =====程序主体===============\n");
	genStmt(Prog->Body);
	assert(Depth == 0);

	// Epilogue，后语
	// 输出标签
	printf("\n# =====程序结束===============\n");
 	printf("# return段标签\n");
	printf(".L.return:\n");
	// 将fp的值改写回sp
	printf("  # 将fp的值写回sp\n");
	printf("  mv sp, fp\n");
	// 将最早fp保存的值弹栈，恢复fp
	printf("  # 将最早fp保存的值弹栈，恢复fp和sp\n");
	printf("  ld fp, 0(sp)\n");
	printf("  addi sp, sp, 8\n");
	// 返回
	printf("  # 返回a0值给系统调用\n");
	printf("  ret\n");
}