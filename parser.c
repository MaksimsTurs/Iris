#include "parser.h"
#include "lexer.h"
#include "mem.h"
#include "typedef.h"
#include "utils.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Iris_AstNode* Iris_ParserParseWeakExpr(Iris_Parser* parser, Iris_Lexer* lex);

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

static void_t Iris_ParserInit(Iris_Parser* parser)
{
  parser->nodes = Iris_MemBufferAlloc(256, sizeof(Iris_AstNode));
  parser->root = NULL;
}

static Iris_AstNode* Iris_ParserCreateNode(Iris_Parser* parser, Iris_AstNodeType type)
{
  Iris_AstNode* node = (Iris_AstNode*)(parser->nodes->buff + (parser->nodes->len * sizeof(Iris_AstNode)));
  
  node->depth = -1;
  node->parent = NULL;
  node->type = type;
  node->is_visited = false;

  parser->nodes->len++;

  return node;
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

static Iris_AstNode* Iris_ParserParsePrimaryExpr(Iris_Parser* parser, Iris_Lexer* lex)
{
  Iris_Token* token = Iris_LexerConsumeToken(lex);
  Iris_Token* tmp_token = NULL;
  Iris_AstNode* expr = NULL; 
  i8_t* str_num = NULL;

  switch(token->type) 
  {
    case TK_NUMLIT:
      expr = Iris_ParserCreateNode(parser, NODE_NUMLIT);
      str_num = strndup(token->start, token->len);
      expr->expr.num = strtoll(str_num, NULL, 10);
      free(str_num);
      break;
    case TK_IDENT:
      expr = Iris_ParserCreateNode(parser, NODE_IDENT);
      tmp_token = token;
      token = Iris_LexerPeekToken(lex, 0);

      if(token && token->type == TK_BPAR_L)
      {
        expr->type = NODE_FN_CALL; 
        expr->expr.call.name = tmp_token;
      }
      else
      {
        expr->expr.ident = tmp_token;
      }

      Iris_LexerConsumeToken(lex);
      break;
    case TK_BBRACES_L:
      expr = Iris_ParserParseWeakExpr(parser, lex);
      Iris_LexerConsumeToken(lex);
      break;
  }

  return expr;
}

static Iris_AstNode* Iris_ParserParseStrongArithmExpr(Iris_Parser* parser, Iris_Lexer* lex)
{
  Iris_AstNode* left = Iris_ParserParsePrimaryExpr(parser, lex);
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
        right = Iris_ParserParsePrimaryExpr(parser, lex);
        left = Iris_ParserCreateBinExpr(parser, left, op, right);
        break;
      default:
        return left;
    }
  }

  return left;
}

static Iris_AstNode* Iris_ParserParseWeakArithmExpr(Iris_Parser* parser, Iris_Lexer* lex)
{
  Iris_AstNode* left = Iris_ParserParseStrongArithmExpr(parser, lex);
  Iris_AstNode* right = NULL;
  Iris_Token* token = NULL;
  Iris_OperType op = OPER_UNKNOWN;

  while((token = Iris_LexerPeekToken(lex, 0))) 
  {
    switch(token->type) 
    {
      case TK_PLUS:
      case TK_MINUS:
        op = Iris_ParserTokenTypeToOperType(token->type);
        Iris_LexerConsumeToken(lex);
        right = Iris_ParserParseStrongArithmExpr(parser, lex);
        left = Iris_ParserCreateBinExpr(parser, left, op, right);
        break;
      default:
        return left;
    }
  }

  return left;
}

Iris_AstNode* Iris_ParserParseWeakExpr(Iris_Parser* parser, Iris_Lexer* lex)
{
  Iris_AstNode* left = Iris_ParserParseWeakArithmExpr(parser, lex);
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
        right = Iris_ParserParseWeakArithmExpr(parser, lex);
        left = Iris_ParserCreateBinExpr(parser, left, op, right);
        break;
      default:
        return left;
    }
  }

  return left;
}

static void_t Iris_ParserParseFnArgs(Iris_FnDefStmt* fn, Iris_Lexer* lex)
{
  Iris_Token* token = NULL;

  while((token = Iris_LexerConsumeToken(lex)))
  {
    switch(token->type)
    {
      case TK_IDENT:
        fn->args_len++;
        fn->stack_size += 8;
        break;
      case TK_COMMA:
        break;
      default:
        return;
    }
  }
}

