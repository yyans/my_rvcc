# C编译器参数：使用C11标准，生成debug信息，禁止将未初始化的全局变量放入到common段
CFLAGS=-std=c11 -g -fno-common
# 指定C编译器，来构建项目
CC=gcc

rvcc: main.o
	$(CC) -o rvcc $(CFLAGS) main.o


test: rvcc
	./test.sh


clean:
	rm -f rvcc *.o *.s tmp* a.out


.PHONY: test clean
