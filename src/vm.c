#include "iris/vm.h"

static bool_t Iris_VmIsRunning(const Iris_Vm* vm)
{
  return (Iris_Cast(char_t*, vm->ip) - vm->bytecode.buff) < vm->bytecode.len;
}

static void_t Iris_VmFetchOpcode(Iris_Vm* vm)
{
  vm->opcode = *(Iris_Opcode*)vm->ip;
  vm->ip += sizeof(Iris_Opcode);
}

static Iris_TypeKind Iris_VmFetchType(Iris_Vm* vm)
{
  Iris_TypeKind type = *(Iris_TypeKind*)vm->ip;
  vm->ip += sizeof(Iris_TypeKind);
  return type;
}

static i64_t Iris_VmFetchInt64(Iris_Vm* vm)
{
  i64_t val = *(i64_t*)(vm->ip);
  vm->ip += sizeof(i64_t);
  return val;
}

static void_t Iris_VmStackAlloc(Iris_Vm* vm, i64_t size)
{
  vm->bp = vm->sp;
  vm->csp = vm->bp;
  vm->sp += size;
}

static i64_t Iris_VmStackReadInt64(Iris_Vm* vm, i64_t offset)
{
  return *(i64_t*)(vm->bp + offset);
}

static void_t Iris_VmStackWriteInt64(Iris_Vm* vm, i64_t offset, i64_t val)
{
  *(i64_t*)(vm->bp + offset) = val;
}

static void_t Iris_VmStackPushInt64(Iris_Vm* vm, i64_t val)
{
  *(i64_t*)vm->csp = val;
  vm->csp += sizeof(i64_t);
}

static void_t Iris_VmOPStackPushInt64(Iris_Vm* vm, i64_t val)
{
  *(i64_t*)(vm->op_stack.seg->buff + vm->op_stack.seg->used) = val;
  vm->op_stack.seg->used += sizeof(val);
}

static i64_t Iris_VmOPStackPopInt64(Iris_Vm* vm)
{
  i64_t val = *(i64_t*)(vm->op_stack.seg->buff + (vm->op_stack.seg->used - (i64_t)sizeof(i64_t)));
  vm->op_stack.seg->used -= sizeof(i64_t);
  return val;
}

