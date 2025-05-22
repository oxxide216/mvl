#ifndef PARSER_H
#define PARSER_H

#include <stdio.h>

#include "../libs/mvm/mvm.h"
#include "shl_defs.h"
#include "lexer.h"

typedef struct {
  Tokens  tokens;
  u32     index;
} Parser;

Token   *parser_peek_token(Parser *parser);
Token   *parser_next_token(Parser *parser);
void     print_id_mask(u32 id_mask, Str lexeme, FILE *stream);
void     expect_token(Token *token, u64 id_mask);
Token   *parser_expect_token(Parser *parser, u64 id_mask);
ArgKind  token_id_to_arg_kind(u32 id);
ValueKind str_to_value_kind(Str str);
Value str_to_number_value(Str str);
Arg str_to_arg(Str text, ArgKind kind);

#endif // PARSER_H
