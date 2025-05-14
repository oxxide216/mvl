#ifndef LEXER_H
#define LEXER_H

#include "shl_defs.h"
#include "shl_str.h"

#define MASK(index) (1 << (index))

typedef struct {
  Str lexeme;
  u32 id;
  u32 row, col;
  Str file_path;
} Token;

typedef Da(Token) Tokens;

void lex(Str text, Tokens *tokens, Str file_path);

#endif // LEXER_H
