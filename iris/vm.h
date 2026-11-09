#ifndef H_IRIS_VM
#define H_IRIS_VM

#include "iris/codegen.h"
#include "iris/lexer.h"
#include "iris/mem.h"
#include "iris/typedef.h"

typedef struct Iris_VmStack
{
  Iris_Arena*          seg;
  struct Iris_VmStack* next;
} Iris_VmStack;

typedef struct
{
  Iris_Opcode   opcode;
  Iris_Code     code;
  Iris_VmStack  stack;
  Iris_VmStack  op_stack;
  ptr_t         registers[8];
  ptr_t         ip;
  ptr_t         rp;
  ptr_t         sp;
  ptr_t         csp;
  ptr_t         bp;
} Iris_Vm;

void_t Iris_VmNew(Iris_Vm* vm, Iris_Codegen* codegen);
void_t Iris_VmStart(Iris_Vm* vm);

#endif
