#!/usr/bin/bash

CFLAGS="-Wall -Wextra -Ilibs -Ilibs/lexgen/include"
LDFLAGS="-z execstack"
BUILD_FLAGS="${@:1}"
SRC="$(find src -name "*.c")"
MVM_SRC="$(find libs/mvm/src -name "*.c")"
LEXGEN_RUNTIME_SRC="$(find libs/lexgen/src/runtime -name "*.c")"

cd libs/lexgen
./build.sh
cd ../..

libs/lexgen/lexgen grammar.h grammar.lg

cc -o mvl $CFLAGS $LDFLAGS $BUILD_FLAGS $SRC $MVM_SRC $LEXGEN_RUNTIME_SRC
