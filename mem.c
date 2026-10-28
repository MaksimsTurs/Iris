#include "mem.h"
#include "typedef.h"

Iris_ArrBuff* Iris_ArrBuffAlloc(i64_t cap, i64_t size)
{
  i64_t buff_size = cap * size;
  ptr_t buff = malloc(sizeof(Iris_ArrBuff) + buff_size);
  Iris_ArrBuff* mem = (Iris_ArrBuff*)buff;

  if(mem == null)
  {
    return null;
  }
  
  mem->buff = buff + sizeof(Iris_ArrBuff);
  mem->cap = cap;
  mem->len = 0;
  mem->_size_of_el = size;

  return mem;
}

Iris_ArrBuff* Iris_ArrBuffReAlloc(Iris_ArrBuff* mem, i64_t new_cap)
{
  Iris_ArrBuff* mem_tmp = realloc(mem, sizeof(Iris_ArrBuff) + new_cap * mem->_size_of_el);

  if(mem_tmp == null)
  {
    return null;
  }

  mem_tmp->cap = new_cap;

  return mem_tmp;
}

void_t Iris_ArrBuffDealloc(Iris_ArrBuff* mem)
{
  mem->buff = NULL;
  mem->cap = 0;
  mem->len = 0;
  mem->_size_of_el = 0;
  free(mem);
}

ptr_t Iris_ArrBuffPush(Iris_ArrBuff* mem, ptr_t data)
{
  memcpy(mem->buff + (mem->len * mem->_size_of_el), data, mem->_size_of_el);
  mem->len++;
  return mem->buff + ((mem->len - 1) * mem->_size_of_el);
}

ptr_t Iris_ArrBuffPop(Iris_ArrBuff* mem)
{
  if(mem->len == 0)
  {
    return mem->buff;
  }

  return mem->buff + (mem->len * mem->_size_of_el);
}

ptr_t Iris_ArrBuffAt(Iris_ArrBuff* mem, i64_t index)
{
  if(index > mem->len)
  {
    return null;
  }

  return mem->buff + (index * mem->_size_of_el);
}
