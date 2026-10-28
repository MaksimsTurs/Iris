#include "utils.h"
#include "report.h"

Iris_ArrBuff* Iris_LoadFile(const i8_t* path)
{
  FILE* file = fopen(path, "rb");
  Iris_ArrBuff* src = null;
  u64_t len = 0;

  if(file == null)
  {
    Iris_ReportSimpleError("could not open file %s!", path);
  }

  fseek(file, 0, SEEK_END);
  len = (u64_t)ftell(file);
  fseek(file, 0, SEEK_SET);

  src = Iris_ArrBuffAlloc(len, sizeof(i8_t));
  src->len = len - 1;

  if(src == null)
  {
    Iris_ReportSimpleError("could not load file %s into memory!", path);
  }

  fread(src->buff, sizeof(i8_t), len, file);
  fclose(file);
  
  return src;
}

void_t Iris_DumpFile(const i8_t* src, u64_t len)
{
  for(u64_t i = 0; i < len; i++)
  {
    printf("%c", src[i]);
  }

  printf("\n");
}
