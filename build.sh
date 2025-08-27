#!/usr/bin/sh

CFLAGS="-Wall -Wextra -Ilibs"
LDFLAGS="-z execstack"
BUILD_FLAGS="${@:1}"
SRC="$(find src -name "*.c")"
MVM_SRC="$(find libs/mvm/src -name "*.c")"
LEXGEN_RUNTIME_SRC="$(find libs/lexgen/runtime-src -name "*.c")"

lexgen grammar.h grammar.lg
cc -o mvl $CFLAGS $LDFLAGS $BUILD_FLAGS $SRC $MVM_SRC $LEXGEN_RUNTIME_SRC
