#include <string.h>

#include "parser.h"
#include "../libs/lexgen-runtime/runtime.h"
#include "../grammar.h"
#include "shl_log.h"

static Str token_id_names[] = {
  STR_LIT("new line"),
  STR_LIT("whitespace"),
  STR_LIT("comment"),
  STR_LIT("`proc`"),
  STR_LIT("`jump`"),
  STR_LIT("`if`"),
  STR_LIT("`ret`"),
  STR_LIT("`alloc`"),
  STR_LIT("`include`"),
  STR_LIT("`static`"),
  STR_LIT("`asm`"),
  STR_LIT("`noret`"),
  STR_LIT("`init`"),
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

Token *parser_peek_token(Parser *parser) {
  if (parser->index >= parser->tokens.len)
    return NULL;

  return parser->tokens.items + parser->index;
}

Token *parser_next_token(Parser *parser) {
  Token *token = parser_peek_token(parser);
  ++parser->index;
  return token;
}

void print_id_mask(u32 id_mask, Str lexeme, FILE *stream) {
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

void expect_token(Token *token, u32 id_mask) {
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

Token *parser_expect_token(Parser *parser, u32 id_mask) {
  Token *token = parser_next_token(parser);
  expect_token(token, id_mask);
  return token;
}

ArgKind token_id_to_arg_kind(u32 id) {
  switch (id) {
  case TT_NUMBER: return ArgKindValue;
  case TT_IDENT:  return ArgKindVar;

  default: {
    ERROR("Wrong token id\n");
    exit(1);
  }
  }
}

Value str_to_value(Str str) {
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

Arg str_to_arg(Str text, ArgKind kind) {
  switch (kind) {
  case ArgKindValue: return arg_value(str_to_value(text));
  case ArgKindVar:   return arg_var(text);

  default: {
    ERROR("Wrong argument kind\n");
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


Arg parser_parse_arg(Parser *parser, Program *program,
                            u32 *static_segment_index) {
  Token *arg = parser_expect_token(parser, MASK(TT_NUMBER) |
                                           MASK(TT_IDENT) |
                                           MASK(TT_STR_LIT) |
                                           MASK(TT_CHAR_LIT));

  return token_to_arg(arg, program, static_segment_index);
}
