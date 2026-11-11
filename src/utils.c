#include "iris/utils.h"
#include <stdio.h>

bool_t Iris_IsSource(const char_t* filepath)
{
  u64_t len = strlen(filepath);

  if(len < 5)
  {
    return false;
  }

  return Iris_StrnCmp((filepath + (len - 4)), IRIS_EXTENSION, 4);
}

Iris_Source Iris_LoadSource(const i8_t* filepath)
{
  Iris_Source src = {0};
  FILE* file = fopen(filepath, "rb");
  i64_t len = 0;

  // printf("%s\n", filepath);
  fseek(file, 0, SEEK_END);
  len = (i64_t)ftell(file);
  fseek(file, 0, SEEK_SET);

  Iris_ArrayNew(src, len);
  src.len = len - 1;

  fread(src.buff, sizeof(i8_t), (u64_t)len, file);
  fclose(file);
  
  return src;
}

void_t Iris_DumpSource(const i8_t* code, i64_t len)
{
  for(i64_t i = 0; i < len; i++)
  {
    printf("%c", code[i]);
  }

  printf("\n");
}

bool_t Iris_StrnCmp(const i8_t* sa, const i8_t* sb, i64_t len)
{
  if(sa == sb)
  {
    return true;
  }

  for (i64_t i = 0; i < len; i++)
  {
    if(sa[i] != sb[i])
    {
      return false;
    }
  }

  return true;
}
