#!/usr/bin/sh

if [ "$1" != "" ]; then
  ./mvl test.s $1 && yasm -f elf64 test.s && ld -o test test.o && ./test
  echo $?
fi
