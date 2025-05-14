#include <string.h>

#define SHL_STR_IMPLEMENTATION
#define SHL_ARENA_IMPLEMENTATION
#define SHL_DEFS_LL_ALLOC aalloc

#include "../libs/lexgen-runtime/runtime.h"
#include "../libs/mvm/mvm.h"
#include "../libs/mvm/misc.h"
#include "../grammar.h"
#include "shl_str.h"
#include "shl_arena.h"
#include "log.h"
#include "io.h"

typedef struct {
  Str lexeme;
  u32 id;
  u32 row, col;
  Str file_path;
} Token;

typedef Da(Token) Tokens;

typedef struct {
  Tokens  tokens;
  u32     index;
} Parser;

static Str token_id_names[ARRAY_LEN(tt)] = {
  STR_LIT("new line"),
  STR_LIT("whitespace"),
  STR_LIT("comment"),
  STR_LIT("`proc`"),
  STR_LIT("`jump`"),
  STR_LIT("`if`"),
  STR_LIT("`call`"),
  STR_LIT("`ret`"),
  STR_LIT("`alloc`"),
  STR_LIT("`include`"),
  STR_LIT("`static`"),
  STR_LIT("identifier"),
  STR_LIT("number"),
  STR_LIT("`(`"),
  STR_LIT("`)`"),
  STR_LIT("`,`"),
  STR_LIT("`@`"),
  STR_LIT("`:`"),
  STR_LIT("`==`"),
  STR_LIT("`!=`"),
  STR_LIT("`>=`"),
  STR_LIT("`<=`"),
  STR_LIT("`>`"),
  STR_LIT("`<`"),
  STR_LIT("`=`"),
  STR_LIT("`->`"),
  STR_LIT("string literal"),
  STR_LIT("character literal"),
};

#define MASK(index) (1 << (index))

char escape_char(char _char) {
  switch (_char) {
  case 'n': return '\n';
  case 'r': return '\r';
  case 't': return '\t';
  case 'v': return '\v';
  case '0': return '\0';
  case 'e': return '\033';
  default:  return _char;
  }
}

Str escape_str(Str str) {
  Str new_str = {
    aalloc(str.len * sizeof(char)),
    str.len,
  };

  memcpy(new_str.ptr, str.ptr, str.len * sizeof(char));

  bool is_escaped = false;

  for (u32 i = 0; i < new_str.len; ++i) {
    if (is_escaped)
      new_str.ptr[i] = escape_char(new_str.ptr[i]);

    if (new_str.ptr[i] == '\\') {
      if (!is_escaped) {
        memmove(new_str.ptr + i, new_str.ptr + i + 1, new_str.len - i - 1);
        --new_str.len;
        --i;
        is_escaped = true;
      }
    } else {
      is_escaped = false;
    }
  }

  return new_str;
}

static void lex(Str text, Tokens *tokens, Str file_path) {
  TransitionTable table = {0};
  u32 row = 0, col = 0;

  for (u32 i = 0; i < ARRAY_LEN(tt); ++i)
    tt_push_row(&table, tt[i].cols, tt[i].cols_count);

  while (text.len > 0) {
    u32 token_id = 0;
    Str lexeme = tt_matches(&table, &text, &token_id);

    if (token_id == (u32) -1) {
      PERROR(STR_FMT":%u:%u: ", "unexpected `%c`\n",
             STR_ARG(file_path), row + 1, col + 1, text.ptr[0]);
      exit(1);
    }

    Token new_token = { lexeme, token_id, row, col, file_path };

    col += lexeme.len;

    if (token_id == TT_NEWLINE) {
      ++row;
      col = 0;
    }

    if (token_id == TT_COMMENT) {
      u32 i = 0;

      while (i < (u32) text.len) {
        if (text.ptr[i] == '\n') {
          ++row;
          col = 0;
          text.ptr += i + 1;
          text.len -= i + 1;
          break;
        }

        ++i;
      }

      if (i == (u32) text.len)
        text.len = 0;

      continue;
    }

    if (token_id == TT_SKIP)
      continue;

    if (token_id == TT_STR_LIT) {
      u32 i = 0;
      bool is_escaped = false;

      while (i < (u32) text.len && (text.ptr[i] != '"' || is_escaped)) {
        ++col;

        if (text.ptr[i] == '\\')
          is_escaped = true;
        else
          is_escaped = false;

        if (text.ptr[i] == '\n') {
          ++row;
          col = 0;
        }

        ++i;
      }

      if (i == (u32) text.len) {
        PERROR(STR_FMT":%u:%u: ", "unclosed string literal\n",
               STR_ARG(file_path), new_token.row + 1, new_token.col + 1);
        exit(1);
      }

      text.ptr += i + 1;
      text.len -= i + 1;

      new_token.lexeme.ptr += 1;
      new_token.lexeme.len += i - 1;

      new_token.lexeme = escape_str(new_token.lexeme);
    }

    if (token_id == TT_CHAR_LIT) {
      if (text.len < 2) {
        PERROR(STR_FMT":%u:%u: ", "unclosed character literal",
               STR_ARG(file_path), new_token.row + 1, new_token.col + 1);
        exit(1);
      }

      if (text.ptr[0] == '\\') {
        if (text.len < 3) {
          PERROR(STR_FMT"%u:%u: ", "unclosed character literal",
                 STR_ARG(file_path), new_token.row + 1, new_token.col + 1);
          exit(1);
        }

        text.ptr[0] = escape_char(text.ptr[1]);

        text.ptr += 1;
        text.len -= 1;
      }

      text.ptr += 2;
      text.len -= 2;

      new_token.lexeme.ptr += 1;
    }

    DA_APPEND(*tokens, new_token);
  }
}

