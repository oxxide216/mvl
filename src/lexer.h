#ifndef LEXER_H
#define LEXER_H

#include "shl/shl-defs.h"
#include "shl/shl-str.h"

#define MASK(index) ((u64) 1 << (index))

typedef struct {
  Str lexeme;
  u64 id;
  u32 row, col;
  Str file_path;
} Token;

typedef Da(Token) Tokens;

void lex(Str text, Tokens *tokens, Str file_path);

#endif // LEXER_H
