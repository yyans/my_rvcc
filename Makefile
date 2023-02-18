# C编译器参数：使用C11标准，生成debug信息，禁止将未初始化的全局变量放入到common段
CFLAGS=-std=c11 -g -fno-common
# 指定C编译器，来构建项目
CC=gcc
# C源代码文件，表示所有的.c结尾的文件  获取一个.c结尾的文件列表
SRCS=$(wildcard *.c)
# C文件编译生成的未链接的可重定位文件，将所有.c文件替换为同名的.o结尾的文件名
OBJS=$(SRCS:.c=.o)

rvcc: $(OBJS)
	$(CC) -o $@ $(CFLAGS) $^ $(LDFLAGES)

$(OBJS): rvcc.h 

test: rvcc
	./test.sh

clean:
	rm -f rvcc *.o *.s tmp* a.out

.PHONY: test clean