static Token *parser_peek_token(Parser *parser) {
  if (parser->index >= parser->tokens.len)
    return NULL;

  return parser->tokens.items + parser->index;
}

static Token *parser_next_token(Parser *parser) {
  Token *token = parser_peek_token(parser);
  ++parser->index;
  return token;
}

static void print_id_mask(u32 id_mask, Str lexeme, FILE *stream) {
  u32 print_or = false;
  for (u32 i = 0; i < ARRAY_LEN(token_id_names); ++i) {
    if (id_mask & MASK(i)) {
      if (!print_or)
        print_or = true;
      else
        fputs(" or ", stdout);

      if ((i == TT_NUMBER || i == TT_IDENT) && lexeme.len != 0) {
        fputc('`', stream);
        str_print(lexeme);
        fputc('`', stream);
      } else {
        str_fprint(stream, token_id_names[i]);
      }
    }
  }
}

static void expect_token(Token *token, u32 id_mask) {
  if (!token) {
    ERROR("Expected ");
    print_id_mask(id_mask, (Str) {0}, stderr);
    fputs(", but got EOF\n", stdout);
    exit(1);
  }

  if (token && id_mask & (1 << token->id))
    return;

  PERROR(STR_FMT":%u:%u: ", "expected ",
         STR_ARG(token->file_path),
         token->row + 1, token->col + 1);
  print_id_mask(id_mask, (Str) {0}, stderr);
  fputs(", but got ", stderr);
  print_id_mask(MASK(token->id), token->lexeme, stderr);
  fputc('\n', stderr);
  exit(1);
}

static Token *parser_expect_token(Parser *parser, u32 id_mask) {
  Token *token = parser_next_token(parser);
  expect_token(token, id_mask);
  return token;
}

static Value str_to_value(Str str) {
  i32 number = 0;

  for (u32 i = 0; i < (u32) str.len; ++i) {
    number *= 10;
    number += str.ptr[i] - '0';
  }

  return (Value) {
    ValueKindS64,
    { .s64 = number },
  };
}

static Arg str_to_arg(Str text, ArgKind kind) {
  switch (kind) {
  case ArgKindValue: return arg_value(str_to_value(text));
  case ArgKindVar:   return arg_var(text);

  default: {
    ERROR("Wrong argument kind\n");
    exit(1);
  }
  }
}

static ArgKind token_id_to_arg_kind(u32 id) {
  switch (id) {
  case TT_NUMBER: return ArgKindValue;
  case TT_IDENT:  return ArgKindVar;

  default: {
    ERROR("Wrong token id\n");
    exit(1);
  }
  }
}

