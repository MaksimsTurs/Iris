#ifndef H_IRIS_BUILTIN_TYPES
#define H_IRIS_BUILTIN_TYPES

#include "iris/typedef.h"

#define Iris_Nan      NAN
#define Iris_Infinity INFINITY
#define Iris_Nil      0
typedef f64_t         Iris_Float64;
typedef i64_t         Iris_Int64;
typedef bool_t        Iris_Bool;

typedef struct
{
  i64_t id;
  ptr_t as;
} Iris_Any;

typedef struct 
{
  char_t* buff;
  i64_t   len;
} Iris_Str;

#endif
