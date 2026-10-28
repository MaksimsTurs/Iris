#include "bytecode.h"
#include "lexer.h"
#include "mem.h"
#include "opcodes.h"
#include "parser.h"
#include "typedef.h"
#include "utils.h"
#include <endian.h>
#include <inttypes.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

static void_t Iris_ByteCodeFindBottom(Iris_ByteCode* bytecode, Iris_AstNode* node)
{
  if(node->parent)
  {
    node->depth = node->parent->depth + 1;
  }

  if(node->depth > bytecode->bottom->depth)
  {
    bytecode->bottom = node;
  }

  switch(node->type) 
  {
    case NODE_BINARY:
      if(node->expr.bin.left->type != NODE_NUMLIT &&
         node->expr.bin.left->type != NODE_IDENT &&
         !Iris_ParserIsNodeFnArg(node->expr.bin.left))
      {
        Iris_ByteCodeFindBottom(bytecode, node->expr.bin.left);
      }
      if(node->expr.bin.right->type != NODE_NUMLIT &&
         node->expr.bin.right->type != NODE_IDENT &&
         !Iris_ParserIsNodeFnArg(node->expr.bin.right))
      {
        Iris_ByteCodeFindBottom(bytecode, node->expr.bin.right);
      }
      break;
    case NODE_FN_DEF:
      for(i32_t i = 0; i < node->stmt.fn.block.len; i++)
      {
        Iris_ByteCodeFindBottom(bytecode, node->stmt.fn.block.nodes[i]);
    }
      break;
    case NODE_VAR:
      if(node->stmt.var.val != null)
      {
        Iris_ByteCodeFindBottom(bytecode, node->stmt.var.val);
      }
      break;
    case NODE_PROG:
      for(i32_t i = 0; i < node->stmt.prog.length; i++)
      {
        Iris_ByteCodeFindBottom(bytecode, node->stmt.prog.nodes[i]);
      }
      break;
  }
}

static void_t Iris_ByteCodePushOpCode(Iris_ByteCode* bytecode, Iris_Opcode opcode)
{
  *(Iris_Opcode*)(bytecode->bytecode->buff + bytecode->bytecode->len) = opcode;
  bytecode->bytecode->len += sizeof(opcode);
}

static void_t Iris_ByteCodePushI64(Iris_ByteCode* bytecode, i64_t num)
{
  *(i64_t*)(bytecode->bytecode->buff + bytecode->bytecode->len) = num;
  bytecode->bytecode->len += sizeof(num);
}

static Iris_Opcode Iris_ByteCodeOperatorToOpCode(Iris_OperType type)
{
  switch(type) 
  {
    case OPER_PLUS:   return OPC_ADD;
    case OPER_MINUS:  return OPC_SUB;
    case OPER_MUL:    return OPC_MUL;
    case OPER_DIV:    return OPC_DIV;
  }
}

static void_t Iris_ByteCodeGenerateBinExpr(Iris_ByteCode* bytecode, Iris_AstNode* node)
{
  if(node->is_visited)
  {
    return;
  };

  node->is_visited = true;

  switch(node->type)
  {
    case NODE_BINARY:
      Iris_ByteCodeGenerateBinExpr(bytecode, node->expr.bin.left);
      Iris_ByteCodeGenerateBinExpr(bytecode, node->expr.bin.right);

      Iris_ByteCodePushOpCode(bytecode, Iris_ByteCodeOperatorToOpCode(node->expr.bin.op));
      
      if(node->parent)
      {
        Iris_ByteCodeGenerateBinExpr(bytecode, node->parent);
      }
      break;
    case NODE_NUMLIT:
      Iris_ByteCodePushOpCode(bytecode, OPC_PUSHNLOS);
      Iris_ByteCodePushI64(bytecode, node->expr.num);
      break;
    case NODE_IDENT:
      Iris_ByteCodePushOpCode(bytecode, OPC_PUSHSFOS);
      Iris_ByteCodePushI64(bytecode, node->expr.ident.index);
      break;
  }
}

void_t Iris_ByteCodeGenerateStmtOrExpr(Iris_ByteCode* bytecode, Iris_AstNode* node)
{
  if(node->is_visited)
  {
    return;
  }

  node->is_visited = true;

  switch(node->type) 
  {
    case NODE_VAR: 
      if(!Iris_ParserIsNodeFnArg(node))
      {
        switch(node->stmt.var.val->type)
        {
          case NODE_NUMLIT:
            Iris_ByteCodePushOpCode(bytecode, OPC_PUSHNLSF);
            Iris_ByteCodePushI64(bytecode, node->stmt.var.val->expr.num);
            break;
          case NODE_IDENT:
            Iris_ByteCodePushOpCode(bytecode, OPC_PUSHSFSF);
            Iris_ByteCodePushI64(bytecode, node->stmt.var.val->expr.ident.index);
            break;
          case NODE_BINARY:
            bytecode->bottom = bytecode->root;
            Iris_ByteCodeFindBottom(bytecode, node->stmt.var.val);
            Iris_ByteCodeGenerateBinExpr(bytecode, bytecode->bottom);
            Iris_ByteCodePushOpCode(bytecode, OPC_MOVOSSF);
            Iris_ByteCodePushI64(bytecode, node->stmt.var.index);
            break;
        }
      }
      break;
    break;
  }
}

