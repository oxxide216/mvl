#include <string.h>
#include <ctype.h>

#include "parser.h"
#include "../libs/mvm/misc.h"
#include "../libs/lexgen-runtime/runtime.h"
#include "../grammar.h"
#include "shl_log.h"

static Str token_id_names[] = {
  STR_LIT("new line"),
  STR_LIT("whitespace"),
  STR_LIT("comment"),
  STR_LIT("string literal"),
  STR_LIT("character literal"),
  STR_LIT("`proc`"),
  STR_LIT("`if`"),
  STR_LIT("`else`"),
  STR_LIT("`while`"),
  STR_LIT("`end`"),
  STR_LIT("`break`"),
  STR_LIT("`continue`"),
  STR_LIT("`ret`"),
  STR_LIT("`include`"),
  STR_LIT("`static`"),
  STR_LIT("`asm`"),
  STR_LIT("`naked`"),
  STR_LIT("`init`"),
  STR_LIT("`cast`"),
  STR_LIT("`macro`"),
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
  STR_LIT("'&'"),
  STR_LIT("`*`"),
  STR_LIT("`!`"),
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

void print_id_mask(u64 id_mask, Str lexeme, FILE *stream) {
  u32 len = ARRAY_LEN(token_id_names);
  u32 max_matched_ids_count = 0;

  for (u32 i = 0; i < len; ++i)
    if (id_mask & MASK(i))
      ++max_matched_ids_count;

  u32 matched_ids_count = 0;
  for (u32 i = 0; i < len; ++i) {
    if (!(id_mask & MASK(i)))
      continue;

    if (max_matched_ids_count > 1) {
      if (matched_ids_count + 1 == max_matched_ids_count)
        fputs(" or ", stream);
      else if (matched_ids_count > 0)
        fputs(", ", stream);
    }

    if ((i == TT_NUMBER || i == TT_IDENT) && lexeme.len != 0) {
      fputc('`', stream);
      str_fprint(stream, lexeme);
      fputc('`', stream);
    } else {
      str_fprint(stream, token_id_names[i]);
    }

    ++matched_ids_count;
  }
}

void expect_token(Token *token, u64 id_mask) {
  if (!token) {
    ERROR("Expected ");
    print_id_mask(id_mask, (Str) {0}, stderr);
    fputs(", but got EOF\n", stderr);
    exit(1);
  }

  if (token && id_mask & ((u64) 1 << token->id))
    return;

  PERROR(STR_FMT":%u:%u: ", "Expected ",
         STR_ARG(token->file_path),
         token->row + 1, token->col + 1);
  print_id_mask(id_mask, (Str) {0}, stderr);
  fputs(", but got ", stderr);
  print_id_mask(MASK(token->id), token->lexeme, stderr);
  fputc('\n', stderr);

  exit(1);
}

Token *parser_expect_token(Parser *parser, u64 id_mask) {
  Token *token = parser_next_token(parser);
  expect_token(token, id_mask);
  return token;
}

ArgKind token_id_to_arg_kind(u64 id) {
  switch (id) {
  case TT_NUMBER:       return ArgKindValue;
  case TT_IDENT:        return ArgKindVar;

  default: {
    ERROR("Wrong token id\n");
    exit(1);
  }
  }
}

ValueKind str_to_value_kind(Str str) {
  if (str_eq(str, STR_LIT("unit")))
    return ValueKindUnit;

  if (str_eq(str, STR_LIT("s64")))
    return ValueKindS64;

  if (str_eq(str, STR_LIT("s32")))
    return ValueKindS32;

  if (str_eq(str, STR_LIT("s16")))
    return ValueKindS16;

  if (str_eq(str, STR_LIT("s8")))
    return ValueKindS8;

  if (str_eq(str, STR_LIT("u64")))
    return ValueKindU64;

  if (str_eq(str, STR_LIT("u32")))
    return ValueKindU32;

  if (str_eq(str, STR_LIT("u16")))
    return ValueKindU16;

  if (str_eq(str, STR_LIT("u8")))
    return ValueKindU8;

  ERROR("Unknown type name: "STR_FMT"\n", STR_ARG(str));
  exit(1);
}

Value str_to_number_value(Str str) {
  i64 number = 0;
  bool is_neg = false;
  u32 i = 0;

  if (str.len > 0 && str.ptr[0] == '-') {
    is_neg = true;
    ++i;
  }

  while (i < (u32) str.len && isdigit(str.ptr[i])) {
    number *= 10;
    number += str.ptr[i] - '0';
    ++i;
  }

  if (is_neg)
    number *= -1;

  if (i < (u32) str.len) {
    str.ptr += i;
    str.len -= i;

    ValueKind kind = str_to_value_kind(str);

    switch (kind) {
    case ValueKindS64: return (Value) { kind, { .s64 = number } };
    case ValueKindS32: return (Value) { kind, { .s32 = number } };
    case ValueKindS16: return (Value) { kind, { .s16 = number } };
    case ValueKindS8:  return (Value) { kind, { .s8 = number } };
    case ValueKindU64: return (Value) { kind, { .u64 = number } };
    case ValueKindU32: return (Value) { kind, { .u32 = number } };
    case ValueKindU16: return (Value) { kind, { .u16 = number } };
    case ValueKindU8:  return (Value) { kind, { .u8 = number } };

    default: {
      ERROR("Unknown type name: "STR_FMT"\n", STR_ARG(str));
      exit(1);
    }
    }
  }

  return (Value) {
    ValueKindS64,
    { .s64 = number },
  };
}

Arg str_to_arg(Str text, ArgKind kind) {
  switch (kind) {
  case ArgKindValue: return arg_value(str_to_number_value(text));
  case ArgKindVar:   return arg_var(text);

  default: {
    ERROR("Wrong argument kind\n");
    exit(1);
  }
  }
}
