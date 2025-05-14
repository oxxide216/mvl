#ifndef LEXGEN_TRANSITION_TABLE
#define LEXGEN_TRANSITION_TABLE

#define TT_NEWLINE 0
#define TT_SKIP 1
#define TT_COMMENT 2
#define TT_PROC 3
#define TT_JUMP 4
#define TT_IF 5
#define TT_CALL 6
#define TT_RET 7
#define TT_ALLOC 8
#define TT_INCLUDE 9
#define TT_STATIC 10
#define TT_IDENT 11
#define TT_NUMBER 12
#define TT_OPAREN 13
#define TT_CPAREN 14
#define TT_COMMA 15
#define TT_AT 16
#define TT_COLON 17
#define TT_EQ 18
#define TT_NE 19
#define TT_GE 20
#define TT_LE 21
#define TT_GT 22
#define TT_LS 23
#define TT_PUT 24
#define TT_RIGHT_ARROW 25
#define TT_STR_LIT 26
#define TT_CHAR_LIT 27

#define TTS_COUNT 28

TransitionTable *get_transition_table(void);

#ifdef LEXGEN_TRANSITION_TABLE_IMPLEMENTATION

TransitionCol table_col_newline[] = {
  { 1, '\n', '\n', 0 },
};

TransitionCol table_col_skip[] = {
  { 1, ' ', ' ', 2 },
  { 1, '\t', '\t', 2 },
  { 1, '\r', '\r', 2 },
  { 2, ' ', ' ', 2 },
  { 2, '\t', '\t', 2 },
  { 2, '\r', '\r', 2 },
  { 2, -1, -1, 0 },
};

TransitionCol table_col_comment[] = {
  { 1, '#', '#', 0 },
};

TransitionCol table_col_proc[] = {
  { 1, 'p', 'p', 2 },
  { 2, 'r', 'r', 3 },
  { 3, 'o', 'o', 4 },
  { 4, 'c', 'c', 0 },
};

TransitionCol table_col_jump[] = {
  { 1, 'j', 'j', 2 },
  { 2, 'u', 'u', 3 },
  { 3, 'm', 'm', 4 },
  { 4, 'p', 'p', 0 },
};

TransitionCol table_col_if[] = {
  { 1, 'i', 'i', 2 },
  { 2, 'f', 'f', 0 },
};

TransitionCol table_col_call[] = {
  { 1, 'c', 'c', 2 },
  { 2, 'a', 'a', 3 },
  { 3, 'l', 'l', 4 },
  { 4, 'l', 'l', 0 },
};

TransitionCol table_col_ret[] = {
  { 1, 'r', 'r', 2 },
  { 2, 'e', 'e', 3 },
  { 3, 't', 't', 0 },
};

TransitionCol table_col_alloc[] = {
  { 1, 'a', 'a', 2 },
  { 2, 'l', 'l', 3 },
  { 3, 'l', 'l', 4 },
  { 4, 'o', 'o', 5 },
  { 5, 'c', 'c', 0 },
};

TransitionCol table_col_include[] = {
  { 1, 'i', 'i', 2 },
  { 2, 'n', 'n', 3 },
  { 3, 'c', 'c', 4 },
  { 4, 'l', 'l', 5 },
  { 5, 'u', 'u', 6 },
  { 6, 'd', 'd', 7 },
  { 7, 'e', 'e', 0 },
};

TransitionCol table_col_static[] = {
  { 1, 's', 's', 2 },
  { 2, 't', 't', 3 },
  { 3, 'a', 'a', 4 },
  { 4, 't', 't', 5 },
  { 5, 'i', 'i', 6 },
  { 6, 'c', 'c', 0 },
};

TransitionCol table_col_ident[] = {
  { 1, 'a', 'z', 2 },
  { 1, 'A', 'Z', 2 },
  { 1, '_', '_', 2 },
  { 2, 'a', 'z', 2 },
  { 2, 'A', 'Z', 2 },
  { 2, '0', '9', 2 },
  { 2, '_', '_', 2 },
  { 2, -1, -1, 0 },
};

TransitionCol table_col_number[] = {
  { 1, '0', '9', 2 },
  { 2, '0', '9', 2 },
  { 2, -1, -1, 0 },
};

TransitionCol table_col_oparen[] = {
  { 1, '(', '(', 0 },
};

