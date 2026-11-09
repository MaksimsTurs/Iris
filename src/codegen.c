#include "iris/codegen.h"

void_t Iris_CodegenGenerateStmtOrExpr(Iris_Codegen* codegen, Iris_Node* expr);

static inline bool_t Iris_CodegenIsSimpleExpr(Iris_Node* expr)
{
   return expr && (expr->type == NODE_FN_CALL || expr->type == NODE_IDENT || expr->type == NODE_NUMLIT);
}

static void_t Iris_CodegenReallocCodeIfNeeded(Iris_Code* code)
{
  if(code->len + 8 > code->cap)
  {
    Iris_ArrayExpand(*code, code->cap * 2);
  }
}

static Iris_Opcode Iris_CodegenOprTypeToOpcode(Iris_OprType type)
{
  if(type == OPR_PLUS)  return OPC_ADD;
  if(type == OPR_MINUS) return OPC_SUB;
  if(type == OPR_MUL)   return OPC_MUL;
  if(type == OPR_DIV)   return OPC_DIV;
  // Throw error or something.
}

static void_t Iris_CodegenFindBottom(Iris_Node* node, Iris_NodeDepth* deepest, i64_t parent_depth)
{
  Iris_NodeDepth curr_node = {0};
  Iris_VarStmt* var_stmt = null;
  Iris_BinExpr* bin_expr = null;
  Iris_FnCallExpr* fn_call_expr = null;

  curr_node.depth = parent_depth + 1;

  if(deepest->depth < curr_node.depth)
  {
    deepest->depth = curr_node.depth;
    deepest->node = node;
  }

  if(node->type == NODE_BIN)
  {
    bin_expr = (Iris_BinExpr*)node->as;
    
    if(!Iris_CodegenIsSimpleExpr(bin_expr->ls))
    {
      Iris_CodegenFindBottom(bin_expr->ls, deepest, curr_node.depth);
    }

    if(!Iris_CodegenIsSimpleExpr(bin_expr->rs))
    {
      Iris_CodegenFindBottom(bin_expr->rs, deepest, curr_node.depth);
    }
  }
  else if(node->type == NODE_FN_CALL)
  {
    fn_call_expr = (Iris_FnCallExpr*)node->as;
      
    for(i16_t i = 0; i < fn_call_expr->args.len; i++)
    {
      if(!Iris_CodegenIsSimpleExpr(fn_call_expr->args.buff[i]))
      {
        Iris_CodegenFindBottom(fn_call_expr->args.buff[i], deepest, curr_node.depth);
      }
    }
  }
  else if(node->type == NODE_VAR)
  {
    var_stmt = (Iris_VarStmt*)node->as;
      
    if(var_stmt->val)
    {
      Iris_CodegenFindBottom(var_stmt->val, deepest, curr_node.depth);
    }
  }
}

static Iris_Node* Iris_CodegenPrepareBinExpr(Iris_Node* start)
{
  Iris_NodeDepth deepest = {0};
  Iris_NodeDepth node_depth = {0};

  node_depth.node = start;
  node_depth.depth = 0;
  deepest.depth = 0;
  deepest.node = start;
  Iris_CodegenFindBottom(start, &deepest, 0);

  return deepest.node;
}

static Iris_JMPEntry* Iris_JMPGetFnEntry(Iris_JMPEntryArray* jmps, const Iris_Token* name)
{
  Iris_JMPEntry* entry = null;

  for(i64_t i = 0; i < jmps->len; i++)
  {
    entry = Iris_ArrayAt(*jmps, i);

    if(Iris_StrnCmp(name->start, entry->fn->name->start, Iris_Max(name->len, entry->fn->name->len)))
    {
      return entry;
    }
  }

  return null;
}

static void_t Iris_JMPSetFnEntry(Iris_JMPEntryArray* jmps, Iris_JMPEntry* entry)
{
  Iris_ArrayPush(*jmps, *entry);
}

static void_t Iris_CodegenBytecodeWriteOpcode(Iris_Codegen* codegen, Iris_Opcode opcode)
{
  Iris_CodegenReallocCodeIfNeeded(&codegen->code);
  *(Iris_Opcode*)(codegen->code.buff + codegen->code.len) = opcode;
  codegen->code.len += sizeof(Iris_Opcode);
}

