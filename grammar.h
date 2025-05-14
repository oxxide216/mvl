#ifndef LEXGEN_TRANSITION_TABLE
#define LEXGEN_TRANSITION_TABLE

#define TT_NEWLINE 0
TransitionCol tt_col_newline[] = {
  { 1, '\n', '\n', 0 },
};

#define TT_SKIP 1
TransitionCol tt_col_skip[] = {
  { 1, ' ', ' ', 2 },
  { 1, '\t', '\t', 2 },
  { 1, '\r', '\r', 2 },
  { 2, ' ', ' ', 2 },
  { 2, '\t', '\t', 2 },
  { 2, '\r', '\r', 2 },
  { 2, -1, -1, 0 },
};

#define TT_COMMENT 2
TransitionCol tt_col_comment[] = {
  { 1, '#', '#', 0 },
};

#define TT_PROC 3
TransitionCol tt_col_proc[] = {
  { 1, 'p', 'p', 2 },
  { 2, 'r', 'r', 3 },
  { 3, 'o', 'o', 4 },
  { 4, 'c', 'c', 0 },
};

#define TT_JUMP 4
TransitionCol tt_col_jump[] = {
  { 1, 'j', 'j', 2 },
  { 2, 'u', 'u', 3 },
  { 3, 'm', 'm', 4 },
  { 4, 'p', 'p', 0 },
};

#define TT_IF 5
TransitionCol tt_col_if[] = {
  { 1, 'i', 'i', 2 },
  { 2, 'f', 'f', 0 },
};

#define TT_CALL 6
TransitionCol tt_col_call[] = {
  { 1, 'c', 'c', 2 },
  { 2, 'a', 'a', 3 },
  { 3, 'l', 'l', 4 },
  { 4, 'l', 'l', 0 },
};

#define TT_RET 7
TransitionCol tt_col_ret[] = {
  { 1, 'r', 'r', 2 },
  { 2, 'e', 'e', 3 },
  { 3, 't', 't', 0 },
};

#define TT_ALLOC 8
TransitionCol tt_col_alloc[] = {
  { 1, 'a', 'a', 2 },
  { 2, 'l', 'l', 3 },
  { 3, 'l', 'l', 4 },
  { 4, 'o', 'o', 5 },
  { 5, 'c', 'c', 0 },
};

#define TT_INCLUDE 9
TransitionCol tt_col_include[] = {
  { 1, 'i', 'i', 2 },
  { 2, 'n', 'n', 3 },
  { 3, 'c', 'c', 4 },
  { 4, 'l', 'l', 5 },
  { 5, 'u', 'u', 6 },
  { 6, 'd', 'd', 7 },
  { 7, 'e', 'e', 0 },
};

#define TT_STATIC 10
TransitionCol tt_col_static[] = {
  { 1, 's', 's', 2 },
  { 2, 't', 't', 3 },
  { 3, 'a', 'a', 4 },
  { 4, 't', 't', 5 },
  { 5, 'i', 'i', 6 },
  { 6, 'c', 'c', 0 },
};

#define TT_IDENT 11
TransitionCol tt_col_ident[] = {
  { 1, 'a', 'z', 2 },
  { 1, 'A', 'Z', 2 },
  { 1, '_', '_', 2 },
  { 2, 'a', 'z', 2 },
  { 2, 'A', 'Z', 2 },
  { 2, '0', '9', 2 },
  { 2, '_', '_', 2 },
  { 2, -1, -1, 0 },
};

#define TT_NUMBER 12
TransitionCol tt_col_number[] = {
  { 1, '0', '9', 2 },
  { 2, '0', '9', 2 },
  { 2, -1, -1, 0 },
};

#define TT_OPAREN 13
TransitionCol tt_col_oparen[] = {
  { 1, '(', '(', 0 },
};

#define TT_CPAREN 14
TransitionCol tt_col_cparen[] = {
  { 1, ')', ')', 0 },
};

#define TT_COMMA 15
TransitionCol tt_col_comma[] = {
  { 1, ',', ',', 0 },
};

#define TT_AT 16
TransitionCol tt_col_at[] = {
  { 1, '@', '@', 0 },
};

#define TT_COLON 17
TransitionCol tt_col_colon[] = {
  { 1, ':', ':', 0 },
};

