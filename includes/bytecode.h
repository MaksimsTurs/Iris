#ifndef H_IRIS_BYTECODE
#define H_IRIS_BYTECODE

#include "lexer.h"
#include "typedef.h"
#include "parser.h"
#include "mem.h"
#include "opcodes.h"

#define Iris_ByteCodeCalcStartOffset(bytecode) ((bytecode->bytecode->buff + bytecode->bytecode->len) - bytecode->bytecode->buff)
#define Iris_ByteCodeCaclCodeSize(start_offset, bytecode) bytecode->bytecode->len - start_offset

typedef struct {
  Iris_ArrBuff*         bytecode;
  Iris_AstNode*         bottom;
  Iris_AstNode*         root;
  i64_t                 depth;
} Iris_ByteCode;

void_t Iris_ByteCodeInit(Iris_ByteCode* bytecode);
void_t Iris_ByteCodeGenerate(Iris_ByteCode* bytecode, Iris_Parser* parser);
void_t Iris_ByteCodeDump(const Iris_ByteCode* bytecode);

#endif
