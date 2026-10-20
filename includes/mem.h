#ifndef H_IRIS_MEM
#define H_IRIS_MEM

#include "typedef.h"

#include <stdlib.h>

typedef struct {
  i8_t* buff;
  u64_t cap;
  u64_t len;
} Iris_MemBuffer;

Iris_MemBuffer* Iris_MemBufferAlloc(u64_t cap, u64_t size);
void_t          Iris_MemBufferDealloc(Iris_MemBuffer* mem);

#endif
