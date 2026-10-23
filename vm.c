#include "vm.h"
#include "bytecode.h"
#include "typedef.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Iris_VmStackBuffer* Iris_VmStackBufferAlloc(void_t)
{
  void* buff = malloc(sizeof(Iris_VmStackBuffer) + IRIS_VM_STACK_SIZE);
  memset(buff, 0, sizeof(Iris_VmStackBuffer) + IRIS_VM_STACK_SIZE);
  Iris_VmStackBuffer* stack = buff;

  stack->buff = buff + sizeof(Iris_VmStackBuffer);
  stack->cap = IRIS_VM_STACK_SIZE;
  stack->len = 0;
  stack->next = NULL;

  return stack;
}

static i8_t* Iris_VmStackBufferAllocFrame(Iris_VmStackBuffer* stack_buffer, i64_t size)
{
  i8_t* frame = stack_buffer->buff + stack_buffer->len;
  stack_buffer->len += size;
  return frame;
}

static i8_t* Iris_VmStackBufferDeallocFrame(Iris_VmStackBuffer* stack_buffer, i64_t size)
{
  stack_buffer->len -= size;
  return stack_buffer->buff - size;
}

static void_t Iris_VmInit(Iris_Vm* vm)
{
  vm->stack_buffer = Iris_VmStackBufferAlloc();
  vm->ip = 0;
}

void_t Iris_VmStart(Iris_Vm *vm, Iris_ByteCode *bytecode)
{
  Iris_VmInit(vm);

  i32_t index = 0;
  i8_t code = 0;

  i64_t stack_size = 0;
  i64_t fn_addr = 0;
  i64_t ret_addr = 0;
  i8_t* buffer = bytecode->bytecode->buff;
  i8_t* frame = NULL;

  buffer += (i32_t)(bytecode->bytecode->len - (sizeof(i8_t) + sizeof(i64_t)));
  
  while((buffer - bytecode->bytecode->buff) < bytecode->bytecode->len)
  {
    code = *buffer++;

    switch(code) 
    {
      case OP_MOV:
        printf("OP_MOV\n");
        index = (i32_t)*(buffer + vm->ip);
        *((i64_t*)frame + (index + sizeof(i32_t))) = (i64_t)*(buffer + vm->ip + sizeof(i32_t));
        break;
      case OP_FN:
        stack_size = (i64_t)*(i64_t*)buffer;
        stack_size += sizeof(i64_t);
        buffer += sizeof(i64_t);

        frame = Iris_VmStackBufferAllocFrame(vm->stack_buffer, stack_size);
        *(i64_t*)frame = ret_addr;
                
        printf("OP_FN: fn_addr: %lli stack_size: %lli ret_addr: %lli\n", fn_addr, stack_size, *(i64_t*)frame);
        break;
      case OP_CALL:
        fn_addr = *(i64_t*)buffer;
        buffer += sizeof(i64_t);
        ret_addr = (i64_t)buffer;
        buffer = (i8_t*)fn_addr;

        printf("OP_CALL: fn_addr: %lli ret_addr: %lli\n", fn_addr, ret_addr);
        break;
      case OP_RET:
        stack_size = (i64_t)*(i64_t*)buffer;
        stack_size += sizeof(i64_t);
        ret_addr = (i64_t)*(i64_t*)frame;
        buffer = (i8_t*)ret_addr;

        frame = Iris_VmStackBufferDeallocFrame(vm->stack_buffer, stack_size);
        printf("OP_RET: stack_size: %lli ret_addr: %lli\n", stack_size, ret_addr);
        break;
    }
  }
}
