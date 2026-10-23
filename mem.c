#include "mem.h"
#include "typedef.h"
#include <stdlib.h>

Iris_MemBuffer* Iris_MemBufferAlloc(u64_t cap, u64_t size)
{
  void_t* buff = malloc(cap * size + sizeof(Iris_MemBuffer));
  Iris_MemBuffer* mem = (Iris_MemBuffer*)(buff + (cap * size));

  mem->buff = buff;
  mem->cap = cap;
  mem->len = 0;

  return mem;
}

void_t Iris_MemBufferDealloc(Iris_MemBuffer* mem)
{
  free(mem->buff);
  mem->buff = NULL;
  mem->cap = 0;
  mem->len = 0;
}
