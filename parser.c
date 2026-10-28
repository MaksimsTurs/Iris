#include "parser.h"
#include "lexer.h"
#include "mem.h"
#include "report.h"
#include "typedef.h"
#include "utils.h"
#include <inttypes.h>
#include <linux/limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

Iris_AstNode* Iris_ParserParseWeakExpr(Iris_Parser* parser, Iris_Lexer* lex, Iris_Scope* scope);

static const i8_t* Iris_ParserAstNodeToString(Iris_AstNodeType type)
{
  switch(type) 
  {
    case NODE_PROG:
      return "Prog";
    case NODE_BINARY:
      return "Bin";
    case NODE_NUMLIT:
      return "NumLit";
    case NODE_VAR:
      return "Var";
    case NODE_FN_DEF:
      return "FnDef";
    case NODE_FN_CALL:
      return "FnCall";
    case NODE_RET:
      return "Ret";
    case NODE_IDENT:
      return "Ident";
    case NODE_UNKNOWN:
    default:
      return "Unknown";
  }
}

static const i8_t* Iris_ParserOperTypeToString(Iris_OperType type)
{
  switch(type)
  {
    case OPER_PLUS:
      return "+";
    case OPER_MINUS:
      return "-";
    case OPER_MUL:
      return "*";
    case OPER_DIV:
      return "/";
    case OPER_EQ:
      return "==";
    case OPER_NOT_EQ:
      return "!=";
    case OPER_EQ_GT:
      return ">=";
    case OPER_EQ_LT:
      return "<=";
    case OPER_GT:
      return ">";
    case OPER_LT:
      return "<";
    case OPER_UNKNOWN:
    default:
      return "??";
  }
}

static Iris_OperType Iris_ParserTokenTypeToOperType(Iris_TokenType type)
{
  switch(type)
  {
    case TK_PLUS:
      return OPER_PLUS;
    case TK_MINUS:
      return OPER_MINUS;
    case TK_MUL:
      return OPER_MUL;
    case TK_DIV:
      return OPER_DIV;
    case TK_LT:
      return OPER_LT;
    case TK_GT:
      return OPER_GT;
    case TK_EQ:
      return OPER_EQ;
    case TK_NOT_EQ:
      return OPER_NOT_EQ;
    case TK_EQ_GT:
      return OPER_EQ_GT;
    case TK_EQ_LT:
      return OPER_EQ_LT;
    case TK_UNKNOWN:
    default:
      return OPER_UNKNOWN;
  }
}

static Iris_AstNode* Iris_ParserCreateNode(Iris_Parser* parser, Iris_AstNodeType type)
{
  Iris_AstNode node = {0};
  
  node.depth = -1;
  node.parent = null;
  node.type = type;
  node.is_visited = false;

  return (Iris_AstNode*)Iris_ArrBuffPush(parser->nodes, &node);
}

static Iris_AstNode* Iris_ParserCreateBinExpr(Iris_Parser* parser, Iris_AstNode* left, Iris_OperType op, Iris_AstNode* right)
{
  Iris_AstNode* node = Iris_ParserCreateNode(parser, NODE_BINARY);

  left->parent = node;
  right->parent = node;
  node->expr.bin.left = left;
  node->expr.bin.op = op;
  node->expr.bin.right = right;
  

  return node;
}

static Iris_Scope Iris_ParserCreateScope(Iris_Scope* parent)
{
  Iris_Scope scope = {0};

  scope.fns = Iris_ArrBuffAlloc(64, sizeof(Iris_FnDefStmt));
  scope.vars = Iris_ArrBuffAlloc(24, sizeof(Iris_VarStmt));
  scope.parent = parent;

  return scope;
}

static Iris_AstNode* Iris_ParserCreateFnDefStmt(Iris_Parser* parser, Iris_Token* name, Iris_Scope* scope)
{
  Iris_AstNode* node = Iris_ParserCreateNode(parser, NODE_FN_DEF);

  node->stmt.fn.name = name;
  node->stmt.fn.stack_size = 0;
  node->stmt.fn.block.len = 0;
  node->stmt.fn.block.scope = Iris_ParserCreateScope(scope);

  return node;
}

static Iris_AstNode* Iris_ParserCreateBlockStmt(Iris_Parser* parser, Iris_Scope* parent)
{
  Iris_AstNode* node = Iris_ParserCreateNode(parser, NODE_BLOCK);

  node->stmt.block.len = 0;
  node->stmt.block.scope = Iris_ParserCreateScope(parent);

  return node;
}

