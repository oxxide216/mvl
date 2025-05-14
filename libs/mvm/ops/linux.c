#include <sys/mman.h>
#include <fcntl.h>

#include "general.h"
#include "../ops.h"

void gen_exit_op(StringBuilder *sb, Loc dest_loc, Str *args) {
  (void) dest_loc;

  if (!str_eq(args[0], STR_LIT("rdi"))) {
    sb_push(sb, "  mov rdi,");
    sb_push_str(sb, args[0]);
    sb_push_char(sb, '\n');
  }
  sb_push(sb, "  mov rax,60\n");
  sb_push(sb, "  syscall\n");
}

void gen_write_op(StringBuilder *sb, Loc dest_loc, Str *args) {
  (void) dest_loc;

  if (!str_eq(args[0], STR_LIT("rdi"))) {
    sb_push(sb, "  mov rdi,");
    sb_push_str(sb, args[0]);
    sb_push_char(sb, '\n');
  }
  if (!str_eq(args[1], STR_LIT("rsi"))) {
    sb_push(sb, "  mov rsi,");
    sb_push_str(sb, args[1]);
    sb_push_char(sb, '\n');
  }
  if (!str_eq(args[2], STR_LIT("rdx"))) {
    sb_push(sb, "  mov rdx,");
    sb_push_str(sb, args[2]);
    sb_push_char(sb, '\n');
  }
  sb_push(sb, "  mov rax,1\n");
  sb_push(sb, "  syscall\n");
}

void gen_read_op(StringBuilder *sb, Loc dest_loc, Str *args) {
  if (!str_eq(args[0], STR_LIT("rdi"))) {
    sb_push(sb, "  mov rdi,");
    sb_push_str(sb, args[0]);
    sb_push_char(sb, '\n');
  }
  if (!str_eq(args[1], STR_LIT("rsi"))) {
    sb_push(sb, "  mov rsi,");
    sb_push_str(sb, args[1]);
    sb_push_char(sb, '\n');
  }
  if (!str_eq(args[2], STR_LIT("rdx"))) {
    sb_push(sb, "  mov rdx,");
    sb_push_str(sb, args[2]);
    sb_push_char(sb, '\n');
  }
  sb_push(sb, "  mov rax,0\n");
  sb_push(sb, "  syscall\n");

  if (!str_eq(dest_loc.str, STR_LIT("rax"))) {
    sb_push(sb, "  mov ");
    sb_push_str(sb, dest_loc.str);
    sb_push(sb, ",rax\n");
  }
}

void gen_mmap_op(StringBuilder *sb, Loc dest_loc, Str *args) {
  if (!str_eq(args[0], STR_LIT("rsi"))) {
    sb_push(sb, "  mov rsi,");
    sb_push_str(sb, args[0]);
    sb_push_char(sb, '\n');
  }
  sb_push(sb, "  mov rdi,0\n");
  sb_push(sb, "  mov rdx,");
  sb_push_u32(sb, PROT_READ | PROT_WRITE);
  sb_push(sb, "\n  mov r10,");
  sb_push_u32(sb, MAP_PRIVATE | MAP_ANONYMOUS);
  sb_push(sb, "\n  mov r8,-1\n");
  sb_push(sb, "  mov r9,0\n");
  sb_push(sb, "  mov rax,9\n");
  sb_push(sb, "  syscall\n");

  sb_push(sb, "  mov ");
  sb_push_str(sb, dest_loc.str);
  sb_push(sb, ",rax\n");
}

void gen_munmap_op(StringBuilder *sb, Loc dest_loc, Str *args) {
  (void) dest_loc;

  if (!str_eq(args[0], STR_LIT("rdi"))) {
    sb_push(sb, "  mov rdi,");
    sb_push_str(sb, args[0]);
    sb_push_char(sb, '\n');
  }
  if (!str_eq(args[1], STR_LIT("rsi"))) {
    sb_push(sb, "  mov rsi,");
    sb_push_str(sb, args[1]);
    sb_push_char(sb, '\n');
  }
  sb_push(sb, "  mov rax,11\n");
  sb_push(sb, "  syscall\n");
}