static Arg token_to_arg(Token *token, Program *program,
                        u32 *static_segment_index) {
  if (token->id == TT_STR_LIT) {
    StringBuilder sb = {0};
    sb_push(&sb, "?s");
    sb_push_u32(&sb, (*static_segment_index)++);

    u8 *bytes = aalloc(sizeof(token->lexeme.len) + 1);
    memcpy(bytes, token->lexeme.ptr, token->lexeme.len);
    bytes[token->lexeme.len] = '\0';
    program_push_static_segment(program, sb_to_str(sb),
                                bytes, token->lexeme.len + 1);

    return arg_var(sb_to_str(sb));
  }

  if (token->id == TT_CHAR_LIT) {
    return arg_value((Value) {
      ValueKindS64,
      { .s64 = token->lexeme.ptr[0] },
    });
  }

  ArgKind arg_kind = token_id_to_arg_kind(token->id);
  return str_to_arg(token->lexeme, arg_kind);
}

static Arg parser_parse_arg(Parser *parser, Program *program,
                            u32 *static_segment_index) {
  Token *arg = parser_expect_token(parser, MASK(TT_NUMBER) |
                                           MASK(TT_IDENT) |
                                           MASK(TT_STR_LIT) |
                                           MASK(TT_CHAR_LIT));

  return token_to_arg(arg, program, static_segment_index);
}

static void compile_op(Parser *parser,
                       Procedure *proc, Program *program,
                       Str dest, u32 *static_segment_index) {
  Token *op = parser_expect_token(parser, MASK(TT_IDENT));

  Args args = create_args();

  while (parser_peek_token(parser) &&
         parser_peek_token(parser)->id != TT_NEWLINE) {
    Arg arg = parser_parse_arg(parser, program, static_segment_index);
    args_push_arg(&args, arg);
  }

  proc_push_op(proc, op->lexeme, dest, args);
}

static void compile_call(Parser *parser, Program *program,
                         Procedure *proc, Str dest,
                         u32 *static_segment_index) {
  Token *callee = parser_expect_token(parser, MASK(TT_IDENT));
  Args args = {0};

  while (parser_peek_token(parser) &&
         parser_peek_token(parser)->id != TT_NEWLINE) {
    Arg arg = parser_parse_arg(parser, program, static_segment_index);
    args_push_arg(&args, arg);
  }

  if (dest.len > 0)
    proc_call_assign(proc, dest, callee->lexeme, args);
  else
    proc_call(proc, callee->lexeme, args);
}

