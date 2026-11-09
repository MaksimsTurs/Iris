#include "iris/mem.h"

void_t Iris_ArenaFree(Iris_Arena* arena)
{
  if(arena->next)
  {
    Iris_ArenaFree(arena->next);
  }

  arena->allocated = 0;
  arena->used = 0;
  arena->next = null;
  arena->tail = null;
  free(arena);
}
