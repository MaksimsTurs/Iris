#ifndef H_IRIS_UTILS
#define H_IRIS_UTILS

#include "mem.h"
#include "typedef.h"

#include <stdio.h>

Iris_MemBuffer* Iris_LoadFile(const i8_t* path);
void_t          Iris_DumpFile(const i8_t* src, u64_t len);
bool_t          Iris_StrCmprN(const i8_t* sa, const i8_t* sb, i32_t len);

#endif