static void compile(Tokens tokens, Program *program) {
  Parser parser = { tokens, 0 };
  Procedure *proc = NULL;
  u32 static_segment_index = 0;

  while (parser_peek_token(&parser)) {
    Token *token = parser_expect_token(&parser, MASK(TT_NEWLINE) |
                                                MASK(TT_PROC) |
                                                MASK(TT_JUMP) |
                                                MASK(TT_IF) |
                                                MASK(TT_CALL) |
                                                MASK(TT_RET) |
                                                MASK(TT_IDENT) |
                                                MASK(TT_AT) |
                                                MASK(TT_INCLUDE) |
                                                MASK(TT_STATIC));

    if (token->id != TT_PROC &&
        token->id != TT_NEWLINE &&
        token->id != TT_INCLUDE &&
        token->id != TT_STATIC &&
        proc == NULL) {
      ERROR("Every instruction should be inside of a procedure\n");
      exit(1);
    }

    switch (token->id) {
    case TT_NEWLINE: {
      continue;
    }

    case TT_PROC: {
      Token *name = parser_expect_token(&parser, MASK(TT_IDENT));
      ValueKind ret_val_kind = ValueKindUnit;
      ProcParams params = {0};

      Token *next = parser_peek_token(&parser);
      while (next->id != TT_NEWLINE && next->id != TT_RIGHT_ARROW) {
        Token *param_name = parser_expect_token(&parser, MASK(TT_IDENT));
        parser_expect_token(&parser, MASK(TT_COLON));
        Token *param_kind = parser_expect_token(&parser, MASK(TT_IDENT));

        ProcParam proc_param = { param_name->lexeme, str_to_value_kind(param_kind->lexeme) };
        DA_APPEND(params, proc_param);

        next = parser_peek_token(&parser);
      }

      if (next->id == TT_RIGHT_ARROW) {
        parser_next_token(&parser);
        Token *ret_val_kind_token = parser_expect_token(&parser, MASK(TT_IDENT));
        ret_val_kind = str_to_value_kind(ret_val_kind_token->lexeme);
      }

      proc = program_push_proc(program, name->lexeme, ret_val_kind, params);
    } break;

    case TT_JUMP: {
      Token *label_name = parser_expect_token(&parser, MASK(TT_IDENT));
      proc_jump(proc, label_name->lexeme);
    } break;

    case TT_IF: {
      Arg arg0 = parser_parse_arg(&parser, program, &static_segment_index);

      Token *op = parser_expect_token(&parser, MASK(TT_EQ) | MASK(TT_NE) |
                                               MASK(TT_GT) | MASK(TT_LS) |
                                               MASK(TT_GE) | MASK(TT_LE));

      Arg arg1 = parser_parse_arg(&parser, program,
                                  &static_segment_index);

      parser_expect_token(&parser, MASK(TT_JUMP));

      Token *label_name = parser_expect_token(&parser, MASK(TT_IDENT));

      RelOp rel_op;

      switch (op->id) {
      case TT_EQ: rel_op = RelOpEqual; break;
      case TT_NE: rel_op = RelOpNotEqual; break;
      case TT_GT: rel_op = RelOpGreater; break;
      case TT_LS: rel_op = RelOpLess; break;
      case TT_GE: rel_op = RelOpGreaterOrEqual; break;
      case TT_LE: rel_op = RelOpLessOrEqual; break;

      default: {
        ERROR("Unreachable\n");
        exit(1);
      }
      }

      proc_cond_jump(proc, rel_op, arg0, arg1, label_name->lexeme);
    } break;

    case TT_CALL: {
      compile_call(&parser, program, proc,
                   (Str) {0}, &static_segment_index);
    } break;

    case TT_RET: {
      if (parser_peek_token(&parser)->id == TT_NEWLINE) {
        proc_return(proc);
      } else {
        Arg arg = parser_parse_arg(&parser, program,
                                   &static_segment_index);
        proc_return_value(proc, arg);
      }
    } break;


    case TT_IDENT: {
      Token *next = parser_expect_token(&parser, MASK(TT_PUT) |
                                                 MASK(TT_COLON));
      if (next->id == TT_COLON) {
        proc_add_label(proc, token->lexeme);
        break;
      }

      next = parser_expect_token(&parser, MASK(TT_CALL) |
                                          MASK(TT_IDENT) |
                                          MASK(TT_NUMBER) |
                                          MASK(TT_AT) |
                                          MASK(TT_ALLOC) |
                                          MASK(TT_STR_LIT) |
                                          MASK(TT_CHAR_LIT));

      if (next->id == TT_CALL) {
        compile_call(&parser, program, proc,
                     token->lexeme, &static_segment_index);
        break;
      }

      if (next->id == TT_AT) {
        compile_op(&parser, proc, program,
                   token->lexeme, &static_segment_index);
        break;
      }

      if (next->id == TT_ALLOC) {
        Token *size = parser_expect_token(&parser, MASK(TT_NUMBER));
        proc_alloc(proc, token->lexeme, (u32) str_to_i32(size->lexeme));
        break;
      }

      Args args = create_args();
      args_push_arg(&args, token_to_arg(next, program, &static_segment_index));

      proc_push_op(proc, STR_LIT("put"), token->lexeme, args);
    } break;

    case TT_AT: {
      compile_op(&parser, proc, program, (Str) {0}, &static_segment_index);
    } break;

    case TT_INCLUDE: {
      parser_expect_token(&parser, MASK(TT_STR_LIT));
    } break;

    case TT_STATIC: {
      Token *name = parser_expect_token(&parser, MASK(TT_IDENT));
      parser_expect_token(&parser, MASK(TT_COLON));
      Token *kind = parser_expect_token(&parser, MASK(TT_IDENT));

      Value value = { str_to_value_kind(kind->lexeme), {0} };
      program_push_static_var(program, name->lexeme, value);
    } break;

    default: {
      ERROR("Unexpected token id\n");
      exit(1);
    }
    }

    if (parser_peek_token(&parser))
      parser_expect_token(&parser, MASK(TT_NEWLINE));
  }
}

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

  Tokens tokens;
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

  Program program = {0};
  compile(tokens, &program);

  program_optimize(&program, TargetPlatformLinux_X86_64);
  Str _asm = program_gen_code(&program, TargetPlatformLinux_X86_64);

  if (!silent_mode)
    printf("Assembly:\n"STR_FMT, STR_ARG(_asm));

  if (!write_file(argc[1], _asm)) {
    ERROR("Could not write to %s\n", argc[1]);
    exit(1);
  }

  return 0;
}
