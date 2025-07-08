#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"
#include "ir.h"

void    expect_token(Token *token, u64 id_mask);
IrProcs parse(Tokens tokens);

#endif // PARSER_H
