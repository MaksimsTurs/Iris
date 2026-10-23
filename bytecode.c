#include "bytecode.h"
#include "lexer.h"
#include "mem.h"
#include "parser.h"
#include "typedef.h"
#include "utils.h"
#include <endian.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

static void_t Iris_ByteCodeJmpTableAddEntry(Iris_ByteCode* bytecode, Iris_ByteCodeJmpTableEntry* entry)
{
  bytecode->jmp_table.entries[bytecode->jmp_table.len++] = *entry; 
}

static Iris_ByteCodeJmpTableEntry* Iris_ByteCodeJmpTableGetEntry(Iris_ByteCode* bytecode, Iris_Token* name)
{
  Iris_Token* token = NULL;

  for(i32_t i = 0; i < bytecode->jmp_table.len; i++)
  {
    token = bytecode->jmp_table.entries[i].name;
    
    if(Iris_StrCmprN(token->start, name->start, name->len))
    {
      return &bytecode->jmp_table.entries[i];
    }
  }

  return NULL;
}

static bool_t Iris_ByteCodeJmpTableHasEntry(Iris_ByteCode* bytecode, Iris_Token* name)
{
  Iris_Token* token = NULL;

  for(i32_t i = 0; i < bytecode->jmp_table.len; i++)
  {
    token = bytecode->jmp_table.entries[i].name;
    
    if(Iris_StrCmprN(token->start, name->start, name->len))
    {
      return true;
    }
  }

  return false;
}

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
         node->expr.bin.left->type != NODE_VAR) Iris_ByteCodeFindBottom(bytecode, node->expr.bin.left);
      if(node->expr.bin.right->type != NODE_NUMLIT &&
         node->expr.bin.right->type != NODE_VAR) Iris_ByteCodeFindBottom(bytecode, node->expr.bin.right);      
      break;
    case NODE_FN_DEF:
      for(i32_t i = 0; i < node->stmt.fn.len; i++)
      {
        Iris_ByteCodeFindBottom(bytecode, node->stmt.fn.nodes[i]);
      }
      break;
    case NODE_VAR:
      Iris_ByteCodeFindBottom(bytecode, node->stmt.var.val);
      break;
    case NODE_PROG:
      for(i32_t i = 0; i < node->stmt.prog.length; i++)
      {
        Iris_ByteCodeFindBottom(bytecode, node->stmt.prog.nodes[i]);
      }
      break;
  }
}

static void_t Iris_ByteCodePushOpCode(Iris_ByteCode* bytecode, Iris_OpCode opcode)
{
  bytecode->bytecode->buff[bytecode->bytecode->len] = (i8_t)opcode;
  bytecode->bytecode->len++;
}

static void_t Iris_ByteCodePushStackIndex(Iris_ByteCode* bytecode, i32_t index)
{
  memcpy(bytecode->bytecode->buff + bytecode->bytecode->len, &index, sizeof(index));
  bytecode->bytecode->len += sizeof(index);
}

static void_t Iris_ByteCodePushStackAddr(Iris_ByteCode* bytecode, i64_t addr)
{
  memcpy(bytecode->bytecode->buff + bytecode->bytecode->len, &addr, sizeof(addr));
  bytecode->bytecode->len += sizeof(addr);
}

static void_t Iris_ByteCodePushI64(Iris_ByteCode* bytecode, i64_t num)
{
  memcpy(bytecode->bytecode->buff + bytecode->bytecode->len, &num, sizeof(num));
  bytecode->bytecode->len += sizeof(num);
}

static void_t Iris_ByteCodeGenerateFromBottom(Iris_ByteCode* bytecode, Iris_AstNode* node)
{
  Iris_ByteCodeJmpTableEntry* entry = NULL;
  i64_t addr = 0;

  if(node->is_visited)
  {
    return;
  }

  node->is_visited = true;

  switch(node->type) 
  {
    case NODE_VAR:
      Iris_ByteCodePushOpCode(bytecode, OP_MOV);
      Iris_ByteCodePushStackIndex(bytecode, node->stmt.var.index);
      
      if(node->stmt.var.val->type != NODE_NUMLIT)
      {
        Iris_ByteCodePushI64(bytecode, node->stmt.var.val->expr.num);
      }
      else
      {
        Iris_ByteCodeGenerateFromBottom(bytecode, node->stmt.var.val);
      }
     break;
    case NODE_FN_CALL:
      Iris_ByteCodePushOpCode(bytecode, OP_CALL);
      entry = Iris_ByteCodeJmpTableGetEntry(bytecode, node->expr.call.name);

      if(entry)
      {
        addr = (i64_t)entry->start_ptr;
      }

      Iris_ByteCodePushStackAddr(bytecode, addr);
      break;
  }

  if(node && node->parent)
  {
    Iris_ByteCodeGenerateFromBottom(bytecode, node->parent);
  }
}

