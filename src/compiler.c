#include <string.h>
#include <ctype.h>

#include "compiler.h"
#include "../libs/lexgen-runtime/runtime.h"
#include "../grammar.h"
#include "shl_log.h"
#include "parser.h"

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

void compile(Tokens tokens, Program *program) {
  Parser parser = { tokens, 0 };
  Procedure *proc = NULL;
  u32 static_segment_index = 0;

  while (parser_peek_token(&parser)) {
    Token *token = parser_expect_token(&parser, MASK(TT_NEWLINE) |
                                                MASK(TT_PROC) |
                                                MASK(TT_JUMP) |
                                                MASK(TT_IF) |
                                                MASK(TT_RET) |
                                                MASK(TT_IDENT) |
                                                MASK(TT_AT) |
                                                MASK(TT_INCLUDE) |
                                                MASK(TT_STATIC) |
                                                MASK(TT_ASM));

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

        ProcParam proc_param = {
          param_name->lexeme,
          str_to_value(param_kind->lexeme).kind,
        };
        DA_APPEND(params, proc_param);

        next = parser_peek_token(&parser);
      }

      if (next->id == TT_RIGHT_ARROW) {
        parser_next_token(&parser);
        Token *ret_val_kind_token = parser_expect_token(&parser, MASK(TT_IDENT));
        ret_val_kind = str_to_value(ret_val_kind_token->lexeme).kind;
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

      next = parser_expect_token(&parser, MASK(TT_IDENT) |
                                          MASK(TT_NUMBER) |
                                          MASK(TT_AT) |
                                          MASK(TT_ALLOC) |
                                          MASK(TT_STR_LIT) |
                                          MASK(TT_CHAR_LIT));

      if (next->id == TT_AT) {
        compile_call(&parser, program, proc,
                     token->lexeme, &static_segment_index);
        break;
      }

      if (next->id == TT_ALLOC) {
        Token *size = parser_expect_token(&parser, MASK(TT_NUMBER));
        proc_alloc(proc, token->lexeme, (u32) str_to_i32(size->lexeme));
        break;
      }

      Arg arg = token_to_arg(next, program, &static_segment_index);
      proc_assign(proc, token->lexeme, arg);
    } break;

    case TT_AT: {
      compile_call(&parser, program, proc, token->lexeme, &static_segment_index);
    } break;

    case TT_INCLUDE: {
      parser_expect_token(&parser, MASK(TT_STR_LIT));
    } break;

    case TT_STATIC: {
      Token *name = parser_expect_token(&parser, MASK(TT_IDENT));
      parser_expect_token(&parser, MASK(TT_COLON));
      Token *kind = parser_expect_token(&parser, MASK(TT_IDENT));

      Value value = { str_to_value(kind->lexeme).kind, {0} };
      program_push_static_var(program, name->lexeme, value);
    } break;

    case TT_ASM: {
      Token *text = parser_expect_token(&parser, MASK(TT_STR_LIT) |
                                                 MASK(TT_NORET));

      bool no_return = text->id == TT_NORET;

      if (no_return)
        text = parser_expect_token(&parser, MASK(TT_STR_LIT));

      Args args = {0};
      while (parser_peek_token(&parser) &&
             parser_peek_token(&parser)->id != TT_NEWLINE) {
        Arg arg = parser_parse_arg(&parser, program, &static_segment_index);
        args_push_arg(&args, arg);
      }

      proc_inline_asm(proc, text->lexeme, args, no_return);
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
