#include "vm.h"
#include "bytecode.h"
#include "mem.h"
#include "opcodes.h"
#include "typedef.h"
#include <endian.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

static bool_t Iris_VmIsRunning(const Iris_Vm* vm)
{
  return (vm->ip - vm->bytecode->buff) < vm->bytecode->len;
}

static void_t Iris_VmFetchOpcode(Iris_Vm* vm)
{
  vm->opcode = *(Iris_Opcode*)vm->ip;
  vm->ip += sizeof(Iris_Opcode);
}

static i64_t Iris_VmFetchInt64(Iris_Vm* vm)
{
  return *(i64_t*)(vm->ip);
}

static void_t Iris_VmStackAlloc(Iris_Vm* vm, i64_t size)
{
  vm->bp = vm->sp;
  vm->csp = vm->bp;
  vm->sp += size;
}

static i64_t Iris_VmStackReadInt64(Iris_Vm* vm, i64_t index)
{
  index += 4;
  return *(i64_t*)(vm->bp + (sizeof(i64_t) * index));
}

static void_t Iris_VmStackWriteInt64(Iris_Vm* vm, i64_t index, i64_t val)
{
  index += 4;
  *(i64_t*)(vm->bp + (sizeof(val) * index)) = val;
}

static void_t Iris_VmStackPushInt64(Iris_Vm* vm, i64_t val)
{
  *(i64_t*)vm->csp = val;
  vm->csp += sizeof(i64_t);
}

static void_t Iris_VmOPStackPushInt64(Iris_Vm* vm, i64_t val)
{
  *(i64_t*)(vm->op_stack.seg->buff + vm->op_stack.seg->len) = val;
  vm->op_stack.seg->len += sizeof(val);
}

static i64_t Iris_VmOPStackPopInt64(Iris_Vm* vm)
{
  i64_t val = *(i64_t*)(vm->op_stack.seg->buff + (vm->op_stack.seg->len - sizeof(i64_t)));
  vm->op_stack.seg->len -= sizeof(i64_t);
  return val;
}

void_t Iris_VmInit(Iris_Vm* vm, Iris_ByteCode* bytecode)
{
  vm->bytecode = bytecode->bytecode;
  vm->ip = bytecode->bytecode->buff;
  vm->rp = bytecode->bytecode->buff + bytecode->bytecode->len;
  vm->stack.next = null;
  vm->op_stack.seg = Iris_ArrBuffAlloc(512, sizeof(i8_t));
  vm->stack.seg = Iris_ArrBuffAlloc(1024, sizeof(i8_t));
  vm->sp = vm->stack.seg->buff;
  vm->bp = vm->stack.seg->buff;
}

void_t Iris_VmStart(Iris_Vm *vm)
{
  i64_t tmp_a = 0;
  i64_t tmp_b = 0;
  ptr_t tmp_sp = null;
  ptr_t tmp_csp = null;
  ptr_t tmp_bp = null;

  while(Iris_VmIsRunning(vm))
  {
    Iris_VmFetchOpcode(vm);

    switch(vm->opcode)
    {
      case OPC_PROLOG:
        tmp_a = Iris_VmFetchInt64(vm);
        tmp_sp = vm->sp;
        tmp_csp = vm->csp;
        tmp_bp = vm->bp;
        Iris_VmStackAlloc(vm, tmp_a);
        Iris_VmStackPushInt64(vm, (i64_t)tmp_sp);
        Iris_VmStackPushInt64(vm, (i64_t)tmp_csp);
        Iris_VmStackPushInt64(vm, (i64_t)tmp_bp);
        Iris_VmStackPushInt64(vm, (i64_t)vm->rp);
        break;
      case OPC_PUSHNLOS:
        tmp_a = Iris_VmFetchInt64(vm);
        Iris_VmOPStackPushInt64(vm, tmp_a);
        break;
      case OPC_PUSHNLSF:
        tmp_a = Iris_VmFetchInt64(vm);
        Iris_VmStackPushInt64(vm, tmp_a);
        break;
      case OPC_PUSHSFOS:
        tmp_a = Iris_VmFetchInt64(vm);
        tmp_a = Iris_VmStackReadInt64(vm, tmp_a);
        Iris_VmOPStackPushInt64(vm, tmp_a); 
        break;
      case OPC_MOVOSSF:
        tmp_a = Iris_VmFetchInt64(vm);
        tmp_b = Iris_VmOPStackPopInt64(vm);
        Iris_VmStackWriteInt64(vm, tmp_a, tmp_b);
        break;
      case OPC_ADD:
        tmp_a = Iris_VmOPStackPopInt64(vm);
        tmp_b = Iris_VmOPStackPopInt64(vm);
        Iris_VmOPStackPushInt64(vm, tmp_a + tmp_b);
        break;
      case OPC_SUB:
        tmp_b = Iris_VmOPStackPopInt64(vm);
        tmp_a = Iris_VmOPStackPopInt64(vm);
        Iris_VmOPStackPushInt64(vm, tmp_a - tmp_b);
        break; 
      case OPC_MUL:
        tmp_a = Iris_VmOPStackPopInt64(vm);
        tmp_b = Iris_VmOPStackPopInt64(vm);
        Iris_VmOPStackPushInt64(vm, tmp_a * tmp_b);
        break;
      case OPC_RETVOID:
        tmp_sp = (ptr_t)Iris_VmStackReadInt64(vm, -4);
        tmp_csp = (ptr_t)Iris_VmStackReadInt64(vm, -3);
        tmp_bp = (ptr_t)Iris_VmStackReadInt64(vm, -2);
        vm->ip = (ptr_t)Iris_VmStackReadInt64(vm, -1);
        vm->sp = tmp_sp;
        vm->csp = tmp_csp;
        vm->bp = tmp_bp;
        break;
    }
  }
}
