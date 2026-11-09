#ifndef H_IRIS_UTILS
#define H_IRIS_UTILS

#include <stdio.h>

#include "iris/mem.h"
#include "iris/typedef.h"

#define IRIS_EXTENSION "iri"

#define Iris_Max(a, b) a > b ? a : b

typedef struct
{
  char_t* buff;
  i64_t   cap;
  i64_t   len;
} Iris_Source;

Iris_Source Iris_LoadSource(const i8_t* filepath);
void_t      Iris_DumpSource(const i8_t* code, i64_t len);
bool_t      Iris_IsSource(const char_t* filepath);
bool_t      Iris_StrnCmp(const i8_t* sa, const i8_t* sb, i64_t len);

#endif

