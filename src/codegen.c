#include "iris/codegen.h"
#include "iris/lexer.h"
#include "iris/opcodes.h"
#include "iris/parser.h"
#include "iris/typedef.h"
#include <stdio.h>

#define Iris_CodegenIsLit($node) (($node)->type == NODE_NULLLIT || ($node)->type == NODE_NANLIT || ($node)->type == NODE_INFINITYLIT || ($node)->type == NODE_BOOLLIT || ($node)->type == NODE_NUMLIT || ($node)->type == NODE_STRINGLIT || ($node)->type == NODE_STRCTLIT)

void_t Iris_CodegenGenerateStmtOrExpr(Iris_Codegen* codegen, Iris_Node* expr);

static inline bool_t Iris_CodegenIsSimpleExpr(Iris_Node* expr)
{
   return expr && (expr->type == NODE_STRCT_FN_ACCESS || expr->type == NODE_STRCT_PROP_ACCESS || expr->type == NODE_FN_CALL || expr->type == NODE_IDENT || expr->type == NODE_NUMLIT);
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
  if(type == OPR_PLUS)    return OPC_ADD;
  if(type == OPR_MINUS)   return OPC_SUB;
  if(type == OPR_MUL)     return OPC_MUL;
  if(type == OPR_DIV)     return OPC_DIV;
  if(type == OPR_EQ)      return OPC_CMPEQ;
  if(type == OPR_NOT_EQ)  return OPC_CMPNEQ;
  if(type == OPR_GT)      return OPC_CMPGT;
  if(type == OPR_LT)      return OPC_CMPLT;
  if(type == OPR_EQ_GT)   return OPC_CMPELT;
  if(type == OPR_EQ_LT)   return OPC_CMPEGT;
  if(type == OPR_OR)      return OPC_OR;
  if(type == OPR_AND)     return OPC_AND;
  // TODO: Handle error
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

static void_t Iris_CodegenBytecodePatchOpcode(Iris_Codegen* codegen, i64_t offset, Iris_Opcode opcode)
{
  *(Iris_Opcode*)(codegen->code.buff + offset) = opcode;
}

static void_t Iris_CodegenBytecodeWriteType(Iris_Codegen* codegen, Iris_Type type)
{
  Iris_CodegenReallocCodeIfNeeded(&codegen->code);
  *(Iris_Type*)(codegen->code.buff + codegen->code.len) = type;
  codegen->code.len += sizeof(Iris_Type);
}

static void_t Iris_CodegenBytecodeWriteInt64(Iris_Codegen* codegen, i64_t num)
{
  Iris_CodegenReallocCodeIfNeeded(&codegen->code);
  *(i64_t*)(codegen->code.buff + codegen->code.len) = num;
  codegen->code.len += sizeof(num);
}

static void_t Iris_CodegenBytecodePatchInt64(Iris_Codegen* codegen, i64_t offset, i64_t num)
{
  *(i64_t*)(codegen->code.buff + offset) = num;
}

static void_t Iris_CodegenBytecodeWriteFloat64(Iris_Codegen* codegen, f64_t num)
{
  Iris_CodegenReallocCodeIfNeeded(&codegen->code);
  *(f64_t*)(codegen->code.buff + codegen->code.len) = num;
  codegen->code.len += sizeof(num);
}

static void_t Iris_CodegenGenerateBlockStmt(Iris_Codegen* codegen, Iris_BlockStmt* block)
{
  for(i64_t i = 0; i < block->nodes.len; i++)
  {
    Iris_CodegenGenerateStmtOrExpr(codegen, block->nodes.buff[i]);
  }
}

static void Iris_CodegenGenerateIdentExpr(Iris_Codegen* codegen, Iris_Opcode opcode, const Iris_IdentExpr* expr)
{
  Iris_CodegenBytecodeWriteOpcode(codegen, opcode);
  Iris_CodegenBytecodeWriteType(codegen, TYPE_F64);
  Iris_CodegenBytecodeWriteInt64(codegen, expr->stack_offset);
}

static void_t Iris_CodegenGenerateNumLitExpr(Iris_Codegen* codegen, Iris_Opcode opcode, const Iris_Token* typename, Iris_LitExpr* expr)
{
  Iris_CodegenBytecodeWriteOpcode(codegen, opcode);
  Iris_CodegenBytecodeWriteType(codegen, TYPE_F64);
  Iris_CodegenBytecodeWriteFloat64(codegen, expr->f64);
}

static void_t Iris_CodegenGenerateCnstLit(Iris_Codegen* codegen, Iris_Opcode opcode, const Iris_Node* node)
{
  Iris_LitExpr* expr = node->as;

  Iris_CodegenBytecodeWriteOpcode(codegen, opcode);
  Iris_CodegenBytecodeWriteType(codegen, TYPE_I64);

  if(node->type == NODE_BOOLLIT)
  {
    Iris_CodegenBytecodeWriteFloat64(codegen, expr->bool);
  }
  else if(node->type == NODE_NULLLIT)
  {
    Iris_CodegenBytecodeWriteInt64(codegen, Iris_Nil);
  }
}

static void_t Iris_CodegenGenerateStrctPropAccessExpr(Iris_Codegen* codegen, Iris_Opcode opcode, Iris_Node* node)
{
  Iris_StrctPropAccessExpr* expr = node->as;

  if(node->type == NODE_STRCT_PROP_ACCESS)
  {
    Iris_CodegenBytecodeWriteOpcode(codegen, opcode);
    Iris_CodegenBytecodeWriteType(codegen, TYPE_F64);
    Iris_CodegenBytecodeWriteInt64(codegen, expr->stack_offset + expr->instrct_offset);
  }
  else
  {
    Iris_CodegenGenerateStmtOrExpr(codegen, expr->val);
  }
}

static void Iris_CodegenGenerateStrctLitExpr(Iris_Codegen* codegen, i64_t base_offset, const Iris_LitExpr* lit)
{
  Iris_StrctProp prop = {0};
  Iris_Opcode opcode = 0;

  for(i64_t i = 0; i < lit->strct.len; i++)
  {
    prop = lit->strct.buff[i];
    
    if(prop.val->type == NODE_IDENT)  opcode = OPC_PUSHSF;
    if(prop.val->type == NODE_NUMLIT) opcode = OPC_PUSHLT;
    if(prop.val->type == NODE_BIN)    opcode = OPC_PUSHOS;

    if(opcode == OPC_PUSHSF)
    {
      Iris_CodegenBytecodeWriteOpcode(codegen, opcode);
      Iris_CodegenBytecodeWriteType(codegen, TYPE_F64);
      Iris_CodegenBytecodeWriteInt64(codegen, ((Iris_IdentExpr*)prop.val->as)->stack_offset);
    }
    else if(opcode == OPC_PUSHLT)
    {
      Iris_CodegenBytecodeWriteOpcode(codegen, opcode);
      Iris_CodegenBytecodeWriteType(codegen, TYPE_F64);
      Iris_CodegenBytecodeWriteFloat64(codegen, ((Iris_LitExpr*)prop.val->as)->f64);
    }
    else if(prop.val->type == NODE_BIN)
    {
      Iris_CodegenGenerateStmtOrExpr(codegen, Iris_CodegenPrepareBinExpr(prop.val));
      Iris_CodegenBytecodeWriteOpcode(codegen, opcode);
      Iris_CodegenBytecodeWriteType(codegen, TYPE_F64);
    }
  }
}

static void_t Iris_CodegenGenerateBinExprNode(Iris_Codegen* codegen, const Iris_Node* node)
{
  if(node->type != NODE_NUMLIT &&
     node->type != NODE_IDENT)
  {
    Iris_CodegenGenerateStmtOrExpr(codegen, node);
  }
  else
  {
    if(node->type == NODE_NUMLIT)            Iris_CodegenGenerateNumLitExpr(codegen, OPC_LOADLT, null, node->as);
    if(node->type == NODE_IDENT)             Iris_CodegenGenerateIdentExpr(codegen, OPC_LOADSF, node->as);
    if(node->type == NODE_STRCT_PROP_ACCESS) Iris_CodegenGenerateStrctPropAccessExpr(codegen, OPC_LOADSF, node->as);
    if(node->type == NODE_STRCT_FN_ACCESS)   Iris_CodegenGenerateStrctPropAccessExpr(codegen, OPC_LOADSF, node->as);
  }
}

static void_t Iris_CodegenGenerateBinExpr(Iris_Codegen* codegen, const Iris_Node* expr)
{
  Iris_BinExpr* bin_expr = expr->as;
  Iris_IdentExpr* dst = null;

  if(bin_expr->op == OPR_ASSIGN)
  {
    dst = bin_expr->ls->as;

    if(bin_expr->rs->type == NODE_NUMLIT)
    {
      Iris_CodegenGenerateNumLitExpr(codegen, OPC_LOADLT, null, bin_expr->rs->as);
    }
    else if(bin_expr->rs->type == NODE_IDENT)
    {
      Iris_CodegenGenerateIdentExpr(codegen, OPC_LOADSF, bin_expr->rs->as);
    }
    else if(bin_expr->rs->type == NODE_FN_CALL || bin_expr->rs->type == NODE_BIN)
    {
      Iris_CodegenGenerateStmtOrExpr(codegen, bin_expr->rs);
      Iris_CodegenBytecodeWriteOpcode(codegen, OPC_PUSHOS);
      Iris_CodegenBytecodeWriteType(codegen, TYPE_F64);
    } 
  }
  else
  {
    Iris_CodegenGenerateBinExprNode(codegen, bin_expr->ls);
    Iris_CodegenGenerateBinExprNode(codegen, bin_expr->rs);
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
  if(Iris_CodegenIsLit(var_stmt->val))
  {
    if(var_stmt->val->type == NODE_NUMLIT)
    {
      Iris_CodegenGenerateNumLitExpr(codegen, OPC_PUSHLT, null, var_stmt->val->as);
    }
    else if(var_stmt->val->type == NODE_STRCTLIT)
    {
      Iris_CodegenGenerateStrctLitExpr(codegen, var_stmt->offset, var_stmt->val->as);
    }
  }
  else if(var_stmt->val->type == NODE_IDENT)
  {
    Iris_CodegenGenerateIdentExpr(codegen, OPC_PUSHSF, var_stmt->val->as);
  }
  else if(var_stmt->val->type == NODE_BIN)
  {
    Iris_CodegenGenerateStmtOrExpr(codegen, Iris_CodegenPrepareBinExpr(var_stmt->val));
    Iris_CodegenBytecodeWriteOpcode(codegen, OPC_PUSHOS);
    // TODO: Extract type from var stmt
    Iris_CodegenBytecodeWriteType(codegen, TYPE_F64);
  }
  else if(var_stmt->val->type == NODE_FN_CALL)
  {
    Iris_CodegenGenerateStmtOrExpr(codegen, var_stmt->val);
    Iris_CodegenBytecodeWriteOpcode(codegen, OPC_PUSHOS);
    Iris_CodegenBytecodeWriteType(codegen, TYPE_F64);
  }
  else if(var_stmt->val->type == NODE_STRCT_PROP_ACCESS || var_stmt->val->type == NODE_STRCT_FN_ACCESS)
  {
    Iris_CodegenGenerateStrctPropAccessExpr(codegen, OPC_PUSHSF, var_stmt->val);
  }
  else
  {
    Iris_CodegenGenerateStmtOrExpr(codegen, var_stmt->val);
  }
}

static void_t Iris_CodegenGenerateIfStmt(Iris_Codegen* codegen, Iris_IfStmt* stmt)
{
  i64_t apatch = 0;
  i64_t bpatch = 0;
  Iris_IfStmt* elsif = null;

  Iris_CodegenGenerateStmtOrExpr(codegen, stmt->expr);
  Iris_CodegenBytecodeWriteOpcode(codegen, OPC_JMPNIF);
  apatch = codegen->code.len;
  Iris_CodegenBytecodeWriteInt64(codegen, 0);
  Iris_CodegenGenerateStmtOrExpr(codegen, stmt->block);
  Iris_CodegenBytecodePatchInt64(codegen, apatch, codegen->code.len);

  for(i64_t i = 0; i < stmt->len; i++)
  {
    elsif = stmt->buff[i]->as;

    if(stmt->buff[i]->type == NODE_ELSE)
    {
      Iris_CodegenBytecodeWriteOpcode(codegen, OPC_JMP);
      bpatch = codegen->code.len;
      Iris_CodegenBytecodeWriteInt64(codegen, 0);
      Iris_CodegenBytecodePatchInt64(codegen, apatch, codegen->code.len);
      Iris_CodegenGenerateStmtOrExpr(codegen, elsif->block);
      Iris_CodegenBytecodePatchInt64(codegen, bpatch, codegen->code.len);
    }
    else
    {
      Iris_CodegenGenerateStmtOrExpr(codegen, elsif->expr);
      Iris_CodegenBytecodeWriteOpcode(codegen, OPC_JMPNIF);
      apatch = codegen->code.len;
      Iris_CodegenBytecodeWriteInt64(codegen, 0);
      Iris_CodegenGenerateStmtOrExpr(codegen, elsif->block);
      Iris_CodegenBytecodePatchInt64(codegen, apatch, codegen->code.len);
    }
  }
}

static void_t Iris_CodegenGenerateForStmt(Iris_Codegen* codegen, Iris_ForStmt* stmt)
{
  i64_t expr_offset = 0;
  i64_t exit_offset = 0;
  i64_t exit_jmp_offset = 0;
  Iris_VarStmt* var_stmt = stmt->init->as;

  // Init section
  Iris_CodegenGenerateStmtOrExpr(codegen, stmt->init);
  expr_offset = codegen->code.len;
  // Expression section
  Iris_CodegenGenerateStmtOrExpr(codegen, stmt->expr);
  Iris_CodegenBytecodeWriteOpcode(codegen, OPC_JMPNIF);
  exit_jmp_offset = codegen->code.len;
  Iris_CodegenBytecodeWriteInt64(codegen, 0);
  // Body and Increment section
  Iris_CodegenGenerateBlockStmt(codegen, stmt->block->as);
  Iris_CodegenGenerateStmtOrExpr(codegen, stmt->inc);
  Iris_CodegenBytecodePatchOpcode(codegen, codegen->code.len - 8, OPC_PUSHSF);
  Iris_CodegenBytecodeWriteInt64(codegen, var_stmt->offset);
  Iris_CodegenBytecodeWriteOpcode(codegen, OPC_JMP);
  Iris_CodegenBytecodeWriteInt64(codegen, expr_offset);
  Iris_CodegenBytecodePatchInt64(codegen, exit_jmp_offset, codegen->code.len);
}

void_t Iris_CodegenGenerateStmtOrExpr(Iris_Codegen* codegen, Iris_Node* stmt_or_expr)
{
  if(stmt_or_expr->is_visited)
  {
    return;
  }

  stmt_or_expr->is_visited = true;

  if(stmt_or_expr->type == NODE_IF)
  {
    Iris_CodegenGenerateIfStmt(codegen, stmt_or_expr->as);
  }
  else if(stmt_or_expr->type == NODE_FOR)
  {
    Iris_CodegenGenerateForStmt(codegen, stmt_or_expr->as);
  }
  else if(stmt_or_expr->type == NODE_BLCK)
  {
    Iris_CodegenGenerateBlockStmt(codegen, stmt_or_expr->as);
  }
  else if(stmt_or_expr->type == NODE_RET)
  {
    Iris_CodegenGenerateRetStmt(codegen, stmt_or_expr->as);
  }
  else if(stmt_or_expr->type == NODE_VAR)
  {
    Iris_CodegenGenerateVarStmt(codegen, stmt_or_expr->as);
  }
  else if(stmt_or_expr->type == NODE_NUMLIT)
  {
    Iris_CodegenGenerateNumLitExpr(codegen, OPC_LOADLT, null, stmt_or_expr->as);
  }
  else if(stmt_or_expr->type == NODE_BOOLLIT ||
          stmt_or_expr->type == NODE_NULLLIT)
  {
    Iris_CodegenGenerateCnstLit(codegen, OPC_LOADLT, stmt_or_expr);
  }
  else if(stmt_or_expr->type == NODE_IDENT)
  {
    Iris_CodegenGenerateIdentExpr(codegen, OPC_LOADSF, stmt_or_expr->as);
  }
  else if(stmt_or_expr->type == NODE_STRCT_PROP_ACCESS || stmt_or_expr->type == NODE_STRCT_FN_ACCESS)
  {
    Iris_CodegenGenerateStrctPropAccessExpr(codegen, OPC_LOADSF, stmt_or_expr);
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
    Iris_CodegenBytecodeWriteOpcode(codegen, OPC_PUSHOS);
    Iris_CodegenBytecodeWriteType(codegen, TYPE_F64);
  }
}

static void_t Iris_CodegenGenerateFnBody(Iris_Codegen* codegen, Iris_Node* fn_node)
{
  Iris_FnDefStmt* fn_def_stmt = (Iris_FnDefStmt*)fn_node->as;
  Iris_BlockStmt* block_stmt = fn_def_stmt->block->as;

  for(i64_t i = 0; i < block_stmt->nodes.len; i++)
  {
    Iris_CodegenGenerateStmtOrExpr(codegen, block_stmt->nodes.buff[i]);
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

const char_t* Iris_CodegenTypeToString(Iris_Type type)
{
  switch(type)
  {
    case TYPE_F64: return "Float64";
    case TYPE_I64: return "Int64";
  }
}

void_t Iris_CodegenDump(const Iris_Codegen* codegen)
{
  ptr_t ip = codegen->code.buff;
  i64_t a = 0;
  f64_t b = 0;
  Iris_Type type = 0;
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
      case OPC_PUSHLT:
        type = *(Iris_Type*)ip;
        ip += sizeof(Iris_Type);
        b = *(f64_t*)ip;
        ip += sizeof(f64_t);
        printf("\e[2m%s\e[0m %s %.2f\n", Iris_CodegenOpcToString(code), Iris_CodegenTypeToString(type), b);
        break;
      case OPC_PUSHSF:
        type = *(Iris_Type*)ip;
        ip += sizeof(Iris_Type);
        a = *(i64_t*)ip;
        ip += sizeof(i64_t);
        printf("\e[2m%s\e[0m %s %li\n", Iris_CodegenOpcToString(code), Iris_CodegenTypeToString(type), a);
        break;
      case OPC_PUSHOS:
        type = *(Iris_Type*)ip;
        ip += sizeof(Iris_Type);
        printf("\e[2m%s\e[0m %s\n", Iris_CodegenOpcToString(code), Iris_CodegenTypeToString(type));
        break;
      case OPC_LOADSF:
        type = *(Iris_Type*)ip;
        ip += sizeof(Iris_Type);
        a = *(i64_t*)ip;
        ip += sizeof(i64_t);
        printf("\e[2m%s\e[0m %s %li\n", Iris_CodegenOpcToString(code), Iris_CodegenTypeToString(type), a);
        break;
      case OPC_LOADLT:
        type = *(Iris_Type*)ip;
        ip += sizeof(Iris_Type);
        b = *(f64_t*)ip;
        ip += sizeof(f64_t);
        printf("\e[2m%s\e[0m %s %.2f\n", Iris_CodegenOpcToString(code), Iris_CodegenTypeToString(type), b);
        break;
      case OPC_JMP:
      case OPC_JMPIF:
      case OPC_JMPNIF:
        a = *(i64_t*)ip;
        ip += sizeof(i64_t);
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
    case OPC_FN:          return "Fn";
    case OPC_FN_CALL:     return "Call";
    case OPC_FN_RETVOID:  return "FnRet[Void]";
    case OPC_FN_RETVAL:   return "FnRet[Val]";
    case OPC_LOADLT:      return "Load[Lt]";
    case OPC_LOADSF:      return "Load[Sf]";
    case OPC_PUSHLT:      return "Push[Lt]";
    case OPC_PUSHSF:      return "Push[Sf]";
    case OPC_PUSHOS:      return "Push[Os]";
    case OPC_JMP:         return "Jmp";
    case OPC_JMPIF:       return "JmpIf";
    case OPC_JMPNIF:      return "JmpNIf";
    case OPC_CMPEQ:       return "CmpEq";
    case OPC_CMPNEQ:      return "CmpNeq";
    case OPC_CMPGT:       return "CmpGt";
    case OPC_CMPLT:       return "CmpLt";
    case OPC_CMPEGT:      return "CmpEGt";
    case OPC_CMPELT:      return "CmpELt";
    case OPC_OR:          return "Or";
    case OPC_AND:         return "And";
    case OPC_PUSHCNOS:    return "Push[CNOS]";
    case OPC_PUSHCNSF:    return "Push[CNSF]";
    case OPC_MOVOSSF:     return "Mov[OSSF]";
    case OPC_COPYSFOS:    return "Copy[SFOS]";
    case OPC_COPYSFSF:    return "Copy[SFSF]";
    case OPC_ADD:         return "Add";
    case OPC_SUB:         return "Sub";
    case OPC_MUL:         return "Mul";
    case OPC_DIV:         return "Div";
    default:
      return "Unknown Opcode";
  }
}
