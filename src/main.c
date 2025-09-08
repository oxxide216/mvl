#define SHL_DEFS_LL_ALLOC aalloc

#include "mvm/src/mvm.h"
#include "mvm/src/misc.h"
#define LEXGEN_TRANSITION_TABLE_IMPLEMENTATION
#include "lexgen/runtime-src/runtime.h"
#include "../grammar.h"
#include "io.h"
#include "parser.h"
#include "ir.h"
#include "compiler.h"
#define SHL_STR_IMPLEMENTATION
#include "shl_str.h"
#define SHL_ARENA_IMPLEMENTATION
#include "shl_arena.h"
#include "shl_log.h"

static char *str_to_cstr(Str str) {
  char *result = malloc((str.len + 1) * sizeof(char));
  memcpy(result, str.ptr, str.len * sizeof(char));
  result[str.len] = 0;
  return result;
}

static Str get_file_dir(Str path) {
  for (u32 i = path.len; i > 0; --i)
    if (path.ptr[i - 1] == '/')
      return (Str) { path.ptr, i };

  return (Str) {0};
}

int main(i32 argv, i8 **argc) {
  if (argv < 2) {
    ERROR("Output file was not provided\n");
    exit(1);
  }

  if (argv < 3) {
    ERROR("Input file was not provided\n");
    exit(1);
  }

  bool silent_mode = false;
  if (argv > 3 && strcmp(argc[3], "-s") == 0)
    silent_mode = true;

  Str text = read_file(argc[2]);
  if (!text.ptr) {
    ERROR("Could not read %s\n", argc[2]);
    exit(1);
  }

  if (!silent_mode)
    printf("Source code:\n"STR_FMT"\n", STR_ARG(text));

  Str file_path = str_new(argc[2]);

  Tokens tokens = {0};
  lex(text, &tokens, file_path);

  Da(Str) included_file_paths = {0};
  DA_APPEND(included_file_paths, file_path);

  for (u32 i = 0; i < tokens.len; ++i) {
    Token *token = tokens.items + i;
    if (token->id != TT_INCLUDE)
      continue;

    Token *path_token = NULL;
    if (i + 1 < tokens.len)
      path_token = token + 1;
    expect_token(path_token, MASK(TT_STR_LIT));

    StringBuilder sb = {0};
    sb_push_str(&sb, get_file_dir(token->file_path));
    sb_push_str(&sb, path_token->lexeme);
    Str path_str = sb_to_str(sb);

    bool already_included = false;
    for (u32 j = 0; j < included_file_paths.len; ++j) {
      if (str_eq(included_file_paths.items[j], path_str)) {
        already_included = true;
        break;
      }
    }

    ++i;

    if (already_included)
      continue;
    DA_APPEND(included_file_paths, path_str);

    char *path = str_to_cstr(path_str);
    text = read_file(path);
    if (!text.ptr) {
      ERROR("Could not read %s\n", path);
      exit(1);
    }

    lex(text, &tokens, path_str);
  }

  Ir ir = parse(&tokens);
  Program program = compile_ir(&ir);

  program_optimize(&program, Arch_X86_64);
  Str _asm = program_gen_code(&program, Arch_X86_64);

  if (!silent_mode)
    printf("Assembly:\n"STR_FMT, STR_ARG(_asm));

  if (!write_file(argc[1], _asm)) {
    ERROR("Could not write to %s\n", argc[1]);
    exit(1);
  }

  return 0;
}
