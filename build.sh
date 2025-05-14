#!/usr/bin/sh

CFLAGS="-Wall -Wextra"
LDFLAGS="-z execstack"
SRC="$(find src -name "*.c")"
MVM_SRC_PATH="../mvm/src"
LEXGEN_RUNTIME_SRC_PATH="../lexgen/runtime-src"
MVM_LIBS_PATH="libs/mvm"
LEXGEN_RUNTIME_LIBS_PATH="libs/lexgen-runtime"

if [ "$1" == "--rebuild-libs" ]; then
  BUILD_FLAGS="${@:2}"
  if [ -d "libs" ]; then
    rm -r libs
  fi
else
  BUILD_FLAGS="${@:1}"
fi

if [ ! -d libs ]; then
  mkdir libs
fi

if [ -d $MVM_LIBS_PATH ] && [ "$1" == "--rebuild-libs" ]; then
  rm -r $MVM_LIBS_PATH
fi

if [ -d $LEXGEN_RUNTIME_LIBS_PATH ] && [ "$1" == "--rebuild-libs" ]; then
  rm -r $LEXGEN_RUNTIME_LIBS_PATH
fi

if [ ! -d $MVM_LIBS_PATH ]; then
  cp -r $MVM_SRC_PATH $MVM_LIBS_PATH
fi

if [ ! -d $LEXGEN_RUNTIME_LIBS_PATH ]; then
  cp -r $LEXGEN_RUNTIME_SRC_PATH $LEXGEN_RUNTIME_LIBS_PATH
fi

LIBS_SRC="$(find libs -name "*.c")"

lexgen grammar.h grammar.lg
cc -o mvl $CFLAGS $LDFLAGS $BUILD_FLAGS $SRC $LIBS_SRC