static Iris_AstNode* Iris_ParserCreateRetStmt(Iris_Parser* parser, Iris_AstNode* expr)
{
  Iris_AstNode* node = Iris_ParserCreateNode(parser, NODE_RET);

  node->stmt.ret.expr = expr;

  return node;
}

static Iris_AstNode* Iris_ParserCreateVarStmt(Iris_Parser* parser, Iris_Token* name, u64_t index, Iris_AstNode* val)
{
  Iris_AstNode* node = Iris_ParserCreateNode(parser, NODE_VAR);

  node->stmt.var.name = name;
  node->stmt.var.index = index;
  node->stmt.var.val = val;

  return node;
}

static void_t Iris_ParserAddFnDefToScope(Iris_Scope* scope, Iris_FnDefStmt* fn_def)
{
  if(scope->fns->len >= scope->fns->cap)
  {
    scope->fns = Iris_ArrBuffReAlloc(scope->fns, scope->fns->cap * 2);

    if(scope->fns == null)
    {
      Iris_ReportSimpleError("could not allocate more memory for function definitions!", null);
    }
  }

  Iris_ArrBuffPush(scope->fns, fn_def);
}

static Iris_FnDefStmt* Iris_ParserScopeGetFnDefStmt(const Iris_Scope* scope, Iris_Token* name)
{
  Iris_FnDefStmt* fn_def = null;
  
  for(i64_t i = 0; i < scope->fns->len; i++)
  {
    fn_def = (Iris_FnDefStmt*)Iris_ArrBuffAt(scope->fns, i);
    
    if(!strncmp(fn_def->name->start, name->start, name->len))
    {
      return fn_def;
    }
  }

  return fn_def;
}

static bool_t Iris_ParserScopeHasFnDef(Iris_Scope* scope, Iris_Token* name)
{
  Iris_FnDefStmt* fn_def = null;

  for(u64_t i = 0; i < scope->fns->len; i++)
  {
    fn_def = Iris_ArrBuffAt(scope->fns, i);
    if(!strncmp(fn_def->name->start, name->start, Iris_Max(fn_def->name->len, name->len)))
    {
      return true;
    }
  }

  return false;
}

static void_t Iris_ParserScopeAddVarStmt(Iris_Scope* scope, Iris_VarStmt* var)
{
  if(scope->vars->len >= scope->vars->cap)
  {
    scope->vars = Iris_ArrBuffReAlloc(scope->vars, scope->vars->cap * 2);

    if(scope->vars == null)
    {
      Iris_ReportSimpleError("could not allocate more memory for variable definitions!", null);
    }
  }

  Iris_ArrBuffPush(scope->vars, var);
}

static bool_t Iris_ParserScopeHasVarStmt(const Iris_Scope* scope, Iris_Token* name)
{
  Iris_VarStmt* var = null;
  
  for(i64_t i = 0; i < scope->vars->len; i++)
  {
    var = (Iris_VarStmt*)Iris_ArrBuffAt(scope->vars, i);
    
    if(!strncmp(var->name->start, name->start, Iris_Max(var->name->len, name->len)))
    {
      return true;
    }
  }

  return false;
}

static Iris_VarStmt* Iris_ParserScopeGetVarStmt(const Iris_Scope* scope, Iris_Token* name)
{
  Iris_VarStmt* var = null;
  
  for(i64_t i = 0; i < scope->vars->len; i++)
  {
    var = (Iris_VarStmt*)Iris_ArrBuffAt(scope->vars, i);
    
    if(!strncmp(var->name->start, name->start, name->len))
    {
      return var;
    }
  }

  return var;
}

static Iris_Token* Iris_ParserExpectAndConsume(Iris_Lexer* lex, Iris_TokenType type, const i8_t* msg)
{
  Iris_Token* token = Iris_LexerConsumeToken(lex);
  Iris_Token* prev = Iris_LexerPeekToken(lex, -2);

  if(token == null || token->type != type)
  {
    Iris_ReportSyntaxError(prev, token, msg);
  }

  return token;
}

static Iris_Token* Iris_ParserPeekAndExpectToken(Iris_Lexer* lex, i64_t offset, Iris_TokenType type, const i8_t* msg)
{
  Iris_Token* prev = Iris_LexerPeekToken(lex, offset - 1);
  Iris_Token* token = Iris_LexerPeekToken(lex, offset);

  if(token == null || token->type != type)
  {
    Iris_ReportSyntaxError(prev, token, msg);
  }

  return token;
}

