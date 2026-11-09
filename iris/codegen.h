#ifndef H_IRIS_CODEGEN
#define H_IRIS_CODEGEN

#include "iris/lexer.h"
#include "iris/parser.h"
#include "iris/opcodes.h"
#include "iris/mem.h"
#include "iris/typedef.h"

#define Iris_CodeGenCalStartOfsset(code) ((code.buff + code.len) - code.buff)

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
  Iris_Node* node;
  i64_t      depth;
} Iris_NodeDepth;

typedef struct
{
  i8_t* buff;
  i64_t cap;
  i64_t len;
} Iris_Code;

typedef struct 
{
  Iris_JMPEntryArray  jmps;
  Iris_Code           code;
  Iris_Node*          root;
  i64_t               main_offset;
} Iris_Codegen;

void_t      Iris_CodegenInit(Iris_Codegen* gen, Iris_Parser* parser);
void_t      Iris_CodegenGenerate(Iris_Codegen* gen);
void_t      Iris_CodegenDump(const Iris_Codegen* gen);
const i8_t* Iris_CodegenOpcToString(Iris_Opcode opcode);

#endif
