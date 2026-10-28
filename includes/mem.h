#ifndef H_IRIS_MEM
#define H_IRIS_MEM

#include <stdlib.h>
#include <string.h>

#include "typedef.h"

typedef struct {
  ptr_t buff;
  i64_t cap;
  i64_t len;

  i64_t _size_of_el;
} Iris_ArrBuff;

Iris_ArrBuff* Iris_ArrBuffAlloc(i64_t cap, i64_t size);
Iris_ArrBuff* Iris_ArrBuffReAlloc(Iris_ArrBuff* mem, i64_t new_cap);
void_t        Iris_ArrBuffDealloc(Iris_ArrBuff* mem);
ptr_t         Iris_ArrBuffPush(Iris_ArrBuff* mem, ptr_t data);
ptr_t         Iris_ArrBuffPop(Iris_ArrBuff* mem);
ptr_t         Iris_ArrBuffAt(Iris_ArrBuff* mem, i64_t index);

#endif
