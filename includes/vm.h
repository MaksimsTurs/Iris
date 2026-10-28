#ifndef H_IRIS_VM
#define H_IRIS_VM

#include "bytecode.h"
#include "lexer.h"
#include "mem.h"
#include "typedef.h"

typedef struct Iris_VmStack
{
  Iris_ArrBuff*        seg;
  struct Iris_VmStack* next;
} Iris_VmStack;

typedef struct
{
  
} Iris_VmStackFrame;

typedef struct
{
  Iris_Opcode   opcode;
  Iris_ArrBuff* bytecode;
  Iris_VmStack  stack;
  Iris_VmStack  op_stack;
  ptr_t         ip;
  ptr_t         rp;
  ptr_t         sp;
  ptr_t         csp;
  ptr_t         bp;
} Iris_Vm;

void_t Iris_VmInit(Iris_Vm* vm, Iris_ByteCode* bytecode);
void_t Iris_VmStart(Iris_Vm* vm);

#endif
