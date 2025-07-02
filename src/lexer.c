#include <string.h>

#include "lexer.h"
#include "shl_log.h"
#include "shl_arena.h"
#include "../libs/lexgen-runtime/runtime.h"
#include "../grammar.h"

static char escape_char(char _char) {
  switch (_char) {
  case 'n': return '\n';
  case 'r': return '\r';
  case 't': return '\t';
  case 'v': return '\v';
  case '0': return '\0';
  case 'e': return '\033';
  default:  return _char;
  }
}

static Str escape_str(Str str) {
  Str new_str = {
    aalloc(str.len * sizeof(char)),
    str.len,
  };

  memcpy(new_str.ptr, str.ptr, str.len * sizeof(char));

  bool is_escaped = false;

  for (u32 i = 0; i < new_str.len; ++i) {
    if (is_escaped)
      new_str.ptr[i] = escape_char(new_str.ptr[i]);

    if (new_str.ptr[i] == '\\') {
      if (!is_escaped) {
        memmove(new_str.ptr + i, new_str.ptr + i + 1, new_str.len - i - 1);
        --new_str.len;
        --i;
        is_escaped = true;
      }
    } else {
      is_escaped = false;
    }
  }

  return new_str;
}

void lex(Str text, Tokens *tokens, Str file_path) {
  TransitionTable *table = get_transition_table();
  u32 row = 0, col = 0;

  while (text.len > 0) {
    u64 token_id = 0;
    Str lexeme = table_matches(table, &text, &token_id);

    if (token_id == (u64) -1) {
      if (text.len == 0)
              PERROR(STR_FMT":%u:%u: ", "unexpected EOF\n",
             STR_ARG(file_path), row + 1, col + 1);
      else
        PERROR(STR_FMT":%u:%u: ", "unexpected `%c`\n",
               STR_ARG(file_path), row + 1, col + 1, text.ptr[0]);
      exit(1);
    }

    Token new_token = { lexeme, token_id, row, col, file_path };

    col += lexeme.len;

    if (token_id == TT_NEWLINE) {
      ++row;
      col = 0;

      continue;
    }

    if (token_id == TT_WHITESPACE)
      continue;

    if (token_id == TT_COMMENT) {
      u32 i = 0;

      while (i < (u32) text.len) {
        if (text.ptr[i] == '\n') {
          ++row;
          col = 0;
          text.ptr += i;
          text.len -= i;
          break;
        }

        ++i;
      }

      if (i == (u32) text.len)
        text.len = 0;

      continue;
    }

    if (token_id == TT_STR_LIT) {
      u32 i = 0;
      bool is_escaped = false;

      while (i < (u32) text.len && (text.ptr[i] != '"' || is_escaped)) {
        ++col;

        if (text.ptr[i] == '\\')
          is_escaped = true;
        else
          is_escaped = false;

        if (text.ptr[i] == '\n') {
          ++row;
          col = 0;
        }

        ++i;
      }

      if (i == (u32) text.len) {
        PERROR(STR_FMT":%u:%u: ", "unclosed string literal\n",
               STR_ARG(file_path), new_token.row + 1, new_token.col + 1);
        exit(1);
      }

      text.ptr += i + 1;
      text.len -= i + 1;

      new_token.lexeme.ptr += 1;
      new_token.lexeme.len += i - 1;

      new_token.lexeme = escape_str(new_token.lexeme);
    }

    if (token_id == TT_CHAR_LIT) {
      if (text.len < 2) {
        PERROR(STR_FMT":%u:%u: ", "unclosed character literal",
               STR_ARG(file_path), new_token.row + 1, new_token.col + 1);
        exit(1);
      }

      if (text.ptr[0] == '\\') {
        if (text.len < 3) {
          PERROR(STR_FMT"%u:%u: ", "unclosed character literal",
                 STR_ARG(file_path), new_token.row + 1, new_token.col + 1);
          exit(1);
        }

        text.ptr[0] = escape_char(text.ptr[1]);

        text.ptr += 1;
        text.len -= 1;
      }

      text.ptr += 2;
      text.len -= 2;

      new_token.lexeme.ptr += 1;
    }

    DA_APPEND(*tokens, new_token);
  }
}