void gen_openat_op(StringBuilder *sb, Loc dest_loc, Str *args) {
  sb_push(sb, "  mov rdi,");
  sb_push_u32(sb, AT_FDCWD);
  sb_push_char(sb, '\n');
  if (!str_eq(args[0], STR_LIT("rsi"))) {
    sb_push(sb, "  mov rsi,");
    sb_push_str(sb, args[0]);
    sb_push_char(sb, '\n');
  }
  sb_push(sb, "  mov rdx,");
  sb_push_u32(sb, O_RDWR);
  sb_push_char(sb, '\n');
  sb_push(sb, "  mov rax,257\n");
  sb_push(sb, "  syscall\n");
  sb_push(sb, "  mov ");
  sb_push_str(sb, dest_loc.str);
  sb_push(sb, ",rax\n");
}

void gen_close_op(StringBuilder *sb, Loc dest_loc, Str *args) {
  (void) dest_loc;

  if (!str_eq(args[0], STR_LIT("rdi"))) {
    sb_push(sb, "  mov rdi,");
    sb_push_str(sb, args[0]);
    sb_push_char(sb, '\n');
  }
  sb_push(sb, "  mov rax,3\n");
  sb_push(sb, "  syscall\n");
}

void gen_fstat_op(StringBuilder *sb, Loc dest_loc, Str *args) {
  (void) dest_loc;

  if (!str_eq(args[0], STR_LIT("rdi"))) {
    sb_push(sb, "  mov rdi,");
    sb_push_str(sb, args[0]);
    sb_push_char(sb, '\n');
  }
  if (!str_eq(args[1], STR_LIT("rsi"))) {
    sb_push(sb, "  mov rsi,");
    sb_push_str(sb, args[1]);
    sb_push_char(sb, '\n');
  }
  sb_push(sb, "  mov rax,5\n");
  sb_push(sb, "  syscall\n");
}

static OpArg bin_op_args[] = {
  { ValueKindS64, ArgConditionAny },
  { ValueKindS64, ArgConditionAny },
};

static OpArg un_op_args[] = {
  { ValueKindS64, ArgConditionAny },
};

static OpArg ternary_op_args[] = {
  { ValueKindS64, ArgConditionAny },
  { ValueKindS64, ArgConditionAny },
  { ValueKindS64, ArgConditionAny },
};

static Op ops[] = {
  {
    STR_LIT("exit"),
    ValueKindUnit,
    un_op_args,
    1,
    false,
  },
  {
    STR_LIT("write"),
    ValueKindUnit,
    ternary_op_args,
    3,
    false,
  },
  {
    STR_LIT("read"),
    ValueKindS64,
    ternary_op_args,
    3,
    false,
  },
  {
    STR_LIT("mmap"),
    ValueKindS64,
    un_op_args,
    1,
    false,
  },
  {
    STR_LIT("munmap"),
    ValueKindUnit,
    bin_op_args,
    2,
    false,
  },
  {
    STR_LIT("openat"),
    ValueKindS64,
    un_op_args,
    1,
    false,
  },
  {
    STR_LIT("close"),
    ValueKindUnit,
    un_op_args,
    1,
    false,
  },
  {
    STR_LIT("fstat"),
    ValueKindUnit,
    bin_op_args,
    2,
    false,
  },
};

static OpGenProc op_gen_procs[] = {
  gen_exit_op,
  gen_write_op,
  gen_read_op,
  gen_mmap_op,
  gen_munmap_op,
  gen_openat_op,
  gen_close_op,
  gen_fstat_op,
};


Ops get_ops_linux(void) {
  return (Ops) { ops, ARRAY_LEN(ops) };
}

OpGenProcs get_op_gen_procs_linux(void) {
  return (OpGenProcs) { op_gen_procs, ARRAY_LEN(op_gen_procs) };
}
