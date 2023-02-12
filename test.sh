#!/bin/bash

assert() {
  exepected="$1"
  input="$2"

  # 运行失败exit 
  # 成功短路
  ./rvcc "$input" > tmp.s || exit
  
  # 编译rvcc产生的汇编文件
  ~/riscv/bin/riscv64-unknown-linux-gnu-gcc -static -o tmp tmp.s

  # qemu运行
  ~/riscv/bin/qemu-riscv64 -L ~/riscv/sysroot ./tmp
  
  # 实际运算值
  actual="$?"
  

  if [ "$actual" = "$exepected" ]; then
    echo "$input => $actual"
  else
    echo "$input => $exepected exepected, but got $actual"
    exit 1
  fi
}


assert 0 0
assert 100 100
assert 10 '4+8-2'

echo OK