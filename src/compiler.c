#include <string.h>
#include <ctype.h>

#include "compiler.h"
#include "../libs/mvm/misc.h"
#include "../libs/lexgen-runtime/runtime.h"
#include "../grammar.h"
#include "shl_log.h"
#include "parser.h"

typedef Da(ValueKind) ParamKinds;

typedef struct {
  Str       var;
  ValueKind kind;
} VariableKind;

typedef struct {
  Da(VariableKind) kinds;
  Da(VariableKind) static_kinds;
} VariableKinds;

typedef struct {
  Str        name;
  ValueKind  ret_val_kind;
  ParamKinds param_kinds;
} DefinedProcedure;

typedef Da(DefinedProcedure) DefinedProcedures;

typedef struct {
  bool is_while;
  Str  begin_label_name;
  Str  end_label_name;
  u32  first_var_index;
} Block;

typedef Da(Block) Blocks;

typedef struct {
  Str       name;
  ValueKind kind;
} MacroParam;

typedef Da(MacroParam) MacroParams;

typedef struct {
  Str         name;
  MacroParams params;
  Tokens      tokens;
} Macro;

typedef Da(Macro) Macros;

typedef struct {
  Program            *program;
  Parser              parser;
  VariableKinds       var_kinds;
  DefinedProcedures   procs;
  Macros              macros;
  u32                 static_segments_count;
} Compiler;

static Str value_kind_to_str(ValueKind kind) {
  switch (kind) {
  case ValueKindUnit: return STR_LIT("unit");
  case ValueKindS64:  return STR_LIT("s64");
  case ValueKindS32:  return STR_LIT("s32");
  case ValueKindS16:  return STR_LIT("s16");
  case ValueKindS8:   return STR_LIT("s8");
  case ValueKindU64:  return STR_LIT("u64");
  case ValueKindU32:  return STR_LIT("u32");
  case ValueKindU16:  return STR_LIT("u16");
  case ValueKindU8:   return STR_LIT("u8");

  default: {
    ERROR("Wrong value kind\n");
    exit(1);
  }
  }
}

static u32 define_proc(Compiler *compiler, Str name,
                       ValueKind ret_val_kind,
                       ParamKinds param_kinds) {
  DefinedProcedure proc = { name, ret_val_kind, param_kinds };
  DA_APPEND(compiler->procs, proc);
  return compiler->procs.len - 1;
}

static void fprint_proc_sign(FILE *stream, Str name, ParamKinds param_kinds) {
  str_fprint(stream, name);
  for (u32 i = 0; i < param_kinds.len; ++i) {
    fputc(' ', stream);
    Str kind_str = value_kind_to_str(param_kinds.items[i]);
    str_fprint(stream, kind_str);
  }
}

static bool proc_sign_eq(Str a_name, ParamKinds a_param_kinds,
                         Str b_name, ParamKinds b_param_kinds) {
  if (!str_eq(a_name, b_name))
    return false;

  if (a_param_kinds.len != b_param_kinds.len)
    return false;

  for (u32 i = 0; i < a_param_kinds.len; ++i)
    if (a_param_kinds.items[i] != b_param_kinds.items[i])
      return false;

  return true;
}

static u32 get_proc_id(Compiler *compiler, Str name,
                       ParamKinds param_kinds,
                       u32 current_proc_id) {
  for (u32 i = 0; i < compiler->procs.len; ++i) {
    DefinedProcedure *proc = compiler->procs.items + i;
    if (proc_sign_eq(proc->name, proc->param_kinds, name, param_kinds))
      return i;
  }

  DefinedProcedure *current_proc = compiler->procs.items + current_proc_id;

  ERROR("");
  fprint_proc_sign(stderr, current_proc->name,
                   current_proc->param_kinds);
  fputs(": procedure `", stderr);
  fprint_proc_sign(stderr, name, param_kinds);
  fputs("` was not found\n", stderr);
  exit(1);
}