static void_t Iris_ParserParseFnCallArgs(Iris_Parser* parser, Iris_Lexer* lex, Iris_Scope* scope, Iris_AstNode* expr)
{
  Iris_Token* name = Iris_LexerPeekToken(lex, -1);
  Iris_FnDefStmt* fn_def = Iris_ParserScopeGetFnDefStmt(scope->parent, name);
  Iris_AstNode* node = null;

  for(i64_t i = 0; i < fn_def->args_count; i++)
  {
    node = Iris_ParserParseWeakExpr(parser, lex, scope);
    expr->expr.call.nodes[expr->expr.call.len++] = node;
    node->parent = expr;
  }

  Iris_LexerConsumeToken(lex);
}

static Iris_AstNode* Iris_ParserParsePrimaryExpr(Iris_Parser* parser, Iris_Lexer* lex, Iris_Scope* scope)
{
  Iris_Token* token = Iris_LexerConsumeToken(lex);
  Iris_Token* tmp_token = null;
  Iris_AstNode* expr = null;
  Iris_VarStmt* var = null;
  i8_t* str_num = null;

  if(token->type == TK_IDENT)
  {
    tmp_token = token;
    token = Iris_LexerPeekToken(lex, 0);

    if(token && token->type == TK_BPAR_L)
    {
      expr = Iris_ParserCreateNode(parser, NODE_FN_CALL);
      expr->expr.call.name = tmp_token;
      Iris_ParserParseFnCallArgs(parser, lex, scope, expr);
    }
    else
    {
      if(!Iris_ParserScopeHasVarStmt(scope, tmp_token))
      {
        Iris_ReportNotFound(tmp_token);
      }

      var = Iris_ParserScopeGetVarStmt(scope, tmp_token);
      expr = Iris_ParserCreateNode(parser, NODE_IDENT);
      expr->expr.ident.index = var->index;
      expr->expr.ident.name = var->name;     
    }
  }
  else if(token->type == TK_NUMLIT)
  {
    expr = Iris_ParserCreateNode(parser, NODE_NUMLIT);
    str_num = strndup(token->start, token->len);
    expr->expr.num = strtoll(str_num, NULL, 10);
    free(str_num);
  }
  else if(token->type == TK_BPAR_L)
  {
    expr = Iris_ParserParseWeakExpr(parser, lex, scope);
  }

  return expr;
}

static Iris_AstNode* Iris_ParserParseStrongArithmExpr(Iris_Parser* parser, Iris_Lexer* lex, Iris_Scope* scope)
{
  Iris_AstNode* left = Iris_ParserParsePrimaryExpr(parser, lex, scope);
  Iris_AstNode* right = NULL;
  Iris_Token* token = NULL;
  Iris_OperType op = OPER_UNKNOWN;

  while((token = Iris_LexerPeekToken(lex, 0))) 
  {
    switch(token->type) 
    {
      case TK_MUL:
      case TK_DIV:
        op = Iris_ParserTokenTypeToOperType(token->type);
        Iris_LexerConsumeToken(lex);
        right = Iris_ParserParsePrimaryExpr(parser, lex, scope);
        left = Iris_ParserCreateBinExpr(parser, left, op, right);
        break;
      default:
        return left;
    }
  }

  return left;
}

static Iris_AstNode* Iris_ParserParseWeakArithmExpr(Iris_Parser* parser, Iris_Lexer* lex, Iris_Scope* scope)
{
  Iris_AstNode* left = Iris_ParserParseStrongArithmExpr(parser, lex, scope);
  Iris_AstNode* right = null;
  Iris_Token* token = null;
  Iris_OperType op = OPER_UNKNOWN;

  while((token = Iris_LexerPeekToken(lex, 0)) && token && (token->type == TK_PLUS || token->type == TK_MINUS)) 
  {
    if(token->type == TK_PLUS || token->type == TK_MINUS)
    {
      op = Iris_ParserTokenTypeToOperType(token->type);
      Iris_LexerConsumeToken(lex);
      right = Iris_ParserParseStrongArithmExpr(parser, lex, scope);
      left = Iris_ParserCreateBinExpr(parser, left, op, right);
    }
    else if(token->type == TK_SEMI)
    {
      break;
    }
  }

  return left;
}

