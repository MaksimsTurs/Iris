#ifndef H_IRIS_UTILS
#define H_IRIS_UTILS

#include <stdio.h>

#include "mem.h"
#include "report.h"
#include "typedef.h"

#define Iris_Max(a, b) a > b ? a : b

Iris_ArrBuff* Iris_LoadFile(const i8_t* path);
void_t        Iris_DumpFile(const i8_t* src, u64_t len);

#endif

