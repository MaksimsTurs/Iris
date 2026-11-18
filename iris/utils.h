#ifndef H_IRIS_UTILS
#define H_IRIS_UTILS

#include <stdio.h>

#include "iris/mem.h"
#include "iris/typedef.h"

#define IRIS_ANSI_RES     "\033[0m"
#define IRIS_ANSI_DIM     "\033[2m"
#define IRIS_ANSI_RED     "\033[91;1m"
#define IRIS_ANSI_YELLOW  "\033[93;1m"

#define IRIS_EXTENSION "iris"

#define Iris_Cast($type, $val) (($type)($val))
#define Iris_Max($a, $b) ($a) > ($b) ? ($a) : ($b)
#define Iris_Unused($var) ((void_t)($var))

typedef struct
{
  char_t* modulepath;
  char_t* buff;
  i64_t   cap;
  i64_t   len;
} Iris_Source;

Iris_Source Iris_LoadSource(const i8_t* modulepath);
void_t      Iris_DumpSource(const i8_t* code, i64_t len);
bool_t      Iris_IsSource(const char_t* filepath);
bool_t      Iris_StrnCmp(const i8_t* sa, const i8_t* sb, i64_t len);
#define     Iris_ReportInternError(...) do { printf(IRIS_ANSI_RED "Error[Intern]: " IRIS_ANSI_RES __VA_ARGS__); printf("\n"); exit(-1); } while(0)
void_t      Iris_ReportNotFoundError(const char_t* modulepath, const char_t* name, i64_t len, i64_t row, i64_t col);
void_t      Iris_ReportRedefinitionError(const char_t* modulepath, const char_t* name, i64_t len, i64_t row, i64_t col);
void_t      Iris_ReportSyntaxError(const char_t* modulepath, const char_t* msg, const i32_t* kinds, const char_t* next, i64_t len, i64_t row, i64_t col);

#endif

