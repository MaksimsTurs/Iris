#ifndef H_IRIS_BYTECODE
#define H_IRIS_BYTECODE

#include "iris/iris.h"
#include "iris/parser.h"
#include "iris/opcodes.h"
#include "iris/mem.h"
#include "iris/typedef.h"

#include <stdio.h>

#define IRIS_BYTECODE_DEF_CODE_CAP         256
#define IRIS_BYTECODE_DEF_JMP_ENTRIES_CAP  4

typedef struct
{
  Iris_Node* node;
  i64_t      depth;
} Iris_NodeDepth;

typedef struct
{
  Iris_FnDefStmt* fn;
  i64_t           start_offset;
} Iris_JMPEntry;

typedef struct
{
  Iris_JMPEntry* buff;
  i64_t          cap;
  i64_t          len;
} Iris_JMPEntryArray;

typedef struct 
{
  i64_t               main_offset;
  Iris_Node*          root;
  Iris_JMPEntryArray  jmps;
  i8_t*               buff;
  i64_t               cap;
  i64_t               len;
} Iris_Bytecode;

void_t Iris_BytecodeNew(Iris_Bytecode* bytecode, Iris_Parser* parser);
void_t Iris_BytecodeGenerate(Iris_Bytecode* bytecode);
void_t Iris_BytecodeDump(const Iris_Bytecode* bytecode);

#endif
