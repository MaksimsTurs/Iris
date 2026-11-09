#ifndef H_IRIS_MEM
#define H_IRIS_MEM

#include "iris/typedef.h"

#include <stdlib.h>
#include <string.h>

#define Iris_ArrayNew($array, $cap) do {                          \
  ($array).buff = malloc(((u64_t)$cap) * sizeof(($array).buff[0])); \
  ($array).cap = $cap;                                            \
  ($array).len = 0;                                               \
} while(0)
#define Iris_ArrayExpand($array, $cap) do {                                         \
  ($array).buff = realloc(($array).buff, (((u64_t)$cap) * sizeof(($array).buff[0]))); \
  ($array).cap = $cap;                                                              \
} while(0)
#define Iris_ArrayDestroy($array) do { \
  free(($array).buff);                 \
  ($array).buff = null;                \
  ($array).cap = 0;                    \
  ($array).len = 0;                    \
} while(0)
#define Iris_ArrayLen($array)         (sizeof(($array)) / sizeof(($array[0])));
#define Iris_ArrayHasCap($array)      ((($array).len + 1) <= ($array).cap)
#define Iris_ArrayAt($array, $index)  ((($index < 0) || ($index >= ($array).len)) ? null : &(($array).buff[$index]))
#define Iris_ArrayAtBack($array)      (&(($array).buff[($array).len == 0 ? 0 : ($array).len - 1]))
#define Iris_ArrayPush($array, $data) do { ($array).buff[($array).len++] = $data; } while(0)

typedef struct Iris_Arena
{
  ptr_t buff;
  i64_t allocated;
  i64_t used;

  struct Iris_Arena* next;
  struct Iris_Arena* tail;
} Iris_Arena;

#define Iris_ArenaHasEnoughMem($arena, $new_size) (($arena)->allocated > ($arena)->used + ($new_size))
#define Iris_ArenaFindWithSize($tmp, $size) do {                           \
  while($tmp)                                                              \
  {                                                                        \
    if(($tmp)->allocated > ($tmp)->used + ($size)) break;                  \
    else if(($tmp)->next != null)                  ($tmp) = ($tmp)->next;  \
    else                                           ($tmp) = null;          \
  }                                                                        \
} while(0)
#define Iris_ArenaNew($arena, $size) do {                            \
  ($arena) = (Iris_Arena*)malloc(sizeof(Iris_Arena) + (u64_t)$size); \
  if(($arena))                                                      \
  {                                                                  \
    ($arena)->buff = ($arena) + 1;                                   \
    ($arena)->tail = ($arena);                                       \
    ($arena)->allocated = 1000;                                     \
    ($arena)->used = 0;                                              \
    ($arena)->next = null;                                           \
  }                                                                  \
} while(0)
#define Iris_ArenaAlloc($arena, $ptr, $size) do { \
  Iris_Arena* $tmp = ($arena);                    \
  if(!Iris_ArenaHasEnoughMem($tmp, $size))        \
  {                                               \
    Iris_ArenaFindWithSize($tmp, $size);          \
    if(!$tmp)                                     \
    {                                             \
      Iris_ArenaNew($tmp, ($arena)->allocated);   \
      ($arena)->tail->next = $tmp;                \
      ($arena)->tail = $tmp;                      \
    }                                             \
  }                                               \
  ($ptr) = ($tmp)->buff + ($tmp)->used;           \
  ($tmp)->used += $size;                          \
} while(0)
void_t      Iris_ArenaFree(Iris_Arena* arena);

#endif
