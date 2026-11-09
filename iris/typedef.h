#ifndef H_TYPEDEF
#define H_TYPEDEF

#include <stdint.h>

typedef void           void_t;
typedef void*          ptr_t;
typedef unsigned char  bool_t;
typedef char           char_t;
typedef char           i8_t;
typedef unsigned char  u8_t;
typedef short          i16_t;
typedef unsigned short u16_t;
typedef int32_t        i32_t;
typedef uint32_t       u32_t;
typedef int64_t        i64_t;
typedef uint64_t       u64_t;
typedef float          f32_t;
typedef double         f64_t;

#define true  (bool_t)1
#define false (bool_t)0
#define null  NULL

#endif