TransitionCol table_col_cparen[] = {
  { 1, ')', ')', 0 },
};

TransitionCol table_col_comma[] = {
  { 1, ',', ',', 0 },
};

TransitionCol table_col_at[] = {
  { 1, '@', '@', 0 },
};

TransitionCol table_col_colon[] = {
  { 1, ':', ':', 0 },
};

TransitionCol table_col_eq[] = {
  { 1, '=', '=', 2 },
  { 2, '=', '=', 0 },
};

TransitionCol table_col_ne[] = {
  { 1, '!', '!', 2 },
  { 2, '=', '=', 0 },
};

TransitionCol table_col_ge[] = {
  { 1, '>', '>', 2 },
  { 2, '=', '=', 0 },
};

TransitionCol table_col_le[] = {
  { 1, '<', '<', 2 },
  { 2, '=', '=', 0 },
};

TransitionCol table_col_gt[] = {
  { 1, '>', '>', 0 },
};

TransitionCol table_col_ls[] = {
  { 1, '<', '<', 0 },
};

TransitionCol table_col_put[] = {
  { 1, '=', '=', 0 },
};

TransitionCol table_col_right_arrow[] = {
  { 1, '-', '-', 2 },
  { 2, '>', '>', 0 },
};

TransitionCol table_col_str_lit[] = {
  { 1, '"', '"', 0 },
};

TransitionCol table_col_char_lit[] = {
  { 1, '\'', '\'', 0 },
};

TransitionRow table_rows[] = {
  { table_col_newline, sizeof(table_col_newline) / sizeof(TransitionCol) },
  { table_col_skip, sizeof(table_col_skip) / sizeof(TransitionCol) },
  { table_col_comment, sizeof(table_col_comment) / sizeof(TransitionCol) },
  { table_col_proc, sizeof(table_col_proc) / sizeof(TransitionCol) },
  { table_col_jump, sizeof(table_col_jump) / sizeof(TransitionCol) },
  { table_col_if, sizeof(table_col_if) / sizeof(TransitionCol) },
  { table_col_call, sizeof(table_col_call) / sizeof(TransitionCol) },
  { table_col_ret, sizeof(table_col_ret) / sizeof(TransitionCol) },
  { table_col_alloc, sizeof(table_col_alloc) / sizeof(TransitionCol) },
  { table_col_include, sizeof(table_col_include) / sizeof(TransitionCol) },
  { table_col_static, sizeof(table_col_static) / sizeof(TransitionCol) },
  { table_col_ident, sizeof(table_col_ident) / sizeof(TransitionCol) },
  { table_col_number, sizeof(table_col_number) / sizeof(TransitionCol) },
  { table_col_oparen, sizeof(table_col_oparen) / sizeof(TransitionCol) },
  { table_col_cparen, sizeof(table_col_cparen) / sizeof(TransitionCol) },
  { table_col_comma, sizeof(table_col_comma) / sizeof(TransitionCol) },
  { table_col_at, sizeof(table_col_at) / sizeof(TransitionCol) },
  { table_col_colon, sizeof(table_col_colon) / sizeof(TransitionCol) },
  { table_col_eq, sizeof(table_col_eq) / sizeof(TransitionCol) },
  { table_col_ne, sizeof(table_col_ne) / sizeof(TransitionCol) },
  { table_col_ge, sizeof(table_col_ge) / sizeof(TransitionCol) },
  { table_col_le, sizeof(table_col_le) / sizeof(TransitionCol) },
  { table_col_gt, sizeof(table_col_gt) / sizeof(TransitionCol) },
  { table_col_ls, sizeof(table_col_ls) / sizeof(TransitionCol) },
  { table_col_put, sizeof(table_col_put) / sizeof(TransitionCol) },
  { table_col_right_arrow, sizeof(table_col_right_arrow) / sizeof(TransitionCol) },
  { table_col_str_lit, sizeof(table_col_str_lit) / sizeof(TransitionCol) },
  { table_col_char_lit, sizeof(table_col_char_lit) / sizeof(TransitionCol) },
};

TransitionTable table = {
  table_rows,
  sizeof(table_rows) / sizeof(TransitionRow),
};

TransitionTable *get_transition_table(void) {
  return &table;
};

#endif // LEXGEN_TRANSITION_TABLE_IMPLEMENTATION

#endif // LEXGEN_TRANSITION_TABLE
