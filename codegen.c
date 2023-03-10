#include "rvcc.h"

// 记录栈的深度
static int Depth;
// 用于函数参数的寄存器们
static char *ArgReg[] = {"a0", "a1", "a2", "a3", "a4", "a5"};
// 当前的函数
static Function *CurrentFn;

static void genExpr(Node *Nd);

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
	switch (Nd->Kind) {
		// 变量
		case ND_VAR:
			printf("  # 获取变量%s的偏移地址%d并存入a0\n", Nd->Var->Name, Nd->Var->offset);
			printf("  addi a0, fp, %d\n", Nd->Var->offset);
			return;
		// 解引用 *
		case ND_DEREF:
			genExpr(Nd->LHS);
			return ;
		default:
			return ;
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
	// 解引用
	case ND_DEREF:
		genExpr(Nd->LHS);
		printf("  # 读取a0中存放的地址，得到的值存入a0\n");
		printf("  ld a0, 0(a0)\n");
		return ;
	// 取地址
	case ND_ADDR:
		genAddr(Nd->LHS);
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
	case ND_FUNCALL:
		// 记录参数个数
		int NArgs = 0;
		// 计算所有的参数值 正向压栈
		for (Node *Arg = Nd->Args; Arg; Arg = Arg->Next) {
			genExpr(Arg);
			push();
			NArgs++;
		}
		for (int I = NArgs - 1; I >= 0; I--) {
			pop(ArgReg[I]);
		}
		printf("\n  # 调用函数%s\n", Nd->FuncName);
		printf("  call %s\n", Nd->FuncName);
		return;
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
			printf("  # 跳转到.L.return.%s段\n", CurrentFn->Name);
			printf("  j .L.return.%s\n", CurrentFn->Name);
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
	for (Function *Fn = Prog; Fn; Fn = Fn->Next) {
		int Offset = 0;
		// 读取所有变量
		for (Obj *Var = Fn->Locals; Var; Var = Var->Next) {
			// 每个变量分配8字节
			Offset += 8;
			// 为每个变量赋一个偏移量，或者说是栈中地址
			Var->offset = -Offset;
		}
		// 将栈对齐到16字节
		Fn->StackSize = alignTo(Offset, 16);
	}
}

void codegen(Function *Prog) {
	assignLVarOffsets(Prog);
	for (Function *Fn = Prog; Fn; Fn = Fn->Next) {
		// 声明一个全局main段，同时也是程序入口段
		printf("  # 定义全局%s段\n", Fn->Name);
		printf("  .globl %s\n", Fn->Name);
		printf("\n# =====程序开始===============\n");
		printf("# %s段标签\n", Fn->Name);
		printf("%s:\n", Fn->Name);
		CurrentFn = Fn;

		// 栈布局
		//-------------------------------// sp
		//              ra
		//-------------------------------// ra = sp-8
		//              fp
		//-------------------------------// fp = sp-16
		//             变量
		//-------------------------------// sp = sp-8-StackSize
		//           表达式计算
		//-------------------------------//

		// Prologue, 前言
		// 将寄存器ra压栈 保存ra的值
		printf("  # 将ra寄存器压栈,保存ra的值\n");
		printf("  addi sp, sp, -16\n");
		printf("  sd ra, 8(sp)\n");
		// 将fp压入栈中，保存fp的值
		printf("  # 将fp压栈,fp属于“被调用者保存”的寄存器,需要恢复原值\n");
		printf("  sd fp, 0(sp)\n");
		// 将sp写入fp
		printf("  # 将sp的值写入fp\n");
		printf("  mv fp, sp\n");

		// 偏移量为实际变量所用的栈大小
		printf("  # sp腾出StackSize大小的栈空间\n");
		printf("  addi sp, sp, -%d\n", Fn->StackSize);

		int I = 0;
		for (Obj *Var = Fn->Params; Var; Var = Var->Next) {
			printf("  # 将寄存器%s的值存入变量名%s的栈地址中\n", ArgReg[I], Var->Name);
			printf("  sd %s, %d(fp)\n", ArgReg[I++], Var->offset);
		}

		// 生成语句链表的代码
		printf("\n# =====程序主体===============\n");
		genStmt(Fn->Body);
		assert(Depth == 0);

		// Epilogue，后语
		// 输出标签
		printf("\n# =====%s结束===============\n", Fn->Name);
		printf("# return段标签\n");
		printf(".L.return.%s:\n", Fn->Name);
		// 将fp的值改写回sp
		printf("  # 将fp的值写回sp\n");
		printf("  mv sp, fp\n");
		// 将最早fp保存的值弹栈，恢复fp
		printf("  # 将fp寄存器的值弹栈，恢复fp\n");
		printf("  ld fp, 0(sp)\n");
		printf("  # 将ra的寄存器弹栈， 恢复ra\n");
		printf("  ld ra, 8(sp)\n");
		printf("  addi sp, sp, 16\n");
		// 返回
		printf("  # 返回a0值给系统调用\n");
		printf("  ret\n");
	}
}