static void_t Iris_CodegenBytecodeWriteInt64(Iris_Codegen* codegen, i64_t num)
{
  Iris_CodegenReallocCodeIfNeeded(&codegen->code);
  *(i64_t*)(codegen->code.buff + codegen->code.len) = num;
  codegen->code.len += sizeof(num);
}

static void_t Iris_CodegenBytecodeWriteFloat64(Iris_Codegen* codegen, f64_t num)
{
  Iris_CodegenReallocCodeIfNeeded(&codegen->code);
  *(f64_t*)(codegen->code.buff + codegen->code.len) = num;
  codegen->code.len += sizeof(num);
}

static void Iris_CodegenGenerateLitExpr(Iris_Codegen* codegen, Iris_Opcode opcode, const Iris_LitExpr* expr)
{
  Iris_CodegenBytecodeWriteOpcode(codegen, opcode);
  Iris_CodegenBytecodeWriteFloat64(codegen, expr->f64);
}

static void Iris_CodegenGenerateIdentExpr(Iris_Codegen* codegen, Iris_Opcode opcode, const Iris_IdentExpr* expr)
{
  Iris_CodegenBytecodeWriteOpcode(codegen, opcode);
  Iris_CodegenBytecodeWriteInt64(codegen, expr->stack_offset);
}

static void_t Iris_CodegenGenerateBinExpr(Iris_Codegen* codegen, const Iris_Node* expr)
{
  Iris_BinExpr* bin_expr = (Iris_BinExpr*)expr->as;
  Iris_IdentExpr* dst = null;
  Iris_Node* rs = null;

  if(bin_expr->op == OPR_ASSIGN)
  {
    dst = bin_expr->ls->as;
    rs = bin_expr->rs;

    if(rs->type == NODE_NUMLIT)
    {
      Iris_CodegenGenerateLitExpr(codegen, OPC_MOVNLSF, rs->as);
    }
    else if(rs->type == NODE_IDENT)
    {
      Iris_CodegenGenerateIdentExpr(codegen, OPC_COPYSFSF, rs->as);
    }
    else if(rs->type == NODE_FN_CALL || rs->type == NODE_BIN)
    {
      Iris_CodegenGenerateStmtOrExpr(codegen, rs);
      Iris_CodegenBytecodeWriteOpcode(codegen, OPC_MOVOSSF);
      Iris_CodegenBytecodeWriteInt64(codegen, dst->stack_offset);
    } 
  }
  else
  {
    Iris_CodegenGenerateStmtOrExpr(codegen, bin_expr->ls);
    Iris_CodegenGenerateStmtOrExpr(codegen, bin_expr->rs);
    Iris_CodegenBytecodeWriteOpcode(codegen, Iris_CodegenOprTypeToOpcode(bin_expr->op));

    if(expr->parent)
    {
      Iris_CodegenGenerateStmtOrExpr(codegen, expr->parent);
    }
  }
}

static void Iris_CodegenGenerateFnCallExpr(Iris_Codegen* codegen, const Iris_FnCallExpr* expr)
{
  Iris_JMPEntry* jmp_entry = null;

  for(i16_t i = expr->args.len - 1; i >= 0; i--)
  {
    Iris_CodegenGenerateStmtOrExpr(codegen, expr->args.buff[i]);
  }
  
  jmp_entry = Iris_JMPGetFnEntry(&codegen->jmps, expr->name);
  
  Iris_CodegenBytecodeWriteOpcode(codegen, OPC_FN_CALL);
  Iris_CodegenBytecodeWriteInt64(codegen, jmp_entry->start_offset);
}

static void_t Iris_CodegenGenerateRetStmt(Iris_Codegen* codegen, const Iris_RetStmt* ret_stmt)
{
  if(ret_stmt->val)
  {
    if(ret_stmt->val->type == NODE_BIN)
    {
      Iris_CodegenGenerateStmtOrExpr(codegen, Iris_CodegenPrepareBinExpr(ret_stmt->val));
    }
    else
    {
      Iris_CodegenGenerateStmtOrExpr(codegen, ret_stmt->val);
    }
  }
}

