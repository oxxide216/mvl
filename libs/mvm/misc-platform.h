#ifndef MISC_PLATFORM_H
#define MISC_PLATFORM_H

#include "mvm.h"
#include "shl_str.h"

typedef struct {
  Str *items;
  u32  len;
} ReservedProcs;

typedef struct {
  u32 *indices;
} VariableLayers;

void sb_begin_program_wrap(StringBuilder *sb, Program *program, TargetPlatform target);
ReservedProcs get_reserved_procs(TargetPlatform target);
VariableLayers get_layers(TargetPlatform target);

#endif // MISC_PLATFORM_H
