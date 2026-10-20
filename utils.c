#include "utils.h"

Iris_MemBuffer* Iris_LoadFile(const i8_t* path)
{
  FILE* file = fopen(path, "rb");
  Iris_MemBuffer* src = NULL;
  u64_t len = 0;

  fseek(file, 0, SEEK_END);
  len = (u64_t)ftell(file);
  fseek(file, 0, SEEK_SET);

  src = Iris_MemBufferAlloc(len, sizeof(i8_t));
  src->len = len - 1;

  fread(src->buff, sizeof(i8_t), len, file);
  fclose(file);
  
  return src;
}

void_t  Iris_DumpFile(const i8_t* src, u64_t len)
{
  for(u64_t i = 0; i < len; i++)
  {
    printf("%c", src[i]);
  }

  printf("\n");
}

bool_t Iris_StrCmprN(const i8_t* sa, const i8_t* sb, i32_t len)
{
  for(i32_t i = 0; i < len; i++)
  {
    if(sa[i] != sb[i])
    {
      return false;
    }
  }

  return true;
}