void_t Iris_VmNew(Iris_Vm* vm, Iris_Bytecode* bytecode)
{
  vm->bytecode = *bytecode;
  vm->ip = vm->bytecode.buff + bytecode->main_offset;
  vm->rp = vm->bytecode.buff + bytecode->len;
  vm->stack.next = null;
  Iris_ArenaNew(vm->stack.seg, 1024);
  Iris_ArenaNew(vm->op_stack.seg, 1024);
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
  Iris_TypeKind type = 0;

  while(Iris_VmIsRunning(vm))
  {
    Iris_VmFetchOpcode(vm);
    switch(vm->opcode)
    {
      case OPC_FN:
        tmp_a = Iris_VmFetchInt64(vm);
        tmp_sp = vm->sp;
        tmp_csp = vm->csp;
        tmp_bp = vm->bp;
        Iris_VmStackAlloc(vm, tmp_a);
        Iris_VmStackPushInt64(vm, Iris_Cast(i64_t, tmp_sp));
        Iris_VmStackPushInt64(vm, Iris_Cast(i64_t, tmp_csp));
        Iris_VmStackPushInt64(vm, Iris_Cast(i64_t, tmp_bp));
        Iris_VmStackPushInt64(vm, Iris_Cast(i64_t, vm->rp));
        break;
      case OPC_FN_CALL:
        tmp_a = Iris_VmFetchInt64(vm);
        vm->rp = vm->ip;
        vm->ip = vm->bytecode.buff + tmp_a;
        break;
      case OPC_FN_RETVOID:
      case OPC_FN_RETVAL:
        tmp_sp = Iris_Cast(ptr_t, Iris_VmStackReadInt64(vm, 0));
        tmp_csp = Iris_Cast(ptr_t, Iris_VmStackReadInt64(vm, 8));
        tmp_bp = Iris_Cast(ptr_t, Iris_VmStackReadInt64(vm, 16));
        vm->ip = Iris_Cast(ptr_t, Iris_VmStackReadInt64(vm, 24));
        vm->sp = tmp_sp;
        vm->csp = tmp_csp;
        vm->bp = tmp_bp;
        break;
      case OPC_LOADLT:
        type = Iris_VmFetchType(vm);
        tmp_a = Iris_VmFetchInt64(vm);
        switch(type)
        {
          case TP_I64:
          case TP_U64:
            Iris_VmOPStackPushInt64(vm, tmp_a);
            break;
        }
        break;
      case OPC_LOADLC:
        type = Iris_VmFetchType(vm);
        tmp_a = Iris_VmFetchInt64(vm);
        switch(type)
        {
          case TP_I64:
          case TP_U64:
            Iris_VmOPStackPushInt64(vm, Iris_VmStackReadInt64(vm, tmp_a));
            break;
        }
        break;
      case OPC_MOVLC:
        type = Iris_VmFetchType(vm);
        tmp_a = Iris_VmFetchInt64(vm);
        switch(type)
        {
          case TP_I64:
          case TP_U64:
            tmp_b = Iris_VmOPStackPopInt64(vm);
            Iris_VmStackWriteInt64(vm, tmp_a, tmp_b);
            break;
        }
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
      case OPC_DIV:
        tmp_b = Iris_VmOPStackPopInt64(vm);
        tmp_a = Iris_VmOPStackPopInt64(vm);
        Iris_VmOPStackPushInt64(vm, tmp_a / tmp_b);
        break;
      case OPC_MOD:
        tmp_a = Iris_VmOPStackPopInt64(vm);
        tmp_b = Iris_VmOPStackPopInt64(vm);
        Iris_VmOPStackPushInt64(vm, tmp_a % tmp_b);
        break;
      case OPC_AND:
        tmp_a = Iris_VmOPStackPopInt64(vm);
        tmp_b = Iris_VmOPStackPopInt64(vm);
        Iris_VmOPStackPushInt64(vm, tmp_a && tmp_b);
        break;
      case OPC_OR:
        tmp_a = Iris_VmOPStackPopInt64(vm);
        tmp_b = Iris_VmOPStackPopInt64(vm);
        Iris_VmOPStackPushInt64(vm, tmp_a || tmp_b);
        break;
      case OPC_BIT_AND:
        tmp_b = Iris_VmOPStackPopInt64(vm);
        tmp_a = Iris_VmOPStackPopInt64(vm);
        Iris_VmOPStackPushInt64(vm, tmp_a & tmp_b);
        break;
      case OPC_BIT_OR:
        tmp_b = Iris_VmOPStackPopInt64(vm);
        tmp_a = Iris_VmOPStackPopInt64(vm);
        Iris_VmOPStackPushInt64(vm, tmp_a | tmp_b);
        break;
      case OPC_BIT_XOR:
        tmp_b = Iris_VmOPStackPopInt64(vm);
        tmp_a = Iris_VmOPStackPopInt64(vm);
        Iris_VmOPStackPushInt64(vm, tmp_a ^ tmp_b);
        break;
      case OPC_BIT_LSHIFT:
        tmp_b = Iris_VmOPStackPopInt64(vm);
        tmp_a = Iris_VmOPStackPopInt64(vm);
        Iris_VmOPStackPushInt64(vm, tmp_a << tmp_b);
        break;
      case OPC_BIT_RSHIFT:
        tmp_b = Iris_VmOPStackPopInt64(vm);
        tmp_a = Iris_VmOPStackPopInt64(vm);
        Iris_VmOPStackPushInt64(vm, tmp_a >> tmp_b);
        break;
      case OPC_CMPEGT:
        tmp_b = Iris_VmOPStackPopInt64(vm);
        tmp_a = Iris_VmOPStackPopInt64(vm);
        Iris_VmOPStackPushInt64(vm, tmp_a >= tmp_b);
        break;
      case OPC_CMPELT:
        tmp_b = Iris_VmOPStackPopInt64(vm);
        tmp_a = Iris_VmOPStackPopInt64(vm);
        Iris_VmOPStackPushInt64(vm, tmp_a <= tmp_b);
        break;
      case OPC_CMPEQ:
        tmp_b = Iris_VmOPStackPopInt64(vm);
        tmp_a = Iris_VmOPStackPopInt64(vm);
        Iris_VmOPStackPushInt64(vm, tmp_a == tmp_b);
        break;
      case OPC_CMPNEQ:
        tmp_b = Iris_VmOPStackPopInt64(vm);
        tmp_a = Iris_VmOPStackPopInt64(vm);
        Iris_VmOPStackPushInt64(vm, tmp_a != tmp_b);
        break;
      case OPC_CMPGT:
        tmp_b = Iris_VmOPStackPopInt64(vm);
        tmp_a = Iris_VmOPStackPopInt64(vm);
        Iris_VmOPStackPushInt64(vm, tmp_a > tmp_b);
        break;
      case OPC_CMPLT:
        tmp_b = Iris_VmOPStackPopInt64(vm);
        tmp_a = Iris_VmOPStackPopInt64(vm);
        Iris_VmOPStackPushInt64(vm, tmp_a < tmp_b);
        break;
      case OPC_NOT:
        tmp_a = Iris_VmOPStackPopInt64(vm);
        Iris_VmOPStackPushInt64(vm, !tmp_a);
        break;
      case OPC_BIT_NOT:
        tmp_a = Iris_VmOPStackPopInt64(vm);
        Iris_VmOPStackPushInt64(vm, ~tmp_a);
        break;
      case OPC_JMP:
        tmp_a = Iris_VmFetchInt64(vm);
        vm->ip = vm->bytecode.buff + tmp_a;
        break;
      case OPC_JMPNIF:
        tmp_a = Iris_VmOPStackPopInt64(vm);
        tmp_b = Iris_VmFetchInt64(vm);
        if(!tmp_a) vm->ip = vm->bytecode.buff + tmp_b;
        break;
    }
  }
}