static void_t Iris_ParserParseBody(Iris_AstNode* parent, Iris_Parser* parser, Iris_Lexer* lex)
{
  Iris_AstNode* node = NULL;
  Iris_Token* token = NULL;

  while((token = Iris_LexerConsumeToken(lex))) 
  {
    switch(token->type)
    {
      case TK_KEYWORD:
        if(Iris_StrCmprN(token->start, "var", 3))
        {
          node = Iris_ParserCreateNode(parser, NODE_VAR);
          parent->stmt.fn.stack_size += 8;
          node->stmt.var.index = (parent->stmt.fn.stack_size / 8) - 1;
          node->stmt.var.name = Iris_LexerConsumeToken(lex);
          Iris_LexerConsumeToken(lex);
          node->stmt.var.val = Iris_ParserParseWeakExpr(parser, lex);
          node->stmt.var.val->parent = node;
        }
        else if(Iris_StrCmprN(token->start, "ret", 3))
        {
          node = Iris_ParserCreateNode(parser, NODE_RET);
          node->stmt.ret.expr = Iris_ParserParseWeakExpr(parser, lex);
        }
          
        node->parent = parent;
        parent->stmt.fn.nodes[parent->stmt.fn.len++] = node;
        break;
      case TK_BBRACES_R:
        return;
    }  
 }
}

static Iris_AstNode* Iris_ParserParseFnDef(Iris_Parser* parser, Iris_Lexer* lex)
{
  Iris_AstNode* node = Iris_ParserCreateNode(parser, NODE_FN_DEF);

  node->stmt.fn.name = Iris_LexerConsumeToken(lex);

  Iris_LexerConsumeToken(lex);
  Iris_ParserParseFnArgs(&node->stmt.fn, lex);
  Iris_LexerConsumeToken(lex);
  Iris_ParserParseBody(node, parser, lex);

  return node;
}

static Iris_AstNode* Iris_ParserParseProg(Iris_Parser* parser, Iris_Lexer* lex)
{
  Iris_AstNode* prog = Iris_ParserCreateNode(parser, NODE_PROG);
  Iris_AstNode* node = NULL;
  Iris_Token* token = NULL;

  while((token = Iris_LexerPeekToken(lex, 0)))
  {
    switch(token->type) 
    {
      case TK_KEYWORD:
        Iris_LexerConsumeToken(lex);
        if(Iris_StrCmprN("fn", token->start, 2))
        {
          node = Iris_ParserParseFnDef(parser, lex);
        }
        break;
      case TK_IDENT:
        node = Iris_ParserParseWeakExpr(parser, lex);
        Iris_LexerConsumeToken(lex);
        Iris_LexerConsumeToken(lex);
        break;
      default:
        node = Iris_ParserParseWeakExpr(parser, lex);
   }
  
    node->parent = prog;
    prog->stmt.prog.nodes[prog->stmt.prog.length++] = node;
 }

  return prog;
}

void_t Iris_ParserParse(Iris_Parser* parser, Iris_Lexer* lex)
{
  Iris_ParserInit(parser);
  parser->root = Iris_ParserParseProg(parser, lex);
}

void_t Iris_ParserDump(const Iris_AstNode* node)
{
  static i32_t depth = 0;

  switch(node->type) 
  {
    case NODE_IDENT:
      printf("\e[2m%*s%s\e[0m: %.*s\n", depth, "", Iris_ParserAstNodeToString(node->type), node->expr.ident->len, node->expr.ident->start);
      break;
    case NODE_PROG:
      printf("\e[2m%s\e[0m:%*s\n", Iris_ParserAstNodeToString(node->type), depth, "");
      for(u32_t i = 0; i < node->stmt.prog.length; i++)
      {
        depth += 3;
        Iris_ParserDump(node->stmt.prog.nodes[i]);
        depth -= 3;
      }
      break;
    case NODE_FN_DEF:
      printf("\e[2m%*s%s\e[0m: %.*s(%i:%i)\n", depth, "", Iris_ParserAstNodeToString(node->type), node->stmt.fn.name->len, node->stmt.fn.name->start, node->stmt.fn.args_len, node->stmt.fn.stack_size);
      depth += 3;
      for(i32_t i = 0; i < node->stmt.fn.len; i++)
      {
        Iris_ParserDump(node->stmt.fn.nodes[i]);
      }
      depth -= 3;
      break;
    case NODE_FN_CALL:
      printf("\e[2m%*s%s\e[0m: %.*s\n", depth, "", Iris_ParserAstNodeToString(node->type), node->expr.call.name->len, node->expr.call.name->start);
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
      printf("\e[2m%*s%s\e[0m[%i]:\n", depth, "", Iris_ParserAstNodeToString(node->type), node->stmt.var.index);
      depth += 3;
      Iris_ParserDump(node->stmt.var.val);
      depth -= 3;
      break;
  }
}
