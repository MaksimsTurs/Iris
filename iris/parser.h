#ifndef H_IRIS_PARSER
#define H_IRIS_PARSER

#include "iris/builtin/types.h"
#include "iris/lexer.h"
#include "iris/utils.h"
#include "iris/mem.h"
#include "iris/ansicodes.h"
#include "iris/typedef.h"

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

#define Iris_ParserCalcStackOffset(fn) ((fn->stack_size / 8) - 1)

#define IRIS_MAX_FN_ARGS              8
#define IRIS_PARSER_DEF_NODE_ARR_CAP  8
#define IRIS_PARSER_DEF_ARENA_SIZE    128 * 128 * 1

// TODO: Maybe do not save pointer to token for functions, variables 
//       and struct names, instead create a hash from this name.

typedef enum 
{
  NODE_UNKNOWN = 0,
  NODE_BLCK,
  NODE_STRCT_DEF,
  NODE_FN_DEF,
  NODE_FN_CALL,
  NODE_VAR,
  NODE_RET,
  NODE_IF,
  NODE_ELSE,
  NODE_ELIF,
  NODE_FOR,
  NODE_BIN,
  NODE_IDENT,
  NODE_STRCT_PROP_ACCESS,
  NODE_STRCT_PROP,
  NODE_NULLLIT,
  NODE_NANLIT,
  NODE_INFINITYLIT,
  NODE_STRINGLIT,
  NODE_BOOLLIT,
  NODE_NUMLIT,
  NODE_STRCTLIT,
} Iris_NodeType;

typedef enum 
{
  OPR_UNKNOWN = 0,
  OPR_PLUS,
  OPR_MINUS,
  OPR_MUL,
  OPR_DIV,
  OPR_ASSIGN,
  OPR_EQ,
  OPR_NOT,
  OPR_NOT_EQ,
  OPR_EQ_GT,
  OPR_EQ_LT,
  OPR_GT,
  OPR_LT,
  OPR_AND,
  OPR_OR
} Iris_OprType;

typedef struct 
{
  Iris_NodeType type;
  ptr_t         as;
  ptr_t         parent;
  bool_t        is_visited : 1;
} Iris_Node;

typedef struct 
{
  Iris_Node** buff;
  i64_t       cap;
  i64_t       len;
} Iris_Nodes;

typedef struct Iris_Scope
{
  struct Iris_Scope* parent;
  Iris_Nodes         nodes;
} Iris_Scope;

typedef struct
{
  Iris_Token* key;
  Iris_Token* typename;
  Iris_Node*  val;
  i64_t       offset;
  i64_t       size;
} Iris_StrctProp;

// Expressions

typedef struct
{
  struct Iris_StrctDefStmt* def;
  Iris_StrctProp*           buff;
  i64_t                     len;
  i64_t                     cap;
} Iris_Strct;

typedef union
{
  Iris_Float64 f64;
  Iris_Int64   i64;
  Iris_Bool    bool;
  Iris_Strct   strct;
  Iris_Str     str;
} Iris_LitExpr;

typedef struct
{
  i64_t      stack_offset;
  i64_t      instrct_offset;
  Iris_Node* val;
} Iris_StrctPropAccessExpr;

typedef struct 
{
  Iris_Token* name;
  i64_t       stack_offset;
} Iris_IdentExpr;

typedef struct 
{
  Iris_Node*   ls;
  Iris_OprType op;
  Iris_Node*   rs;
} Iris_BinExpr;

typedef struct
{
  Iris_Node* buff[IRIS_MAX_FN_ARGS];
  i16_t      len;
} Iris_FnArgsList;

typedef struct 
{
  Iris_Token*     name;
  Iris_FnArgsList args; 
} Iris_FnCallExpr;

// Statements

typedef struct 
{
  Iris_Token* name;
  i64_t       offset;
  Iris_Node*  val;
} Iris_VarStmt;

typedef struct 
{
  Iris_Node* val;
} Iris_RetStmt;

typedef struct
{
  Iris_Scope scope;
  Iris_Nodes nodes;
} Iris_BlockStmt;

typedef struct Iris_IfStmt
{
  Iris_Node* expr;
  Iris_Node* block;
} Iris_IfStmt;

typedef struct
{
  Iris_Node* init;
  Iris_Node* expr;
  Iris_Node* inc;
  Iris_Node* block;
} Iris_ForStmt;

typedef struct 
{
  bool_t         has_ret;
  i64_t          args_count;
  i64_t          stack_size;
  Iris_Token*    name;
  Iris_Node*     block;
} Iris_FnDefStmt;

typedef struct
{
  Iris_Token*     name;
  i64_t           size;
  Iris_StrctProp* buff;
  i64_t           len;
  i64_t           cap;
} Iris_StrctDefStmt;

typedef struct 
{
  Iris_Node*  root;
  Iris_Lexer* lex;
  Iris_Arena* arena;
} Iris_Parser;

void_t Iris_ParserNew(Iris_Parser* parser, Iris_Lexer* lex);
void_t Iris_ParserDestroy(Iris_Parser* parser);
void_t Iris_ParserParse(Iris_Parser* parser);
void_t Iris_ParserDump(const Iris_Node* node, i64_t max_depth);
const i8_t* Iris_ParserNodeTypeToString(Iris_NodeType type);
const i8_t* Iris_ParserOprtTypeToString(Iris_OprType type);

#endif