static void_t Iris_ByteCodeGenerateFnPrologue(Iris_ByteCode* bytecode, Iris_AstNode* node)
{
  Iris_FnDefStmt *fn = &node->stmt.fn;

  fn->stack_size += sizeof(i64_t); // rp
  fn->stack_size += sizeof(i64_t); // sp
  fn->stack_size += sizeof(i64_t); // csp
  fn->stack_size += sizeof(i64_t); // bp

  Iris_ByteCodePushOpCode(bytecode, OPC_PROLOG);
  Iris_ByteCodePushI64(bytecode, fn->stack_size);  
}

static void_t Iris_ByteCodeGenerateFnBody(Iris_ByteCode* bytecode, Iris_FnDefStmt* fn)
{
  Iris_RetStmt* ret_stmt = null;
  Iris_AstNode* maybe_ret = fn->block.nodes[fn->block.len - 1];
  i64_t end = 0;

  if(maybe_ret->type == NODE_RET)
  {
    ret_stmt = &maybe_ret->stmt.ret;
  }
  
  end = ret_stmt ? fn->block.len - 1 : fn->block.len;
  
  for(i64_t i = 0; i < end; i++)
  {
    Iris_ByteCodeGenerateStmtOrExpr(bytecode, fn->block.nodes[i]);
  }
}

static void_t Iris_ByteCodeGenerateFnEpilogue(Iris_ByteCode* bytecode, Iris_AstNode* node)
{
  Iris_FnDefStmt fn = node->stmt.fn;

  Iris_ByteCodePushOpCode(bytecode, OPC_RETVOID);
}

void_t Iris_ByteCodeInit(Iris_ByteCode *bytecode)
{
  bytecode->bytecode = Iris_ArrBuffAlloc(1024, sizeof(i8_t));
  bytecode->bottom = null;
  bytecode->depth = -1;
}

void_t Iris_ByteCodeGenerate(Iris_ByteCode* bytecode, Iris_Parser* parser)
{
  Iris_BlockStmt prog = {0};

  prog = parser->root->stmt.block;
  bytecode->root = parser->root;

  for(i32_t i = 0; i < prog.len; i++)
  {
    switch(prog.nodes[i]->type)
    {
      case NODE_FN_DEF:
        bytecode->bottom = parser->root;
        bytecode->depth = -1;

        Iris_ByteCodeGenerateFnPrologue(bytecode, prog.nodes[i]);
        Iris_ByteCodeGenerateFnBody(bytecode, &prog.nodes[i]->stmt.fn);
        Iris_ByteCodeGenerateFnEpilogue(bytecode, prog.nodes[i]);
        break;
    }
  }
}

static const i8_t* Iris_ByteCodeOpCodeToString(Iris_Opcode opcode)
{
  switch(opcode) 
  {
    case OPC_PROLOG:
      return "Prolog";
    case OPC_PUSHNLOS:
      return "Push[NLOS]";
    case OPC_PUSHNLSF:
      return "Push[NLSF]";
    case OPC_PUSHSFOS:
      return "Push[SFOS]";
    case OPC_PUSHSFSF:
      return "Push[SFSF]";
    case OPC_MOVOSSF:
      return "Mov[OSSF]";
    case OPC_RETVAL:
      return "Ret[Val]";
    case OPC_RETVOID:
      return "Ret[Void]";
    case OPC_ADD:
      return "Add";
    case OPC_SUB:
      return "Sub";
    case OPC_MUL:
      return "Mul";
    case OPC_DIV:
      return "Div";
  }
}

void_t Iris_ByteCodeDump(const Iris_ByteCode* bytecode)
{
  ptr_t ip = bytecode->bytecode->buff;
  i64_t a = 0;
  i64_t b = 0;
  Iris_Opcode code = 0;

  printf("Size of bytecode: %li\n", bytecode->bytecode->len);

  while(ip - bytecode->bytecode->buff < bytecode->bytecode->len)
  {
    code = *(Iris_Opcode*)ip;
    ip += sizeof(code);
    
    switch(code) 
    {
      case OPC_PROLOG:
        a = *(i64_t*)ip;
        ip += sizeof(a);
        printf("\e[2m%s\e[0m %li\n", Iris_ByteCodeOpCodeToString(code), a);
        break;
      case OPC_PUSHNLOS:
        a = *(i64_t*)ip;
        ip += sizeof(a);
        printf("\e[2m%s\e[0m %li\n", Iris_ByteCodeOpCodeToString(code), a);
        break;
      case OPC_PUSHNLSF:
        a = *(i64_t*)ip;
        ip += sizeof(a);
        printf("\e[2m%s\e[0m %li\n", Iris_ByteCodeOpCodeToString(code), a);
        break;
      case OPC_PUSHSFSF:
        a = *(i64_t*)ip;
        ip += sizeof(a);
        printf("\e[2m%s\e[0m %li\n", Iris_ByteCodeOpCodeToString(code), a);
        break;
      case OPC_PUSHSFOS:
        a = *(i64_t*)ip;
        ip += sizeof(a);
        printf("\e[2m%s\e[0m %li\n", Iris_ByteCodeOpCodeToString(code), a);
        break;
      case OPC_MOVOSSF:
        a = *(i64_t*)ip;
        ip += sizeof(a);
        printf("\e[2m%s\e[0m %li\n", Iris_ByteCodeOpCodeToString(code), a);
        break;
      case OPC_RETVAL:
      case OPC_RETVOID:
        printf("\e[2m%s\e[0m\n", Iris_ByteCodeOpCodeToString(code));
        break;
      case OPC_ADD:
      case OPC_MUL:
      case OPC_SUB:
      case OPC_DIV:
        printf("\e[2m%s\e[0m\n", Iris_ByteCodeOpCodeToString(code));
        break;
    }
  }
}
