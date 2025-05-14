#ifndef OPS_GENERAL_H
#define OPS_GENERAL_H

#include "../shl_str.h"
#include "../mvm.h"

typedef enum {
  LocKindReg = 0,
  LocKindStack,
} LocKind;

typedef struct {
  LocKind kind;
  Str     str;
} Loc;

typedef void (*OpGenProc)(StringBuilder *sb, Loc result_loc, Str *args);

typedef struct {
  OpGenProc *items;
  u32        len;
} OpGenProcs;

#endif // OPS_GENERAL_H
