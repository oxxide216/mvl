#include "misc-platform.h"
#include "log.h"

static u32 x86_64_indices[ValueKindCount] = {
  [ValueKindUnit] = 0,
  [ValueKindS64] = 0,
};

void sb_begin_program_wrap(StringBuilder *sb, Program *program, TargetPlatform target) {
  switch (target) {
  case TargetPlatformRaw_X86_64: break;

  case TargetPlatformLinux_X86_64: {
    sb_push(sb, "global _start\n");
    sb_push(sb, "section .text\n");
    sb_push(sb, "_start:\n");
    if (program->procs) {
      sb_push(sb, "  mov rdi,qword[rsp]\n");
      sb_push(sb, "  lea rsi,qword[rsp+8]\n");
      sb_push(sb, "  call ");
      sb_push_str(sb, program->procs->name);
      sb_push(sb, "\n  mov rdi,rax\n");
    }
    if (!program->procs || program->procs->ret_val_kind == ValueKindUnit)
      sb_push(sb, "  mov rdi,0\n");
    sb_push(sb, "  mov rax,60\n");
    sb_push(sb, "  syscall\n");
  } break;
  }
}

ReservedProcs get_reserved_procs(TargetPlatform target) {
  static Str reserved_procs_linux_x86_64[] = { STR_LIT("_start") };

  switch (target) {
  case TargetPlatformRaw_X86_64: {
    return  (ReservedProcs) {0};
  };

  case TargetPlatformLinux_X86_64: {
    return (ReservedProcs) {
      reserved_procs_linux_x86_64,
      ARRAY_LEN(reserved_procs_linux_x86_64),
    };
  }

  default: {
    ERROR("Wrong target platform\n");
    exit(1);
  }
  }
}

VariableLayers get_layers(TargetPlatform target) {
  switch (target) {
  case TargetPlatformRaw_X86_64:
  case TargetPlatformLinux_X86_64: {
    return (VariableLayers) { x86_64_indices };
  }

  default: {
    ERROR("Wrong target platform\n");
    exit(1);
  }
  }
}