Iris_AstNode* Iris_ParserParseWeakExpr(Iris_Parser* parser, Iris_Lexer* lex, Iris_Scope* scope)
{
  Iris_AstNode* left = Iris_ParserParseWeakArithmExpr(parser, lex, scope);
  Iris_AstNode* right = NULL;
  Iris_Token* token = NULL;
  Iris_OperType op = OPER_UNKNOWN;

  while((token = Iris_LexerPeekToken(lex, 0))) 
  {
    switch(token->type) 
    {
      case TK_EQ:
      case TK_EQ_GT:
      case TK_EQ_LT:
      case TK_GT:
      case TK_LT:
        op = Iris_ParserTokenTypeToOperType(token->type);
        Iris_LexerConsumeToken(lex);
        right = Iris_ParserParseWeakArithmExpr(parser, lex, scope);
        left = Iris_ParserCreateBinExpr(parser, left, op, right);
        break;
      default:
        return left;
    }
  }

  return left;
}

static void_t Iris_ParserParseFnArgs(Iris_Parser* parser, Iris_Lexer* lex, Iris_AstNode* fn)
{
  Iris_AstNode* var = null;
  Iris_Token* token = null;
  Iris_VarStmt* var_stmt = null;
  u64_t index = 0;

  Iris_ParserExpectAndConsume(lex, TK_BPAR_L, "(");

  while((token = Iris_LexerConsumeToken(lex)) && token && token->type == TK_IDENT)
  {
    if(Iris_ParserScopeHasVarStmt(&fn->stmt.fn.block.scope, token))
    {
      var_stmt = Iris_ParserScopeGetVarStmt(&fn->stmt.fn.block.scope, token);
      Iris_ReportRedefenitionEror(var_stmt->name);
    }

    index = Iris_ParserCalcVarStackIndex((&fn->stmt.fn));
    var = Iris_ParserCreateVarStmt(parser, token, index, null);
    Iris_ParserScopeAddVarStmt(&fn->stmt.fn.block.scope, &var->stmt.var);
    fn->stmt.fn.args_count++;
    var->parent = fn;
  }

  Iris_ParserPeekAndExpectToken(lex, -1, TK_BPAR_R, ")");
}

static void_t Iris_ParserParseFnBody(Iris_Parser* parser, Iris_Lexer* lex, Iris_AstNode* _fn)
{
  Iris_AstNode* node = null;
  Iris_Token* token = null;
  Iris_VarStmt* var_stmt = null;
  Iris_FnDefStmt* fn = &_fn->stmt.fn;
  u64_t index = 0;
    
  Iris_ParserExpectAndConsume(lex, TK_BBRACES_L, "{");
 
  while((token = Iris_LexerConsumeToken(lex)) && token && token->type != TK_BBRACES_R) 
  {
    if(token->type == TK_KEYWORD)
    {
      if(!strncmp(token->start, "var", 3))
      {
        index = Iris_ParserCalcVarStackIndex(fn);
        node = Iris_ParserCreateVarStmt(parser, null, index, null);
        node->stmt.var.name = Iris_ParserExpectAndConsume(lex, TK_IDENT, Iris_LexerTokenTypeToString(TK_IDENT));
        if(Iris_ParserScopeHasVarStmt(&fn->block.scope, node->stmt.var.name))
        {
          var_stmt = Iris_ParserScopeGetVarStmt(&fn->block.scope, node->stmt.var.name);
          Iris_ReportRedefenitionEror(var_stmt->name);
        }

        Iris_ParserExpectAndConsume(lex, TK_ASSIGN, "=");
        node->stmt.var.val = Iris_ParserParseWeakExpr(parser, lex, &fn->block.scope);

        Iris_ParserScopeAddVarStmt(&fn->block.scope, &node->stmt.var);
        fn->block.nodes[fn->block.len++] = node;
        node->stmt.var.val->parent = node;
        node->parent = _fn;
      }
      else if(!strncmp(token->start, "ret", 3))
      {
        node = Iris_ParserCreateRetStmt(parser, Iris_ParserParseWeakExpr(parser, lex, &fn->block.scope));
        fn->block.nodes[fn->block.len++] = node;
        node->parent = _fn;
        break;
      }   
    }
  }

  // Iris_ParserExpectAndConsume(lex, TK_BBRACES_R, "}");
}

static Iris_AstNode* Iris_ParserParseFnDefStmt(Iris_Parser* parser, Iris_Lexer* lex, Iris_Scope* scope)
{
  Iris_Token* name = Iris_ParserExpectAndConsume(lex, TK_IDENT, Iris_LexerTokenTypeToString(TK_IDENT));
  Iris_AstNode* node = Iris_ParserCreateFnDefStmt(parser, name, scope);
  Iris_FnDefStmt* fn_def = null;

  if(Iris_ParserScopeHasFnDef(scope, name))
  {
    fn_def = Iris_ParserScopeGetFnDefStmt(scope, name);
    Iris_ReportRedefenitionEror(fn_def->name);
  }

  Iris_ParserParseFnArgs(parser, lex, node);
  Iris_ParserParseFnBody(parser, lex, node);

  return node;
}

