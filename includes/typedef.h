#ifndef H_TYPEDEF
#define H_TYPEDEF

#include <stdint.h>

typedef void                void_t;
typedef unsigned char       bool_t;
typedef char                i8_t;
typedef unsigned char       u8_t;
typedef short               i16_t;
typedef unsigned short      u16_t;
typedef int                 i32_t;
typedef unsigned int        u32_t;
typedef long long           i64_t;
typedef unsigned long long  u64_t;
typedef float               f32_t;
typedef double              f64_t;

#define true  (bool_t)1
#define false (bool_t)0
#define null  NULL

#endif
