#include "iris/vm.h"

static bool_t Iris_VmIsRunning(const Iris_Vm* vm)
{
  return ((char_t*)vm->ip - vm->code.buff) < vm->code.len;
}

static void_t Iris_VmFetchOpcode(Iris_Vm* vm)
{
  vm->opcode = *(Iris_Opcode*)vm->ip;
  vm->ip += sizeof(Iris_Opcode);
}

static i64_t Iris_VmFetchInt64(Iris_Vm* vm)
{
  i64_t val = *(i64_t*)(vm->ip);
  vm->ip += sizeof(i64_t);
  return val;
}

static f64_t Iris_VmFetchFloat64(Iris_Vm* vm)
{
  f64_t val = *(f64_t*)(vm->ip);
  vm->ip += sizeof(f64_t);
  return val;
}

static void_t Iris_VmStackAlloc(Iris_Vm* vm, i64_t size)
{
  vm->bp = vm->sp;
  vm->csp = vm->bp;
  vm->sp += size;
}

static i64_t Iris_VmStackReadInt64(Iris_Vm* vm, i64_t index)
{
  return *(i64_t*)(vm->bp + ((i64_t)sizeof(i64_t) * index));
}

static void_t Iris_VmStackWriteInt64(Iris_Vm* vm, i64_t index, i64_t val)
{
  *(i64_t*)(vm->bp + ((i64_t)sizeof(val) * index)) = val;
}

static f64_t Iris_VmStackReadFloat64(Iris_Vm* vm, i64_t index)
{
  return *(f64_t*)(vm->bp + ((i64_t)sizeof(f64_t) * (index)));
}

static void_t Iris_VmStackWriteFloat64(Iris_Vm* vm, i64_t index, f64_t val)
{
  *(f64_t*)(vm->bp + ((i64_t)sizeof(val) * (index))) = val;
}

static void_t Iris_VmStackPushInt64(Iris_Vm* vm, i64_t val)
{
  vm->csp += sizeof(i64_t);
  *(i64_t*)vm->csp = val;
}

static void_t Iris_VmStackPushFloat64(Iris_Vm* vm, f64_t val)
{
  vm->csp += sizeof(f64_t);
  *(f64_t*)vm->csp = val;
}

static void_t Iris_VmOPStackPushFloat64(Iris_Vm* vm, f64_t val)
{
  *(f64_t*)(vm->op_stack.seg->buff + vm->op_stack.seg->used) = val;
  vm->op_stack.seg->used += sizeof(val);
}

static f64_t Iris_VmOPStackPopFloat64(Iris_Vm* vm)
{
  f64_t val = *(f64_t*)(vm->op_stack.seg->buff + (vm->op_stack.seg->used - sizeof(i64_t)));
  vm->op_stack.seg->used -= sizeof(f64_t);
  return val;
}

void_t Iris_VmNew(Iris_Vm* vm, Iris_Codegen* codegen)
{
  vm->code = codegen->code;
  vm->ip = vm->code.buff + codegen->main_offset;
  vm->rp = vm->code.buff + (codegen->main_offset + 14);
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
  f64_t f64_tmp_a = 0;
  f64_t f64_tmp_b = 0;
  ptr_t tmp_sp = null;
  ptr_t tmp_csp = null;
  ptr_t tmp_bp = null;

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
        Iris_VmStackPushInt64(vm, (i64_t)tmp_sp);
        Iris_VmStackPushInt64(vm, (i64_t)tmp_csp);
        Iris_VmStackPushInt64(vm, (i64_t)tmp_bp);
        Iris_VmStackPushInt64(vm, (i64_t)vm->rp);
        break;
      case OPC_FN_CALL:
        tmp_a = Iris_VmFetchInt64(vm);
        vm->rp = vm->ip;
        vm->ip = (vm->code.buff + tmp_a);
        break;
      case OPC_MOVNLOS:
        f64_tmp_a = Iris_VmFetchFloat64(vm);
        Iris_VmOPStackPushFloat64(vm, f64_tmp_a);
        break;
      case OPC_MOVNLSF:
        f64_tmp_a = Iris_VmFetchFloat64(vm);
        Iris_VmStackPushFloat64(vm, f64_tmp_a);
        break;
      case OPC_MOVOSSF:
        tmp_a = Iris_VmFetchInt64(vm);
        f64_tmp_b = Iris_VmOPStackPopFloat64(vm);
        Iris_VmStackWriteFloat64(vm, tmp_a, f64_tmp_b);
        break;
      case OPC_COPYSFOS:
        tmp_a = Iris_VmFetchInt64(vm);
        f64_tmp_b = Iris_VmStackReadFloat64(vm, tmp_a);
        Iris_VmOPStackPushFloat64(vm, f64_tmp_b); 
        break;
      case OPC_COPYSFSF:
        tmp_a = Iris_VmFetchInt64(vm);
        f64_tmp_a = Iris_VmStackReadFloat64(vm, tmp_a);
        Iris_VmOPStackPushFloat64(vm, f64_tmp_a); 
      case OPC_ADD:
        f64_tmp_a = Iris_VmOPStackPopFloat64(vm);
        f64_tmp_b = Iris_VmOPStackPopFloat64(vm);
        Iris_VmOPStackPushFloat64(vm, f64_tmp_a + f64_tmp_b);
        break;
      case OPC_SUB:
        f64_tmp_b = Iris_VmOPStackPopFloat64(vm);
        f64_tmp_a = Iris_VmOPStackPopFloat64(vm);
        Iris_VmOPStackPushFloat64(vm, f64_tmp_a - f64_tmp_b);
        break; 
      case OPC_MUL:
        f64_tmp_a = Iris_VmOPStackPopFloat64(vm);
        f64_tmp_b = Iris_VmOPStackPopFloat64(vm);
        Iris_VmOPStackPushFloat64(vm, f64_tmp_a * f64_tmp_b);
        break;
      case OPC_DIV:
        f64_tmp_b = Iris_VmOPStackPopFloat64(vm);
        f64_tmp_a = Iris_VmOPStackPopFloat64(vm);
        Iris_VmOPStackPushFloat64(vm, f64_tmp_a / f64_tmp_b);
        break;
      case OPC_FN_RETVOID:
        tmp_sp = (ptr_t)Iris_VmStackReadInt64(vm, 0);
        tmp_csp = (ptr_t)Iris_VmStackReadInt64(vm, 1);
        tmp_bp = (ptr_t)Iris_VmStackReadInt64(vm, 2);
        vm->ip = (ptr_t)Iris_VmStackReadInt64(vm, 3);
        vm->sp = tmp_sp;
        vm->csp = tmp_csp;
        vm->bp = tmp_bp;
        break;
      case OPC_FN_RETVAL:
        tmp_sp = (ptr_t)Iris_VmStackReadInt64(vm, 0);
        tmp_csp = (ptr_t)Iris_VmStackReadInt64(vm, 1);
        tmp_bp = (ptr_t)Iris_VmStackReadInt64(vm, 2);
        vm->ip = (ptr_t)Iris_VmStackReadInt64(vm, 3);
        vm->sp = tmp_sp;
        vm->csp = tmp_csp;
        vm->bp = tmp_bp;
        break;
    }
  }
}