void_t Iris_ParserInit(Iris_Parser* parser)
{
  parser->nodes = Iris_ArrBuffAlloc(256, sizeof(Iris_AstNode));
  parser->root = null;
}

void_t Iris_ParserParse(Iris_Parser* parser, Iris_Lexer* lex)
{
  parser->root = Iris_ParserCreateBlockStmt(parser, null);
  
  Iris_AstNode* node = null;
  Iris_Token* token = null;

  while((token = Iris_LexerConsumeToken(lex)))
  {
    if(token->type == TK_KEYWORD)
    {
      if(!strncmp("fn", token->start, 2))
      {
        node = Iris_ParserParseFnDefStmt(parser, lex, &parser->root->stmt.block.scope);
        Iris_ParserAddFnDefToScope(&parser->root->stmt.block.scope, &node->stmt.fn);
      }
      
      parser->root->stmt.block.nodes[parser->root->stmt.block.len++] = node;
      node->parent = parser->root;
    }
    else if(token->type == TK_IDENT) 
    {
      node = Iris_ParserParseWeakExpr(parser, lex, &parser->root->stmt.block.scope);
      parser->root->stmt.block.nodes[parser->root->stmt.block.len++] = node;
      node->parent = parser->root;
    }
  }
}

void_t Iris_ParserDump(const Iris_AstNode* node)
{
  static i32_t depth = 0;

  switch(node->type) 
  {
    case NODE_IDENT:
      printf("\e[2m%*sIdent\e[0m[%zu]: %.*s\n", depth, "", node->expr.ident.index, node->expr.ident.name->len, node->expr.ident.name->start);
      break;
    case NODE_BLOCK:
      printf("\e[2mBlock\e[0m:%*s\n", depth, "");
      for(u64_t i = 0; i < node->stmt.block.len; i++)
      {
        depth += 3;
        Iris_ParserDump(node->stmt.block.nodes[i]);
        depth -= 3;
      }
      break;
    case NODE_FN_DEF:
      printf("\e[2m%*sFnDef\e[0m: %.*s(%i:%i)\n", depth, "", node->stmt.fn.name->len, node->stmt.fn.name->start, node->stmt.fn.args_count, node->stmt.fn.stack_size);
      depth += 3;
      for(i32_t i = 0; i < node->stmt.fn.block.len; i++)
      {
        Iris_ParserDump(node->stmt.fn.block.nodes[i]);
      }
      depth -= 3;
      break;
    case NODE_FN_CALL:
      printf("\e[2m%*s%s\e[0m: %.*s", depth, "", Iris_ParserAstNodeToString(node->type), node->expr.call.name->len, node->expr.call.name->start);
      printf("(");
      for(i64_t i = 0; i < node->expr.call.len; i++)
      {
        if(i + 1 < node->expr.call.len)
          printf("%lli ", node->expr.call.nodes[i]->expr.num);
        else
          printf("%lli", node->expr.call.nodes[i]->expr.num);
      }
      printf(")\n");
      break;
    case NODE_RET:
      printf("\e[2m%*s%s\e[0m:\n", depth, "", Iris_ParserAstNodeToString(node->type));
      depth += 3;
      Iris_ParserDump(node->stmt.ret.expr);
      depth -= 3;
      break;
    case NODE_BINARY:
      printf("\e[2m%*s%s\e[0m:\n", depth, "", Iris_ParserAstNodeToString(node->type));
      
      depth += 3;
      Iris_ParserDump(node->expr.bin.left);
      printf("%*s%s\n", depth, "", Iris_ParserOperTypeToString(node->expr.bin.op));
      Iris_ParserDump(node->expr.bin.right);
      depth -= 3;
      break;
    case NODE_NUMLIT:
      printf("%*s%lli\n", depth, "", node->expr.num);
      break;
    case NODE_VAR:
      printf("\e[2m%*sVar\e[0m[%i]:", depth, "", node->stmt.var.index);
      if(node->stmt.var.val)
      {
        printf("\n");
        depth += 3;
        Iris_ParserDump(node->stmt.var.val);
        depth -= 3;
      }
      else
      {
        printf(" %.*s\n", node->stmt.var.name->len, node->stmt.var.name->start);
      }
     break;
  }
}