static void_t Iris_CodegenGenerateVarStmt(Iris_Codegen* codegen, const Iris_VarStmt* var_stmt)
{
  if(var_stmt->val->type == NODE_NUMLIT)
  {
   Iris_CodegenGenerateLitExpr(codegen, OPC_MOVNLSF, var_stmt->val->as);
  }
  else if(var_stmt->val->type == NODE_IDENT)
  {
    Iris_CodegenGenerateIdentExpr(codegen, OPC_COPYSFSF, var_stmt->val->as);
  }
  else if(var_stmt->val->type == NODE_BIN)
  {
    Iris_CodegenGenerateStmtOrExpr(codegen, Iris_CodegenPrepareBinExpr(var_stmt->val));
    Iris_CodegenBytecodeWriteOpcode(codegen, OPC_MOVOSSF);
    Iris_CodegenBytecodeWriteInt64(codegen, var_stmt->stack_offset);
  }
  else if(var_stmt->val->type == NODE_FN_CALL)
  {
    Iris_CodegenGenerateStmtOrExpr(codegen, var_stmt->val);
    Iris_CodegenBytecodeWriteOpcode(codegen, OPC_MOVOSSF);
    Iris_CodegenBytecodeWriteInt64(codegen, var_stmt->stack_offset);
  }
}

void_t Iris_CodegenGenerateStmtOrExpr(Iris_Codegen* codegen, Iris_Node* stmt_or_expr)
{
  if(stmt_or_expr->is_visited)
  {
    return;
  }

  stmt_or_expr->is_visited = true;

  if(stmt_or_expr->type == NODE_RET)
  {
    Iris_CodegenGenerateRetStmt(codegen, stmt_or_expr->as);
  }
  else if(stmt_or_expr->type == NODE_VAR)
  {
    Iris_CodegenGenerateVarStmt(codegen, stmt_or_expr->as);
  }
  else if(stmt_or_expr->type == NODE_NUMLIT)
  {
    Iris_CodegenGenerateLitExpr(codegen, OPC_MOVNLOS, stmt_or_expr->as);
  }
  else if(stmt_or_expr->type == NODE_IDENT)
  {
    Iris_CodegenGenerateIdentExpr(codegen, OPC_COPYSFOS, stmt_or_expr->as);
  }
  else if(stmt_or_expr->type == NODE_BIN)
  {
    Iris_CodegenGenerateBinExpr(codegen, stmt_or_expr);
  }
  else if(stmt_or_expr->type == NODE_FN_CALL)
  {
    Iris_CodegenGenerateFnCallExpr(codegen, stmt_or_expr->as);
  }
}

static void_t Iris_CodegenGenerateFnPrologue(Iris_Codegen* codegen, Iris_Node* fn_node)
{
  Iris_FnDefStmt *fn_def_stmt = (Iris_FnDefStmt*)fn_node->as;
  Iris_JMPEntry fn_entry = {0};
  
  fn_entry.fn = fn_def_stmt;
  fn_entry.start_offset = codegen->code.len;
  Iris_JMPSetFnEntry(&codegen->jmps, &fn_entry);
  Iris_CodegenBytecodeWriteOpcode(codegen, OPC_FN);
  Iris_CodegenBytecodeWriteInt64(codegen, fn_def_stmt->stack_size);

  if(Iris_StrnCmp(fn_def_stmt->name->start, "main", Iris_Max(fn_def_stmt->name->len, 4)))
  {
    codegen->main_offset = fn_entry.start_offset;
  }

  for(i8_t i = 0; i < fn_def_stmt->args_count; i++)
  {
    Iris_CodegenBytecodeWriteOpcode(codegen, OPC_MOVOSSF);
    Iris_CodegenBytecodeWriteInt64(codegen, i + 4);
  }
}

static void_t Iris_CodegenGenerateFnBody(Iris_Codegen* codegen, Iris_Node* fn_node)
{
  Iris_FnDefStmt* fn_def_stmt = (Iris_FnDefStmt*)fn_node->as;
  Iris_BlockStmt block_stmt = fn_def_stmt->block;

  for(i64_t i = 0; i < block_stmt.nodes.len; i++)
  {
    Iris_CodegenGenerateStmtOrExpr(codegen, block_stmt.nodes.buff[i]);
  }
}

static void_t Iris_CodegenGenerateFnEpilogue(Iris_Codegen* codegen, Iris_Node* fn_node)
{
  Iris_FnDefStmt* fn_def_stmt = (Iris_FnDefStmt*)fn_node->as;

  if(fn_def_stmt->has_ret)
  {
    Iris_CodegenBytecodeWriteOpcode(codegen, OPC_FN_RETVAL);
  }
  else
  {
    Iris_CodegenBytecodeWriteOpcode(codegen, OPC_FN_RETVOID);
  }
}
 
