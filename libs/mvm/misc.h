#ifndef MISC_H
#define MICS_H

#include "mvm.h"
#include "proc-ctx.h"

u32        get_value_size(ValueKind kind);
ValueKind  str_to_value_kind(Str str);
Str        value_to_str(Value value);
Arg       *get_first_arg_ptr(Instr *instr);
Arg       *get_second_arg_ptr(Instr *instr);
void      *arrays_concat(void *a, u32 a_len, void *b, u32 b_len, u32 size);

#endif // MISC_H
