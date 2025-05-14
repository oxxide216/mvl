#include "runtime.h"

void tt_push_row(TransitionTable *tt, TransitionCol *cols, u32 cols_count) {
  TransitionRow new_row = { cols, cols_count };
  DA_APPEND(*tt, new_row);
}

static bool row_matches(TransitionRow *row, Str text, u32 *lexeme_len) {
  u32 state = 1;

  for (u32 i = 0; i <= (u32) text.len; ++i) {
    bool found = false;

    for (u32 j = 0; j < row->cols_count; ++j) {
      TransitionCol *col = row->cols + j;

      if (col->prev_state != state)
        continue;

      if (col->min_char != -1 && (i == (u32) text.len ||
                                  text.ptr[i] < col->min_char ||
                                  text.ptr[i] > col->max_char))
        continue;

      if (col->min_char == -1)
        --i;

      found = true;
      state = col->next_state;
      if (state == 0) {
        *lexeme_len = i + 1;
        return true;
      }

      break;
    }

    if (!found) {
      break;
    }
  }

  return false;
}

Str tt_matches(TransitionTable *tt, Str *text, u32 *token_id) {
  Str lexeme = { text->ptr, 0 };
  u32 longest_token_id = (u32) -1;

  for (u32 i = 0; i < tt->len; ++i) {
    u32 new_lexeme_len = 0;
    bool row_match = row_matches(tt->items + i, *text,
                                 &new_lexeme_len);

    if (row_match && new_lexeme_len > (u32) lexeme.len) {
      lexeme.len = new_lexeme_len;
      longest_token_id = i;
    }
  }

  if (longest_token_id != (u32) -1) {
    text->ptr += lexeme.len;
    text->len -= lexeme.len;
  }

  if (token_id)
    *token_id = longest_token_id;

  return lexeme;
}
