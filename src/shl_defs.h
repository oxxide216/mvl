#ifndef SHL_DEFS_H
#define SHL_DEFS_H

#include <stdlib.h>
#include <stdbool.h>

#ifndef SHL_DEFS_DA_ALLOC
#define SHL_DEFS_DA_ALLOC malloc
#endif

#ifndef SHL_DEFS_DA_REALLOC
#define SHL_DEFS_DA_REALLOC realloc
#endif

#ifndef SHL_DEFS_LL_ALLOC
#define SHL_DEFS_LL_ALLOC malloc
#endif

#define ARRAY_LEN(array) (sizeof(array) / sizeof(array[0]))

#define Da(type)  \
  struct {        \
    type *items;  \
    u32 len, cap; \
  }

#define DA_APPEND(da, element)                                                    \
  do {                                                                            \
    if ((da).cap <= (da).len) {                                                   \
      if ((da).cap != 0) {                                                        \
        while ((da).cap <= (da).len)                                              \
          (da).cap *= 2;                                                          \
        (da).items = SHL_DEFS_DA_REALLOC((da).items, sizeof(element) * (da).cap); \
      } else {                                                                    \
        (da).cap = 1;                                                             \
        (da).items = SHL_DEFS_DA_ALLOC(sizeof(element));                          \
      }                                                                           \
    }                                                                             \
    (da).items[(da).len++] = element;                                             \
  } while (0)

#define DA_REMOVE(da) (da).items[--(da).len]

#define DA_REMOVE_AT(da, index)                                 \
  do {                                                          \
    if ((index) < (da).len + 1) {                               \
        memmove((da).items + (index), (da).items + (index) + 1, \
                sizeof(*(da).items) * (da).len);                \
      --(da).len;                                               \
    }                                                           \
  } while (0)

#define LL_LEN(ll, type, target) \
  do {                           \
    *target = 0;                 \
    type *next = ll;             \
    while (next) {               \
      ++*target;                 \
      next = next->next;         \
    }                            \
  } while (0)

#define LL_APPEND(ll, type)                      \
  do {                                           \
    type *new = SHL_DEFS_LL_ALLOC(sizeof(type)); \
    new->next = ll;                              \
    ll = new;                                    \
  } while(0)

#define LL_PREPEND(ll, ll_end, type)             \
  do {                                           \
    type *new = SHL_DEFS_LL_ALLOC(sizeof(type)); \
    if (ll_end)                                  \
      (ll_end)->next = new;                      \
    else                                         \
      ll = new;                                  \
                                      \
    ll_end = new;                     \
  } while(0)

typedef char           i8;
typedef unsigned char  u8;
typedef short          i16;
typedef unsigned short u16;
typedef int            i32;
typedef unsigned int   u32;
typedef long           i64;
typedef unsigned long  u64;

typedef float  f32;
typedef double f64;

#endif // SHL_DEFS_H
