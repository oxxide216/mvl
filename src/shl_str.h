#ifndef SHL_STR_H
#define SHL_STR_H

#include <stdio.h>
#include <stdbool.h>

#define STR(ptr, len) ((Str) { ptr, len })
#define STR_LIT(ptr) ((Str) { ptr, sizeof(ptr) - 1 })
#define STR_FMT "%.*s"
#define STR_ARG(str) (str).len, (str).ptr

typedef struct {
  char         *ptr;
  unsigned int  len;
} Str;

typedef struct {
  char         *buffer;
  unsigned int  cap;
  unsigned int  len;
} StringBuilder;

Str str_new(char *str);
bool str_eq(Str a, Str b);
void str_fprint(FILE *stream, Str str);
void str_fprintln(FILE *stream, Str str);
void str_print(Str str);
void str_println(Str str);
int str_to_i32(Str str);
long int str_to_i64(Str str);
unsigned int str_to_u32(Str str);
unsigned long int str_to_u64(Str str);
unsigned long int str_hash(Str str);

Str sb_to_str(StringBuilder sb);
void sb_push(StringBuilder *sb, char *str);
void sb_push_char(StringBuilder *sb, char ch);
void sb_push_str(StringBuilder *sb, Str str);
void sb_push_i8(StringBuilder *sb, char num);
void sb_push_i16(StringBuilder *sb, short int num);
void sb_push_i32(StringBuilder *sb, int num);
void sb_push_i64(StringBuilder *sb, long int num);
void sb_push_u8(StringBuilder *sb, unsigned char num);
void sb_push_u16(StringBuilder *sb, unsigned short int num);
void sb_push_u32(StringBuilder *sb, unsigned int num);
void sb_push_u64(StringBuilder *sb, unsigned long int num);

#ifdef SHL_STR_IMPLEMENTATION

#include <string.h>
#include <stdlib.h>

Str str_new(char *str) {
  return (Str) { .ptr = str, .len = strlen(str) };
}

bool str_eq(Str a, Str b) {
  if (a.len != b.len)
    return false;

  for (int i = 0; i < (int) a.len; ++i)
    if (a.ptr[i] != b.ptr[i])
      return false;

  return true;
}

void str_fprint(FILE *stream, Str str) {
  for (int i = 0; i < (int) str.len; ++i)
    putc(str.ptr[i], stream);
}

void str_fprintln(FILE *stream, Str str) {
  str_fprint(stream, str);
  putc('\n', stream);
}

void str_print(Str str) {
  str_fprint(stdout, str);
}

void str_println(Str str) {
  str_fprintln(stdout, str);
}

int str_to_i32(Str str) {
  return (int) str_to_i64(str);
}

long int str_to_i64(Str str) {
  long int num = 0;

  if (str.len == 0)
    return 0;

  bool is_neg = str.ptr[0] == '-';
  if (is_neg) {
    ++str.ptr;
    --str.len;
  }

  for (int i = 0; i < (int) str.len; ++i) {
    num *= 10;
    num += str.ptr[i] - '0';
  }

  if (is_neg)
    return -num;
  return num;
}

unsigned int str_to_u32(Str str) {
  return (unsigned int) str_to_u64(str);
}

unsigned long int str_to_u64(Str str) {
  unsigned long int num = 0;

  for (int i = 0; i < (int) str.len; ++i) {
    num *= 10;
    num += str.ptr[i] - '0';
  }

  return num;
}

// sdbm hash function
unsigned long int str_hash(Str str) {
  unsigned long int result = 0;

  for (int i = 0; i < (int) str.len; ++i) {
    result = str.ptr[i] + (result << 6) + (result << 16) - result;
  }

  return result;
}

void sb_reserve_space(StringBuilder *sb, unsigned int amount) {
  if (amount > sb->cap - sb->len) {
    if (sb->cap != 0) {
      while (amount > sb->cap - sb->len)
        sb->cap *= 2;

      sb->buffer = realloc(sb->buffer, sb->cap + 1);
    } else {
      sb->cap += amount;
      sb->buffer = malloc(sb->cap + 1);
    }
  }
}

Str sb_to_str(StringBuilder sb) {
  return (Str) {
    .ptr = sb.buffer,
    .len = sb.len,
  };
}

void sb_push_char(StringBuilder *sb, char ch) {
  sb_reserve_space(sb, 1);
  sb->buffer[sb->len++] = ch;
}

void sb_push(StringBuilder *sb, char *str) {
  sb_push_str(sb, str_new(str));
}

void sb_push_str(StringBuilder *sb, Str str) {
  sb_reserve_space(sb, str.len);
  memmove(sb->buffer + sb->len, str.ptr, str.len);
  sb->len += str.len;
}

void sb_push_i8(StringBuilder *sb, char num) {
  sb_push_i64(sb, num);
}

void sb_push_i16(StringBuilder *sb, short int num) {
  sb_push_i64(sb, num);
}

void sb_push_i32(StringBuilder *sb, int num) {
  sb_push_i64(sb, num);
}

void sb_push_i64(StringBuilder *sb, long int num) {
  long int _num = num;
  unsigned int len = 1;

  if (_num < 0) {
    _num *= -1;
    ++len;
  }

  while (_num >= 10) {
    _num /= 10;
    ++len;
  }

  sb_reserve_space(sb, len);
  snprintf(sb->buffer + sb->len, len + 1, "%ld", num);
  sb->len += len;
}

void sb_push_u8(StringBuilder *sb, unsigned char num) {
  sb_push_u64(sb, num);
}

void sb_push_u16(StringBuilder *sb, unsigned short int num) {
  sb_push_u64(sb, num);
}

void sb_push_u32(StringBuilder *sb, unsigned int num) {
  sb_push_u64(sb, num);
}

void sb_push_u64(StringBuilder *sb, unsigned long int num) {
  unsigned long int _num = num;
  unsigned int len = 1;

  while (_num >= 10) {
    _num /= 10;
    ++len;
  }

  sb_reserve_space(sb, len);
  snprintf(sb->buffer + sb->len, len + 1, "%lu", num);
  sb->len += len;
}

#endif // SHL_STR_IMPLEMENTATION

#endif // SHL_STR_H
