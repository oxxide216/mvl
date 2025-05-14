#include "shl_defs.h"
#include "shl_str.h"

typedef struct {
  u32 prev_state;
  i8  min_char;
  i8  max_char;
  u32 next_state;
} TransitionCol;

typedef struct {
  TransitionCol *cols;
  u32            cols_count;
} TransitionRow;

typedef Da(TransitionRow) TransitionTable;

void tt_push_row(TransitionTable *tt, TransitionCol *cols, u32 cols_count);
Str  tt_matches(TransitionTable *tt, Str *text, u32 *lexeme_len);