void_t Iris_CodegenInit(Iris_Codegen* codegen, Iris_Parser* parser)
{
  codegen->root = parser->root;
  Iris_ArrayExpand(codegen->code, 1024);
  Iris_ArrayNew(codegen->jmps, 8);
}

void_t Iris_CodegenGenerate(Iris_Codegen* codegen)
{
  Iris_BlockStmt* global = (Iris_BlockStmt*)codegen->root->as;

  for(i64_t i = 0; i < global->nodes.len; i++)
  {
    switch(global->nodes.buff[i]->type)
    {
      case NODE_FN_DEF:
        Iris_CodegenGenerateFnPrologue(codegen, global->nodes.buff[i]);
        Iris_CodegenGenerateFnBody(codegen, global->nodes.buff[i]);
        Iris_CodegenGenerateFnEpilogue(codegen, global->nodes.buff[i]);
        break;
    }
  }
}

void_t Iris_CodegenDump(const Iris_Codegen* codegen)
{
  ptr_t ip = codegen->code.buff;
  i64_t a = 0;
  f64_t b = 0;
  Iris_Opcode code = 0;

  printf("Size of bytecode: %li\n", codegen->code.len);

  while(ip - (ptr_t)codegen->code.buff < codegen->code.len)
  {
    code = *(Iris_Opcode*)ip;
    ip += sizeof(code);

    switch(code) 
    {
      case OPC_FN:
        a = *(i64_t*)ip;
        ip += sizeof(a);
        printf("\e[2m%s\e[0m %li\n", Iris_CodegenOpcToString(code), a);
        break;
      case OPC_FN_CALL:
        a = *(i64_t*)ip;
        ip += sizeof(a);
        printf("\e[2m%s\e[0m %li\n", Iris_CodegenOpcToString(code), a);
        break;
      case OPC_MOVNLOS:
        b = *(f64_t*)ip;
        ip += sizeof(b);
        printf("\e[2m%s\e[0m %.2f\n", Iris_CodegenOpcToString(code), b);
        break;
      case OPC_MOVNLSF:
        b = *(f64_t*)ip;
        ip += sizeof(a);
        printf("\e[2m%s\e[0m %.2f\n", Iris_CodegenOpcToString(code), b);
        break;
      case OPC_COPYSFSF:
        a = *(i64_t*)ip;
        ip += sizeof(a);
        printf("\e[2m%s\e[0m %li\n", Iris_CodegenOpcToString(code), a);
        break;
      case OPC_COPYSFOS:
        a = *(i64_t*)ip;
        ip += sizeof(a);
        printf("\e[2m%s\e[0m %li\n", Iris_CodegenOpcToString(code), a);
        break;
      case OPC_MOVOSSF:
        a = *(i64_t*)ip;
        ip += sizeof(a);
        printf("\e[2m%s\e[0m %li\n", Iris_CodegenOpcToString(code), a);
        break;
      case OPC_FN_RETVAL:
      case OPC_FN_RETVOID:
        printf("\e[2m%s\e[0m\n", Iris_CodegenOpcToString(code));
        break;
      case OPC_ADD:
      case OPC_MUL:
      case OPC_SUB:
      case OPC_DIV:
        printf("\e[2m%s\e[0m\n", Iris_CodegenOpcToString(code));
        break;
      default:
        printf("\e[2m%s\e[0m\n", Iris_CodegenOpcToString(code));
    }
  }
}

const i8_t* Iris_CodegenOpcToString(Iris_Opcode opcode)
{
  switch(opcode) 
  {
    case OPC_FN:
      return "Fn";
    case OPC_FN_CALL:
      return "Call";
    case OPC_FN_RETVOID:
      return "FnRet[Void]";
    case OPC_FN_RETVAL:
      return "FnRet[Val]";
    case OPC_MOVNLOS:
      return "Mov[NLOS]";
    case OPC_MOVNLSF:
      return "Mov[NLSF]";
    case OPC_MOVOSSF:
      return "Mov[OSSF]";
    case OPC_COPYSFOS:
      return "Copy[SFOS]";
    case OPC_COPYSFSF:
      return "Copy[SFSF]";
    case OPC_ADD:
      return "Add";
    case OPC_SUB:
      return "Sub";
    case OPC_MUL:
      return "Mul";
    case OPC_DIV:
      return "Div";
    default:
      return "Unknown Opcode";
  }
}
