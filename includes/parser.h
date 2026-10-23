#ifndef H_IRIS_PARSER
#define H_IRIS_PARSER

#include <stdio.h>
#include <string.h>

#include "lexer.h"
#include "mem.h"
#include "typedef.h"

#define IRIS_PARSER_NODES_BUFF_SIZE 34

// WeakExpr
// | < | > | == | != | <= | => |
// WeakArithmExp
// | + | - |
// StrongArithmExpr
// | * | / |
// PrimaryExpr
// | 0..9 | ident |

typedef enum {
  NODE_UNKNOWN = 0,
  NODE_PROG,
  NODE_BINARY,
  NODE_NUMLIT,
  NODE_IDENT,
  NODE_VAR,
  NODE_FN_DEF,
  NODE_FN_CALL,
  NODE_RET,
} Iris_AstNodeType;

typedef enum {
  OPER_UNKNOWN = 0,
  OPER_PLUS,
  OPER_MINUS,
  OPER_MUL,
  OPER_DIV,
  OPER_EQ,
  OPER_NOT_EQ,
  OPER_EQ_GT,
  OPER_EQ_LT,
  OPER_GT,
  OPER_LT
} Iris_OperType;

typedef struct {
  struct Iris_AstNode* left;
  Iris_OperType        op;
  struct Iris_AstNode* right;
} Iris_BinExpr;

typedef struct {
  Iris_Token* name;
} Iris_FnCallExp;

typedef union {
  i64_t           num;
  Iris_BinExpr    bin;
  Iris_Token*     ident;
  Iris_FnCallExp  call;
} Iris_Expr;

typedef struct {
  struct Iris_AstNode* expr;
} Iris_RetStmt;

typedef struct {
  Iris_Token*           name;
  i32_t                 index;
  struct Iris_AstNode*  val;
} Iris_VarStmt;

typedef struct {
  struct Iris_AstNode*  nodes[IRIS_PARSER_NODES_BUFF_SIZE];
  i32_t                 len;
  Iris_Token*           name;
  i32_t                 stack_size;
  i32_t                 args_len;
} Iris_FnDefStmt;

typedef struct {
  struct Iris_AstNode*  nodes[IRIS_PARSER_NODES_BUFF_SIZE];
  i32_t                 length;
} Iris_ProgStmt;

typedef union {
  Iris_ProgStmt   prog;
  Iris_VarStmt    var;
  Iris_FnDefStmt  fn;
  Iris_RetStmt    ret;
} Iris_Stmt;

typedef struct Iris_AstNode {
  Iris_AstNodeType     type;
  Iris_Expr            expr;
  Iris_Stmt            stmt;

  bool_t               is_visited;
  i32_t                depth;
  struct Iris_AstNode* parent;
} Iris_AstNode;

typedef struct {
  struct Iris_AstNode*  root;
  Iris_MemBuffer*       nodes;
} Iris_Parser;

void_t Iris_ParserParse(Iris_Parser* parser, Iris_Lexer* lex);
void_t Iris_ParserDump(const Iris_AstNode* node);

#endif
