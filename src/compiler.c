#include <string.h>
#include <ctype.h>

#include "compiler.h"
#include "../libs/mvm/misc.h"
#include "../libs/lexgen-runtime/runtime.h"
#include "../grammar.h"
#include "shl_log.h"
#include "parser.h"

typedef struct {
  Program   *program;
  Parser     parser;
  u32        static_segments_count;
} Compiler;

static Arg token_to_arg(Token *token, Compiler *compiler) {
  if (token->id == TT_STR_LIT) {
    StringBuilder sb = {0};
    sb_push(&sb, "?s");
    sb_push_u32(&sb, compiler->static_segments_count++);

    u8 *bytes = aalloc(sizeof(token->lexeme.len) + 1);
    memcpy(bytes, token->lexeme.ptr, token->lexeme.len);
    bytes[token->lexeme.len] = '\0';
    program_push_static_segment(compiler->program, sb_to_str(sb),
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

static Arg compile_arg(Compiler *compiler) {
  Token *arg = parser_expect_token(&compiler->parser, MASK(TT_IDENT) |
                                                      MASK(TT_NUMBER) |
                                                      MASK(TT_NUMBER_TYPED) |
                                                      MASK(TT_STR_LIT) |
                                                      MASK(TT_CHAR_LIT));

  return token_to_arg(arg, compiler);
}

static void compile_call(Compiler *compiler, Procedure *proc, Str dest) {
  Token *callee = parser_expect_token(&compiler->parser, MASK(TT_IDENT));
  Args args = {0};

  while (parser_peek_token(&compiler->parser) &&
         parser_peek_token(&compiler->parser)->id != TT_NEWLINE) {
    Arg arg = compile_arg(compiler);
    args_push_arg(&args, arg);
  }

  if (dest.len > 0)
    proc_call_assign(proc, dest, callee->lexeme, args);
  else
    proc_call(proc, callee->lexeme, args);
}

void compile(Tokens tokens, Program *program) {
  Compiler compiler = {0};
  compiler.program = program;
  compiler.parser.tokens = tokens;

  Procedure *proc = program_push_proc(program, STR_LIT("init"),
                                      ValueKindUnit, (ProcParams) {0});

  while (parser_peek_token(&compiler.parser)) {
    Token *token = parser_expect_token(&compiler.parser, MASK(TT_NEWLINE) |
                                                         MASK(TT_PROC) |
                                                         MASK(TT_JUMP) |
                                                         MASK(TT_IF) |
                                                         MASK(TT_RET) |
                                                         MASK(TT_IDENT) |
                                                         MASK(TT_AT) |
                                                         MASK(TT_INCLUDE) |
                                                         MASK(TT_STATIC) |
                                                         MASK(TT_ASM) |
                                                         MASK(TT_INIT) |
                                                         MASK(TT_DEREF));

    if (token->id != TT_PROC &&
        token->id != TT_NEWLINE &&
        token->id != TT_INCLUDE &&
        token->id != TT_STATIC &&
        token->id != TT_INIT &&
        proc == NULL) {
      ERROR("Every instruction should be inside of a procedure\n");
      exit(1);
    }

    switch (token->id) {
    case TT_NEWLINE: {
      continue;
    }

    case TT_PROC: {
      Token *name = parser_expect_token(&compiler.parser, MASK(TT_IDENT));
      ValueKind ret_val_kind = ValueKindUnit;
      ProcParams params = {0};

      Token *next = parser_peek_token(&compiler.parser);
      while (next && next->id != TT_NEWLINE && next->id != TT_RIGHT_ARROW) {
        Token *param_name = parser_expect_token(&compiler.parser, MASK(TT_IDENT));
        parser_expect_token(&compiler.parser, MASK(TT_COLON));
        Token *param_kind = parser_expect_token(&compiler.parser, MASK(TT_IDENT));

        ProcParam proc_param = {
          param_name->lexeme,
          str_to_value(param_kind->lexeme).kind,
        };
        DA_APPEND(params, proc_param);

        next = parser_peek_token(&compiler.parser);
      }

      if (next && next->id == TT_RIGHT_ARROW) {
        parser_next_token(&compiler.parser);
        Token *ret_val_kind_token = parser_expect_token(&compiler.parser, MASK(TT_IDENT));
        ret_val_kind = str_to_value(ret_val_kind_token->lexeme).kind;
      }

      proc = program_push_proc(program, name->lexeme, ret_val_kind, params);
    } break;

    case TT_JUMP: {
      Token *label_name = parser_expect_token(&compiler.parser, MASK(TT_IDENT));
      proc_jump(proc, label_name->lexeme);
    } break;

    case TT_IF: {
      Arg arg0 = compile_arg(&compiler);

      Token *op = parser_expect_token(&compiler.parser, MASK(TT_EQ) | MASK(TT_NE) |
                                                        MASK(TT_GT) | MASK(TT_LS) |
                                                        MASK(TT_GE) | MASK(TT_LE));

      Arg arg1 = compile_arg(&compiler);

      parser_expect_token(&compiler.parser, MASK(TT_JUMP));

      Token *label_name = parser_expect_token(&compiler.parser, MASK(TT_IDENT));

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

    case TT_RET: {
      Token *next = parser_peek_token(&compiler.parser);
      if (!next || next->id == TT_NEWLINE) {
        proc_return(proc);
      } else {
        Arg arg = compile_arg(&compiler);
        proc_return_value(proc, arg);
      }
    } break;


    case TT_IDENT: {
      Token *next = parser_expect_token(&compiler.parser, MASK(TT_PUT) |
                                                          MASK(TT_COLON));
      if (next->id == TT_COLON) {
        proc_add_label(proc, token->lexeme);
        break;
      }

      next = parser_peek_token(&compiler.parser);

      if (next && next->id == TT_AT) {
        compile_call(&compiler, proc, token->lexeme);
        break;
      }

      if (next && next->id == TT_REF) {
        parser_next_token(&compiler.parser);
        Token *src_name = parser_expect_token(&compiler.parser, MASK(TT_IDENT));
        proc_ref(proc, token->lexeme, src_name->lexeme);
        break;
      }

      if (next && next->id == TT_DEREF) {
        parser_next_token(&compiler.parser);
        Token *value_kind_name = parser_expect_token(&compiler.parser, MASK(TT_IDENT));
        Token *ref_name = parser_expect_token(&compiler.parser, MASK(TT_IDENT));

        ValueKind kind = str_to_value_kind(value_kind_name->lexeme);
        proc_deref(proc, token->lexeme, kind, ref_name->lexeme);
        break;
      }

      Arg arg = compile_arg(&compiler);
      proc_assign(proc, token->lexeme, arg);
    } break;

    case TT_AT: {
      compile_call(&compiler, proc, (Str) {0});
    } break;

    case TT_INCLUDE: {
      parser_expect_token(&compiler.parser, MASK(TT_STR_LIT));
    } break;

    case TT_STATIC: {
      Token *name = parser_expect_token(&compiler.parser, MASK(TT_IDENT));
      parser_expect_token(&compiler.parser, MASK(TT_COLON));
      Token *kind = parser_expect_token(&compiler.parser, MASK(TT_IDENT));

      Value value = { str_to_value(kind->lexeme).kind, {0} };
      program_push_static_var(program, name->lexeme, value);
    } break;

    case TT_ASM: {
      Token *text = parser_expect_token(&compiler.parser, MASK(TT_STR_LIT) |
                                                          MASK(TT_NORET));

      bool no_return = text->id == TT_NORET;

      if (no_return)
        text = parser_expect_token(&compiler.parser, MASK(TT_STR_LIT));

      Args args = {0};
      while (parser_peek_token(&compiler.parser) &&
             parser_peek_token(&compiler.parser)->id != TT_NEWLINE) {
        Arg arg = compile_arg(&compiler);
        args_push_arg(&args, arg);
      }

      proc_inline_asm(proc, text->lexeme, args, no_return);
    } break;

    case TT_INIT: {
      proc = program->procs;
    } break;

    case TT_DEREF: {
      Token *dest_token = parser_expect_token(&compiler.parser, MASK(TT_IDENT));
      parser_expect_token(&compiler.parser, MASK(TT_PUT));
      Arg arg = compile_arg(&compiler);
      proc_ref_assign(proc, dest_token->lexeme, arg);
    } break;

    default: {
      ERROR("Unexpected token id\n");
      exit(1);
    }
    }

    if (parser_peek_token(&compiler.parser))
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