#define TT_EQ 18
TransitionCol tt_col_eq[] = {
  { 1, '=', '=', 2 },
  { 2, '=', '=', 0 },
};

#define TT_NE 19
TransitionCol tt_col_ne[] = {
  { 1, '!', '!', 2 },
  { 2, '=', '=', 0 },
};

#define TT_GE 20
TransitionCol tt_col_ge[] = {
  { 1, '>', '>', 2 },
  { 2, '=', '=', 0 },
};

#define TT_LE 21
TransitionCol tt_col_le[] = {
  { 1, '<', '<', 2 },
  { 2, '=', '=', 0 },
};

#define TT_GT 22
TransitionCol tt_col_gt[] = {
  { 1, '>', '>', 0 },
};

#define TT_LS 23
TransitionCol tt_col_ls[] = {
  { 1, '<', '<', 0 },
};

#define TT_PUT 24
TransitionCol tt_col_put[] = {
  { 1, '=', '=', 0 },
};

#define TT_RIGHT_ARROW 25
TransitionCol tt_col_right_arrow[] = {
  { 1, '-', '-', 2 },
  { 2, '>', '>', 0 },
};

#define TT_STR_LIT 26
TransitionCol tt_col_str_lit[] = {
  { 1, '"', '"', 0 },
};

#define TT_CHAR_LIT 27
TransitionCol tt_col_char_lit[] = {
  { 1, '\'', '\'', 0 },
};

#define TTS_COUNT 28
TransitionRow tt[] = {
  { tt_col_newline, sizeof(tt_col_newline) / sizeof(TransitionCol) },
  { tt_col_skip, sizeof(tt_col_skip) / sizeof(TransitionCol) },
  { tt_col_comment, sizeof(tt_col_comment) / sizeof(TransitionCol) },
  { tt_col_proc, sizeof(tt_col_proc) / sizeof(TransitionCol) },
  { tt_col_jump, sizeof(tt_col_jump) / sizeof(TransitionCol) },
  { tt_col_if, sizeof(tt_col_if) / sizeof(TransitionCol) },
  { tt_col_call, sizeof(tt_col_call) / sizeof(TransitionCol) },
  { tt_col_ret, sizeof(tt_col_ret) / sizeof(TransitionCol) },
  { tt_col_alloc, sizeof(tt_col_alloc) / sizeof(TransitionCol) },
  { tt_col_include, sizeof(tt_col_include) / sizeof(TransitionCol) },
  { tt_col_static, sizeof(tt_col_static) / sizeof(TransitionCol) },
  { tt_col_ident, sizeof(tt_col_ident) / sizeof(TransitionCol) },
  { tt_col_number, sizeof(tt_col_number) / sizeof(TransitionCol) },
  { tt_col_oparen, sizeof(tt_col_oparen) / sizeof(TransitionCol) },
  { tt_col_cparen, sizeof(tt_col_cparen) / sizeof(TransitionCol) },
  { tt_col_comma, sizeof(tt_col_comma) / sizeof(TransitionCol) },
  { tt_col_at, sizeof(tt_col_at) / sizeof(TransitionCol) },
  { tt_col_colon, sizeof(tt_col_colon) / sizeof(TransitionCol) },
  { tt_col_eq, sizeof(tt_col_eq) / sizeof(TransitionCol) },
  { tt_col_ne, sizeof(tt_col_ne) / sizeof(TransitionCol) },
  { tt_col_ge, sizeof(tt_col_ge) / sizeof(TransitionCol) },
  { tt_col_le, sizeof(tt_col_le) / sizeof(TransitionCol) },
  { tt_col_gt, sizeof(tt_col_gt) / sizeof(TransitionCol) },
  { tt_col_ls, sizeof(tt_col_ls) / sizeof(TransitionCol) },
  { tt_col_put, sizeof(tt_col_put) / sizeof(TransitionCol) },
  { tt_col_right_arrow, sizeof(tt_col_right_arrow) / sizeof(TransitionCol) },
  { tt_col_str_lit, sizeof(tt_col_str_lit) / sizeof(TransitionCol) },
  { tt_col_char_lit, sizeof(tt_col_char_lit) / sizeof(TransitionCol) },
};

#endif // LEXGEN_TRANSITION_TABLE