static void_t Iris_ByteCodeGenerateFnDef(Iris_ByteCode* bytecode, Iris_AstNode* node)
{
  Iris_FnDefStmt fn = node->stmt.fn;
  Iris_ByteCodeJmpTableEntry entry = {0};

  entry.name = fn.name;
  entry.ret_ptr = NULL;
  entry.start_ptr = bytecode->bytecode->buff + bytecode->bytecode->len;
  
  Iris_ByteCodeJmpTableAddEntry(bytecode, &entry);
  Iris_ByteCodePushOpCode(bytecode, OP_FN);
  Iris_ByteCodePushI64(bytecode, fn.stack_size);
}

static void_t Iris_ByteCodeGenerateFnEpilogue(Iris_ByteCode* bytecode, Iris_AstNode* node)
{
  Iris_FnDefStmt fn = node->stmt.fn;
  Iris_ByteCodeJmpTableEntry* entry = NULL;

  entry = Iris_ByteCodeJmpTableGetEntry(bytecode, fn.name);
  entry->ret_ptr = bytecode->bytecode->buff;
  Iris_ByteCodePushOpCode(bytecode, OP_RET);
  Iris_ByteCodePushI64(bytecode, fn.stack_size); 
}

void_t Iris_ByteCodeGenerate(Iris_ByteCode* bytecode, Iris_Parser* parser)
{
  Iris_ProgStmt prog = {0};

  bytecode->bytecode = Iris_MemBufferAlloc(1024, sizeof(i8_t));
  prog = parser->root->stmt.prog;

  for(i32_t i = 0; i < prog.length; i++)
  {
    switch(prog.nodes[i]->type)
    {
      case NODE_FN_DEF:
        bytecode->bottom = parser->root;

        Iris_ByteCodeGenerateFnDef(bytecode, prog.nodes[i]);
        Iris_ByteCodeFindBottom(bytecode, prog.nodes[i]);
        Iris_ByteCodeGenerateFromBottom(bytecode, bytecode->bottom);
        Iris_ByteCodeGenerateFnEpilogue(bytecode, prog.nodes[i]);
        break;
      case NODE_FN_CALL:
        bytecode->bottom = parser->root;

        Iris_ByteCodeFindBottom(bytecode, prog.nodes[i]);
        Iris_ByteCodeGenerateFromBottom(bytecode, bytecode->bottom);
        break;
    }
  }
}

void_t Iris_ByteCodeDump(const Iris_ByteCode* bytecode)
{
  i64_t operand = 0;
  i32_t index = 0;
  i8_t* buffer = bytecode->bytecode->buff;
  i8_t code = 0;

  for(i32_t i = 0; i < bytecode->bytecode->len; i++)
  {
    code = buffer[i];

    switch(code) 
    {
      case OP_MOV: 
        memcpy(&index, buffer + i + 1, sizeof(index));
        memcpy(&operand, buffer + i + 1 + sizeof(index), sizeof(operand));
        printf("\e[2mMov \e[m[sp + %i] %lli\n", index, operand);
        buffer += sizeof(index) + sizeof(operand);
        break;
      case OP_FN:
        memcpy(&operand, buffer + i + 1, sizeof(operand));
        printf("\e[2mFn \e[m%lli\n", operand);
        buffer += sizeof(operand);
        break;
      case OP_CALL:
        buffer+=i+1;
        printf("\e[2mFnCall \e[0m%lli\n", (i64_t)*((i64_t*)buffer));
        break;
      case OP_RET:
        memcpy(&operand, buffer + i + 1, sizeof(operand));
        printf("\e[2mRet \e[m%lli\n", operand);
        buffer += sizeof(operand);
        break;
    }
  }
}
