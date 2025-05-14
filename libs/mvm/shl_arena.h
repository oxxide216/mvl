#ifndef SHL_ARENA_H
#define SHL_ARENA_H

void *aalloc(unsigned int amount);

#ifdef SHL_ARENA_IMPLEMENTATION

#include <stdlib.h>

#define DEFAULT_ARENA_CAP 256

typedef struct Arena Arena;

struct Arena {
  char         *buffer;
  unsigned int  cap, len;
  Arena        *next;
};

Arena *arena = NULL;
Arena *last_arena = NULL;

Arena *create_arena(unsigned int cap) {
  Arena *_arena = malloc(sizeof(Arena) + sizeof(char) * cap);
  _arena->buffer = (char *) _arena + sizeof(Arena);
  _arena->cap = cap;
  _arena->len = 0;
  _arena->next = NULL;
  last_arena = _arena;
  return _arena;
}

void *aalloc(unsigned int amount) {
  if (amount > DEFAULT_ARENA_CAP)
    arena = create_arena(amount);
  else
    arena = create_arena(DEFAULT_ARENA_CAP);

  Arena *_arena = arena;

  while (_arena->len + amount > _arena->cap) {
    if (!_arena->next) {
      unsigned int new_arena_cap = DEFAULT_ARENA_CAP;
      if (amount > new_arena_cap)
        new_arena_cap = amount;
      _arena->next = create_arena(new_arena_cap);
    }
    _arena = _arena->next;
  }

  unsigned int len = _arena->len;
  _arena->len += amount;
  return _arena->buffer + len;
}

#endif // SHL_ARENA_IMPLEMENTATION

#endif // SHL_ARENA_H
