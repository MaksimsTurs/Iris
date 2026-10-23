#ifndef H_IRIS_BYTECODE
#define H_IRIS_BYTECODE

#include "typedef.h"
#include "parser.h"
#include "mem.h"

typedef enum {
  OP_UNKNOWN = 0,
  OP_ADD,
  OP_SUB,
  OP_DIV,
  OP_MUL,
  OP_MOV,
  OP_FN,
  OP_CALL,
  OP_RET
} Iris_OpCode;

typedef struct {
  Iris_Token* name;
  i8_t*       ret_ptr;
  i8_t*       start_ptr;
} Iris_ByteCodeJmpTableEntry;

typedef struct {
  Iris_ByteCodeJmpTableEntry entries[32];
  i32_t                 len;
} Iris_ByteCodeJmpTable;

typedef struct {
  Iris_MemBuffer*       bytecode;
  Iris_AstNode*         bottom;
  Iris_ByteCodeJmpTable jmp_table;
} Iris_ByteCode;

void_t Iris_ByteCodeGenerate(Iris_ByteCode* bytecode, Iris_Parser* parser);
void_t Iris_ByteCodeDump(const Iris_ByteCode* bytecode);

#endif