// djb2 hash function
static Str get_proc_hashed_name(Compiler *compiler, u32 id) {
  DefinedProcedure *proc = compiler->procs.items + id;
  u32 hash = 5381;

  for (u32 i = 0; i < proc->param_kinds.len; ++i)
    hash = ((hash << 5) + hash) + proc->param_kinds.items[i];

  StringBuilder sb = {0};
  sb_push_str(&sb, proc->name);
  sb_push_char(&sb, '@');
  sb_push_u32(&sb, hash);

  return sb_to_str(sb);
}

static ValueKind get_proc_ret_val_kind(Compiler *compiler, u32 id) {
  return compiler->procs.items[id].ret_val_kind;
}

static Arg token_to_arg(Token *token, Compiler *compiler) {
  if (token->id == TT_STR_LIT) {
    StringBuilder sb = {0};
    sb_push(&sb, "?s");
    sb_push_u32(&sb, compiler->static_segments_count++);
    Str name = sb_to_str(sb);

    u8 *bytes = aalloc(sizeof(token->lexeme.len) + 1);
    memcpy(bytes, token->lexeme.ptr, token->lexeme.len);
    bytes[token->lexeme.len] = '\0';
    program_push_static_segment(compiler->program, name, bytes,
                                token->lexeme.len + 1);

    VariableKind var_kind = { name, ValueKindS64 };
    DA_APPEND(compiler->var_kinds.kinds, var_kind);

    return arg_var(name);
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

ValueKind compiler_get_arg_kind(Compiler *compiler, Arg *arg, Procedure *current_proc) {
  switch (arg->kind) {
  case ArgKindValue: {
    return arg->as.value.kind;
  }

  case ArgKindVar: {
    for (u32 i = 0; i < compiler->var_kinds.kinds.len; ++i) {
      VariableKind *var_kind = compiler->var_kinds.kinds.items + i;
      if (str_eq(var_kind->var, arg->as.var))
        return var_kind->kind;
    }

    for (u32 i = 0; i < compiler->var_kinds.static_kinds.len; ++i) {
      VariableKind *var_kind = compiler->var_kinds.static_kinds.items + i;
      if (str_eq(var_kind->var, arg->as.var))
        return var_kind->kind;
    }

    ERROR(STR_FMT": variable `"STR_FMT"` was not defined before usage\n",
          STR_ARG(current_proc->name), STR_ARG(arg->as.var));
    exit(1);
  } break;

  default: {
    ERROR("Wrong argument kind\n");
    exit(1);
  }
  }
}

static Arg compile_arg(Compiler *compiler) {
  Token *arg = parser_expect_token(&compiler->parser, MASK(TT_IDENT) |
                                                      MASK(TT_NUMBER) |
                                                      MASK(TT_STR_LIT) |
                                                      MASK(TT_CHAR_LIT));

  return token_to_arg(arg, compiler);
}

static void compile_call(Compiler *compiler, Str dest,
                         Procedure *current_proc, u32 caller_id) {
  Token *callee_name_token = parser_expect_token(&compiler->parser, MASK(TT_IDENT));
  Args args = {0};
  ParamKinds param_kinds = {0};

  while (parser_peek_token(&compiler->parser) &&
         parser_peek_token(&compiler->parser)->id != TT_NEWLINE) {
    Arg arg = compile_arg(compiler);
    args_push_arg(&args, arg);

    ValueKind param_kind = compiler_get_arg_kind(compiler, &arg, current_proc);
    DA_APPEND(param_kinds, param_kind);
  }

  u32 callee_id = get_proc_id(compiler, callee_name_token->lexeme,
                              param_kinds, caller_id);
  Str callee_name = get_proc_hashed_name(compiler, callee_id);

  if (dest.len > 0) {
    ValueKind param_kind = get_proc_ret_val_kind(compiler, callee_id);
    VariableKind var_kind = { dest, param_kind };
    DA_APPEND(compiler->var_kinds.kinds, var_kind);

    proc_call_assign(current_proc, dest, callee_name, args);
  } else {
    proc_call(current_proc, callee_name, args);
  }
}

bool macro_sign_eq(Macro *macro, Str name, ParamKinds param_kinds) {
  if (!str_eq(macro->name, name))
    return false;

  if (macro->params.len != param_kinds.len)
    return false;

  for (u32 i = 0; i < macro->params.len; ++i) {
    ValueKind macro_param_kind = macro->params.items[i].kind;
    ValueKind param_kind = param_kinds.items[i];

    if (macro_param_kind != param_kind)
      return false;
  }

  return true;
}

void collect_defs(Compiler *compiler) {
  define_proc(compiler, STR_LIT("init"), ValueKindUnit, (ParamKinds) {0});

  while (parser_peek_token(&compiler->parser)) {
    Token *token = parser_next_token(&compiler->parser);

    if (token->id == TT_PROC) {
      Token *name_token = parser_expect_token(&compiler->parser, MASK(TT_IDENT) |
                                                                 MASK(TT_NAKED));

      if (name_token->id == TT_NAKED)
        name_token = parser_expect_token(&compiler->parser, MASK(TT_IDENT));

      ValueKind ret_val_kind = ValueKindUnit;
      ParamKinds param_kinds = {0};

      Token *next = parser_peek_token(&compiler->parser);
      while (next && next->id != TT_NEWLINE && next->id != TT_RIGHT_ARROW) {
        parser_expect_token(&compiler->parser, MASK(TT_IDENT));
        parser_expect_token(&compiler->parser, MASK(TT_COLON));
        Token *param_kind_token = parser_expect_token(&compiler->parser, MASK(TT_IDENT));

        ValueKind param_kind = str_to_value_kind(param_kind_token->lexeme);
        DA_APPEND(param_kinds, param_kind);

        next = parser_peek_token(&compiler->parser);
      }

      if (next && next->id == TT_RIGHT_ARROW) {
        parser_next_token(&compiler->parser);
        Token *ret_val_kind_token = parser_expect_token(&compiler->parser, MASK(TT_IDENT));
        ret_val_kind = str_to_value_kind(ret_val_kind_token->lexeme);
      }

      define_proc(compiler, name_token->lexeme, ret_val_kind, param_kinds);
    } else if (token->id == TT_STATIC) {
      Token *name_token = parser_expect_token(&compiler->parser, MASK(TT_IDENT));
      parser_expect_token(&compiler->parser, MASK(TT_PUT));
      Token *value_token = parser_expect_token(&compiler->parser, MASK(TT_NUMBER));

      ValueKind value_kind = str_to_number_value(value_token->lexeme).kind;
      VariableKind var_kind = { name_token->lexeme, value_kind };
      DA_APPEND(compiler->var_kinds.static_kinds, var_kind);
    } else if (token->id == TT_MACRO) {
      Macro macro = {0};

      Token *name = parser_expect_token(&compiler->parser, MASK(TT_IDENT));
      macro.name = name->lexeme;

      Token *next = parser_peek_token(&compiler->parser);
      while (next && next->id != TT_NEWLINE && next->id != TT_RIGHT_ARROW) {
        Token *param_name_token = parser_expect_token(&compiler->parser, MASK(TT_IDENT));
        parser_expect_token(&compiler->parser, MASK(TT_COLON));
        Token *param_kind_token = parser_expect_token(&compiler->parser, MASK(TT_IDENT));

        ValueKind param_kind = str_to_value_kind(param_kind_token->lexeme);
        MacroParam param = { param_name_token->lexeme, param_kind };
        DA_APPEND(macro.params, param);

        next = parser_peek_token(&compiler->parser);
      }

      parser_next_token(&compiler->parser);

      if (next->id == TT_RIGHT_ARROW) {
        Token *ret_val_kind_token = parser_expect_token(&compiler->parser, MASK(TT_IDENT));

        ValueKind param_kind = str_to_value_kind(ret_val_kind_token->lexeme);
        MacroParam param = { STR_LIT("@ret_val"), param_kind };
        DA_APPEND(macro.params, param);

        parser_expect_token(&compiler->parser, MASK(TT_NEWLINE));
      }

      ParamKinds param_kinds = {0};
      param_kinds.items = malloc(macro.params.len * sizeof(ValueKind));
      param_kinds.len = macro.params.len;

      for (u32 i = 0; i < param_kinds.len; ++i)
        param_kinds.items[i] = macro.params.items[i].kind;

      for (u32 i = 0; i < compiler->macros.len; ++i) {
        Macro *temp_macro = compiler->macros.items + i;
        if (macro_sign_eq(temp_macro, macro.name, param_kinds)) {
          ERROR("Macro `"STR_FMT"` was redefined\n",
                STR_ARG(name->lexeme));
          exit(1);
        }
      }

      u32 recursion_level = 1;

      while (parser_peek_token(&compiler->parser)) {
        Token *token = parser_next_token(&compiler->parser);

        if (token->id == TT_IF || token->id == TT_WHILE) {
          ++recursion_level;
        } else if (token->id == TT_END) {
          if (--recursion_level == 0)
            break;
        }

        DA_APPEND(macro.tokens, *token);
      }

      if (recursion_level > 0) {
        ERROR("Unclosed `"STR_FMT"` macro definition\n",
              STR_ARG(name->lexeme));
        exit(1);
      }

      DA_APPEND(compiler->macros, macro);
    }
  }

  compiler->parser.index = 0;
}

Macro *get_macro(Macros *macros, Str name, ParamKinds param_kinds) {
  for (u32 i = 0; i < macros->len; ++i)
    if (macro_sign_eq(macros->items + i, name, param_kinds))
      return macros->items + i;

  ERROR("`"STR_FMT"` macro was not found\n",
        STR_ARG(name));
  exit(1);
}

void expand_macro(Macro *macro, Parser *parser, u32 prev_index, Tokens *args) {
  Tokens *tokens = &parser->tokens;
  u32 index = parser->index;
  u32 inserted_tokens_len = macro->tokens.len + prev_index - index;

  if (tokens->len + inserted_tokens_len >= tokens->cap) {
    u32 new_cap;

    if (tokens->cap == 0) {
      new_cap = macro->tokens.len;

      tokens->items = malloc(new_cap * sizeof(Token));
    } else {
      new_cap = 1;
      while (tokens->len + inserted_tokens_len >= new_cap)
        new_cap *= 2;

      tokens->items = realloc(tokens->items, new_cap * sizeof(Token));
    }

    tokens->cap = new_cap;
  }

  memmove(tokens->items + prev_index + macro->tokens.len,
          tokens->items + index,
          (tokens->len - index) * sizeof(Token));

  memcpy(tokens->items + prev_index,
         macro->tokens.items,
         macro->tokens.len * sizeof(Token));

  tokens->len += inserted_tokens_len;
  parser->index = prev_index;

  for (u32 i = 0; i < macro->tokens.len; ++i) {
    Token *token = tokens->items + prev_index + i;

    if (token->id == TT_IDENT) {
      for (u32 j = 0; j < macro->params.len; ++j) {
        if (str_eq(token->lexeme, macro->params.items[j].name)) {
          *token = args->items[j];
          break;
        }
      }
    }
  }
}

void compile_macro_call(Compiler *compiler, Token *dest,
                        u32 prev_index, Procedure *current_proc) {
  Token *name = parser_expect_token(&compiler->parser, MASK(TT_IDENT));
  Args args = {0};
  ParamKinds param_kinds = {0};
  u32 arg_tokens_begin_index = compiler->parser.index;

  Token *next = parser_peek_token(&compiler->parser);
  while (next && next->id != TT_NEWLINE) {
    Arg arg = compile_arg(compiler);
    ValueKind param_kind = compiler_get_arg_kind(compiler, &arg, current_proc);
    DA_APPEND(param_kinds, param_kind);

    next = parser_peek_token(&compiler->parser);
  }

  Token *arg_tokens_begin = compiler->parser.tokens.items + arg_tokens_begin_index;
  u32 arg_tokens_len = compiler->parser.index - arg_tokens_begin_index;
  Tokens arg_tokens = { arg_tokens_begin, arg_tokens_len, 0 };

  parser_next_token(&compiler->parser);

  if (dest)
    DA_APPEND(arg_tokens, *dest);

  Macro *macro = get_macro(&compiler->macros, name->lexeme, param_kinds);
  expand_macro(macro, &compiler->parser, prev_index, &arg_tokens);
}

void compile(Tokens tokens, Program *program) {
  Compiler compiler = {0};
  compiler.program = program;
  compiler.parser.tokens = tokens;

  program_push_proc(program, STR_LIT("@init"),
                    ValueKindUnit, (ProcParams) {0},
                    false);
  define_proc(&compiler, STR_LIT("@init"),
              ValueKindUnit, (ParamKinds) {0});

  Procedure *current_proc = NULL;
  u32 current_proc_id = 0;

  u32 labels_count = 0;
  Blocks blocks = {0};

  collect_defs(&compiler);

  while (parser_peek_token(&compiler.parser)) {
    Token *token = parser_expect_token(&compiler.parser, MASK(TT_NEWLINE) |
                                                         MASK(TT_PROC) |
                                                         MASK(TT_IF) |
                                                         MASK(TT_ELSE) |
                                                         MASK(TT_WHILE) |
                                                         MASK(TT_END) |
                                                         MASK(TT_BREAK) |
                                                         MASK(TT_CONTINUE) |
                                                         MASK(TT_RET) |
                                                         MASK(TT_IDENT) |
                                                         MASK(TT_AT) |
                                                         MASK(TT_INCLUDE) |
                                                         MASK(TT_STATIC) |
                                                         MASK(TT_ASM) |
                                                         MASK(TT_INIT) |
                                                         MASK(TT_DEREF) |
                                                         MASK(TT_MACRO) |
                                                         MASK(TT_MACRO_CALL));

    if (!current_proc &&
        token->id != TT_PROC &&
        token->id != TT_NEWLINE &&
        token->id != TT_INCLUDE &&
        token->id != TT_STATIC &&
        token->id != TT_INIT &&
        token->id != TT_MACRO) {
      ERROR("Every instruction should be inside of a procedure\n");
      exit(1);
    }

    bool expect_new_line = true;

    switch (token->id) {
    case TT_NEWLINE: {
      continue;
    }

    case TT_PROC: {
      if (blocks.len > 0) {
        ERROR(STR_FMT": block was not closed\n", STR_ARG(current_proc->name));
        exit(1);
      }

      Token *name_token = parser_expect_token(&compiler.parser, MASK(TT_IDENT) |
                                                                MASK(TT_NAKED));

      bool is_naked = name_token->id == TT_NAKED;
      if (is_naked)
        name_token = parser_expect_token(&compiler.parser, MASK(TT_IDENT));

      ValueKind ret_val_kind = ValueKindUnit;
      ProcParams params = {0};
      ParamKinds param_kinds = {0};

      compiler.var_kinds.kinds.len = 0;

      Token *next = parser_peek_token(&compiler.parser);
      while (next && next->id != TT_NEWLINE && next->id != TT_RIGHT_ARROW) {
        Token *param_name_token = parser_expect_token(&compiler.parser, MASK(TT_IDENT));
        parser_expect_token(&compiler.parser, MASK(TT_COLON));
        Token *param_kind_token = parser_expect_token(&compiler.parser, MASK(TT_IDENT));

        ValueKind param_kind = str_to_value_kind(param_kind_token->lexeme);
        DA_APPEND(param_kinds, param_kind);

        VariableKind var_kind = { param_name_token->lexeme, param_kind };
        DA_APPEND(compiler.var_kinds.kinds, var_kind);

        ProcParam proc_param = { param_name_token->lexeme, param_kind };
        DA_APPEND(params, proc_param);

        next = parser_peek_token(&compiler.parser);
      }

      if (next && next->id == TT_RIGHT_ARROW) {
        parser_next_token(&compiler.parser);
        Token *ret_val_kind_token = parser_expect_token(&compiler.parser, MASK(TT_IDENT));
        ret_val_kind = str_to_value_kind(ret_val_kind_token->lexeme);
      }

      current_proc_id = define_proc(&compiler, name_token->lexeme,
                                    ret_val_kind, param_kinds);
      Str name = get_proc_hashed_name(&compiler, current_proc_id);
      current_proc = program_push_proc(program, name, ret_val_kind, params, is_naked);
    } break;

    case TT_IF:
    case TT_WHILE: {
      Arg arg0 = compile_arg(&compiler);

      Token *op = parser_expect_token(&compiler.parser, MASK(TT_EQ) | MASK(TT_NE) |
                                                        MASK(TT_GT) | MASK(TT_LS) |
                                                        MASK(TT_GE) | MASK(TT_LE));

      Arg arg1 = compile_arg(&compiler);

      RelOp rel_op;

      switch (op->id) {
      case TT_EQ: rel_op = RelOpNotEqual; break;
      case TT_NE: rel_op = RelOpEqual; break;
      case TT_GT: rel_op = RelOpLessOrEqual; break;
      case TT_LS: rel_op = RelOpGreaterOrEqual; break;
      case TT_GE: rel_op = RelOpLess; break;
      case TT_LE: rel_op = RelOpGreater; break;

      default: {
        ERROR("Unreachable\n");
        exit(1);
      }
      }

      bool is_while = token->id == TT_WHILE;

      Str begin_label_name = {0};
      if (is_while) {
        StringBuilder begin_label_sb = {0};
        sb_push(&begin_label_sb, "label");
        sb_push_u32(&begin_label_sb, labels_count++);
        begin_label_name = sb_to_str(begin_label_sb);

        proc_add_label(current_proc, begin_label_name);
      }

      StringBuilder end_label_sb = {0};
      sb_push(&end_label_sb, "label");
      sb_push_u32(&end_label_sb, labels_count++);
      Str end_label_name = sb_to_str(end_label_sb);

      Block new_block = {
        is_while,
        begin_label_name,
        end_label_name,
        compiler.var_kinds.kinds.len,
      };
      DA_APPEND(blocks, new_block);

      proc_cond_jump(current_proc, rel_op, arg0, arg1, end_label_name);
    } break;

    case TT_ELSE: {
      if (blocks.len == 0 || blocks.items[blocks.len - 1].is_while) {
        ERROR(STR_FMT": `else` not inside of `if`\n", STR_ARG(current_proc->name));
        exit(1);
      }

      Block *block = blocks.items + blocks.len - 1;

      compiler.var_kinds.kinds.len = block->first_var_index;

      StringBuilder new_end_label_sb = {0};
      sb_push(&new_end_label_sb, "label");
      sb_push_u32(&new_end_label_sb, labels_count++);
      Str new_end_label_name = sb_to_str(new_end_label_sb);

      proc_jump(current_proc, new_end_label_name);
      proc_add_label(current_proc, block->end_label_name);

      block->end_label_name = new_end_label_name;
    } break;

    case TT_END: {
      if (blocks.len == 0) {
        ERROR(STR_FMT": `end` not inside of a block\n", STR_ARG(current_proc->name));
        exit(1);
      }

      Block *block = blocks.items + --blocks.len;

      compiler.var_kinds.kinds.len = block->first_var_index;

      if (block->is_while)
        proc_jump(current_proc, block->begin_label_name);
      proc_add_label(current_proc, block->end_label_name);
    } break;

    case TT_BREAK:
    case TT_CONTINUE: {
      bool found_while = false;

      for (u32 i = blocks.len; i > 0; --i) {
        if (blocks.items[i - 1].is_while) {
          found_while = true;
          if (token->id == TT_BREAK)
            proc_jump(current_proc, blocks.items[i - 1].end_label_name);
          else
            proc_jump(current_proc, blocks.items[i - 1].begin_label_name);
          break;
        }
      }

      if (!found_while) {
        if (token->id == TT_BREAK)
          ERROR(STR_FMT": `break` not inside of a loop\n", STR_ARG(current_proc->name));
        else
          ERROR(STR_FMT": `continue` not inside of a loop\n", STR_ARG(current_proc->name));
        exit(1);
      }
    } break;

    case TT_RET: {
      Token *next = parser_peek_token(&compiler.parser);
      if (!next || next->id == TT_NEWLINE) {
        proc_return(current_proc);
      } else {
        Arg arg = compile_arg(&compiler);
        proc_return_value(current_proc, arg);
      }
    } break;


    case TT_IDENT: {
      u32 prev_index = compiler.parser.index - 1;

      Token *next = parser_expect_token(&compiler.parser, MASK(TT_PUT) |
                                                          MASK(TT_COLON));
      if (next->id == TT_COLON) {
        proc_add_label(current_proc, token->lexeme);
        break;
      }

      next = parser_peek_token(&compiler.parser);

      if (next && next->id == TT_AT) {
        parser_next_token(&compiler.parser);
        compile_call(&compiler, token->lexeme, current_proc, current_proc_id);
        break;
      }

      if (next && next->id == TT_REF) {
        VariableKind var_kind = { token->lexeme, ValueKindU64 };
        DA_APPEND(compiler.var_kinds.kinds, var_kind);

        parser_next_token(&compiler.parser);
        Token *src_name = parser_expect_token(&compiler.parser, MASK(TT_IDENT));
        proc_ref(current_proc, token->lexeme, src_name->lexeme);
        break;
      }

      if (next && next->id == TT_DEREF) {
        parser_next_token(&compiler.parser);
        Token *value_kind_name = parser_expect_token(&compiler.parser, MASK(TT_IDENT));
        Token *ref_name = parser_expect_token(&compiler.parser, MASK(TT_IDENT));

        ValueKind kind = str_to_value_kind(value_kind_name->lexeme);

        VariableKind var_kind = { token->lexeme, kind };
        DA_APPEND(compiler.var_kinds.kinds, var_kind);

        proc_deref(current_proc, token->lexeme, kind, ref_name->lexeme);
        break;
      }

      if (next && next->id == TT_CAST) {
        parser_next_token(&compiler.parser);
        Token *value_kind_name = parser_expect_token(&compiler.parser, MASK(TT_IDENT));
        Token *arg_var_name = parser_expect_token(&compiler.parser, MASK(TT_IDENT));

        ValueKind kind = str_to_value_kind(value_kind_name->lexeme);

        VariableKind var_kind = { token->lexeme, kind };
        DA_APPEND(compiler.var_kinds.kinds, var_kind);

        proc_cast(current_proc, token->lexeme, kind, arg_var_name->lexeme);
        break;
      }

      if (next && next->id == TT_MACRO_CALL) {
        parser_next_token(&compiler.parser);
        compile_macro_call(&compiler, token, prev_index, current_proc);
        expect_new_line = false;

        break;
      }

      Arg arg = compile_arg(&compiler);

      ValueKind param_kind = compiler_get_arg_kind(&compiler, &arg, current_proc);
      VariableKind var_kind = { token->lexeme, param_kind };
      DA_APPEND(compiler.var_kinds.kinds, var_kind);

      proc_assign(current_proc, token->lexeme, arg);
    } break;

    case TT_AT: {
      compile_call(&compiler, (Str) {0}, current_proc, current_proc_id);
    } break;

    case TT_INCLUDE: {
      parser_expect_token(&compiler.parser, MASK(TT_STR_LIT));
    } break;

    case TT_STATIC: {
      Token *name_token = parser_expect_token(&compiler.parser, MASK(TT_IDENT));
      parser_expect_token(&compiler.parser, MASK(TT_PUT));
      Token *value_token = parser_expect_token(&compiler.parser, MASK(TT_NUMBER));

      Value value = str_to_number_value(value_token->lexeme);
      program_push_static_var(program, name_token->lexeme, value, false);
    } break;

    case TT_ASM: {
      Token *text_token = parser_expect_token(&compiler.parser, MASK(TT_STR_LIT));

      Da(Arg) args = {0};
      while (parser_peek_token(&compiler.parser) &&
             parser_peek_token(&compiler.parser)->id != TT_NEWLINE) {
        Arg arg = compile_arg(&compiler);
        DA_APPEND(args, arg);
      }

      InlineAsmSegments segments = {0};

      u32 arg_index = 0;
      Str text = { text_token->lexeme.ptr, 0 };
      for (u32 i = 0; i < text_token->lexeme.len; ++i) {
        if (text_token->lexeme.ptr[i] == '@') {
          if (arg_index >= args.len) {
            ERROR(STR_FMT": not enough arguments in inline assembly\n",
                  STR_ARG(current_proc->name));
            exit(1);
          }

          if (text.len > 0)
            segments_push_text(&segments, text);

          bool is_high_priority_var = false;
          if (i + 1 < text_token->lexeme.len &&
              text_token->lexeme.ptr[i + 1] == '@') {
            is_high_priority_var = true;
            ++i;
          }

          segments_push_arg(&segments, args.items[arg_index++],
                            is_high_priority_var);

          text.ptr = text_token->lexeme.ptr + i + 1;
          text.len = 0;
        } else {
          ++text.len;
        }
      }

      if (text.len > 0)
        segments_push_text(&segments, text);

      proc_inline_asm(current_proc, segments);
    } break;

    case TT_INIT: {
      current_proc = program->procs;
    } break;

    case TT_DEREF: {
      Token *dest_token = parser_expect_token(&compiler.parser, MASK(TT_IDENT));
      parser_expect_token(&compiler.parser, MASK(TT_PUT));
      Arg arg = compile_arg(&compiler);
      proc_ref_assign(current_proc, dest_token->lexeme, arg);
    } break;

    case TT_MACRO: {
      Token *name = parser_expect_token(&compiler.parser, MASK(TT_IDENT));

      Token *next = parser_peek_token(&compiler.parser);
      while (next && next->id != TT_NEWLINE && next->id != TT_RIGHT_ARROW) {
        parser_expect_token(&compiler.parser, MASK(TT_IDENT));
        parser_expect_token(&compiler.parser, MASK(TT_COLON));
        parser_expect_token(&compiler.parser, MASK(TT_IDENT));

        next = parser_peek_token(&compiler.parser);
      }

      parser_next_token(&compiler.parser);

      if (next->id == TT_RIGHT_ARROW) {
        parser_expect_token(&compiler.parser, MASK(TT_IDENT));
        parser_expect_token(&compiler.parser, MASK(TT_NEWLINE));
      }

      u32 recursion_level = 1;

      while (parser_peek_token(&compiler.parser)) {
        Token *token = parser_next_token(&compiler.parser);

        if (token->id == TT_IF || token->id == TT_WHILE) {
          ++recursion_level;
        } else if (token->id == TT_END) {
          if (--recursion_level == 0)
            break;
        }
      }

      if (recursion_level > 0) {
        ERROR("Unclosed `"STR_FMT"` macro definition\n",
              STR_ARG(name->lexeme));
        exit(1);
      }
    } break;

    case TT_MACRO_CALL: {
      u32 prev_index = compiler.parser.index - 1;
      compile_macro_call(&compiler, NULL, prev_index, current_proc);
      expect_new_line = false;
    } break;

    default: {
      ERROR("Unexpected token id\n");
      exit(1);
    }
    }

    if (expect_new_line && parser_peek_token(&compiler.parser))
      parser_expect_token(&compiler.parser, MASK(TT_NEWLINE));
  }

  if (program->procs->next) {
    if (program->procs->instrs) {
      program->procs->instrs_end->next = program->procs->next->instrs;
      program->procs->next->instrs = program->procs->instrs;
    }
    program->procs = program->procs->next;
  }
}
