#ifndef H_IRIS_VM
#define H_IRIS_VM

#include "bytecode.h"
#include "lexer.h"
#include "typedef.h"

#define IRIS_VM_STACK_SIZE 1024

typedef struct Iris_VmStackBuffer {
  i8_t*                       buff;
  u64_t                       cap;
  u64_t                       len;
  struct Iris_VmStackBuffer*  next;
} Iris_VmStackBuffer;

typedef struct {
  i32_t               ip;
  Iris_VmStackBuffer* stack_buffer;
} Iris_Vm;

void_t Iris_VmStart(Iris_Vm* vm, Iris_ByteCode* bytecode);

#endif
