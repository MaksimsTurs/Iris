#include "iris/utils.h"

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

  fseek(file, 0, SEEK_END);
  len = (i64_t)ftell(file);
  fseek(file, 0, SEEK_SET);

  Iris_ArrayNew(src, len);
  src.len = len - 1;
  src.modulepath = (char_t*)filepath;

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

void_t Iris_ReportNotFoundError(const char_t* modulepath, const char_t* name, i64_t len, i64_t row, i64_t col)
{
  printf(IRIS_ANSI_RED "Error[NotFound]" IRIS_ANSI_RES "[%s:%li:%li]: could not find \"%.*s\"!\n", modulepath, row, col, (i32_t)len, name);
  exit(-1);
}

void_t Iris_ReportRedefinitionError(const char_t* modulepath, const char_t* name, i64_t len, i64_t row, i64_t col)
{
  printf(IRIS_ANSI_RED "Error[Redefinition]" IRIS_ANSI_RES "[%s:%li:%li]: redefinition of \"%.*s\"!\n", modulepath, row, col, (i32_t)len, name);
  exit(-1);
}

void_t Iris_ReportSyntaxError(const char_t* modulepath, const char_t* msg, const i32_t* kinds, const char_t* next, i64_t len, i64_t row, i64_t col)
{
  i32_t kind = -1;

  printf(IRIS_ANSI_RED "Error[Syntax]" IRIS_ANSI_RES "[%s:%li:%li]:", modulepath, row, col);

  if(kinds)
  {
    printf("%s ",  msg);
    for(i64_t i = 0; (kind = kinds[i]) != -1; i++)
    {
      if(i > 0) printf(" or ");
      printf("\"%s\"", Iris_LexerTokenTypeToString(kind));
    }
    printf(" before \"%.*s\"!\n", (i32_t)len, next);
  }
  else
  {
    printf("%s before \"%.*s\"!\n", msg, (i32_t)len, next);
  }
  exit(-1);
}
