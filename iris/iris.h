#ifndef H_IRIS
#define H_IRIS

#include "iris/utils.h"
#include "iris/ansicodes.h"
#include "iris/typedef.h"

#include <stdio.h>

typedef struct
{
  char_t* filepath;
  bool_t  p_tokens;
  bool_t  p_ast;
  bool_t  p_code;
} Iris_Options;

bool_t Iris_ProcessCmdArgs(Iris_Options* options, i8_t** args, i32_t len);

#endif
