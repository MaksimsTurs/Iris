#ifndef H_IRIS_PARSER
#define H_IRIS_PARSER

#include "iris/lexer.h"
#include "iris/utils.h"
#include "iris/mem.h"
#include "iris/typedef.h"

#include <assert.h>
#include <float.h>

#define IRIS_PARSER_DEF_NODE_ARR_CAP  16
#define IRIS_PARSER_DEF_ARENA_SIZE    128 * 128 * 1

#define Iris_Nan      NAN
#define Iris_Infinity INFINITY
#define Iris_Nil      null

typedef enum 
{
  NODE_BODY,
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
  NODE_UNARY,
  NODE_IDENT,
  NODE_STRCT_FN_ACCESS,
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
  OPR_PLUS,
  OPR_MINUS,
  OPR_MUL,
  OPR_DIV,
  OPR_MOD,
  OPR_ASSIGN,
  OPR_EQ,
  OPR_NOT,
  OPR_NOT_EQ,
  OPR_EQ_GT,
  OPR_EQ_LT,
  OPR_GT,
  OPR_LT,
  OPR_AND,
  OPR_OR,
  OPR_BIT_OR,
  OPR_BIT_AND,
  OPR_BIT_NOT,
  OPR_BIT_XOR,
  OPR_BIT_LSHIFT,
  OPR_BIT_RSHIFT
} Iris_OprKind;

typedef enum
{
  TP_VOID,
  TP_CHAR,
  TP_BOOL,
  TP_NIL,
  TP_NAN,
  TP_INF,
  TP_I8,
  TP_U8,
  TP_I16,
  TP_U16,
  TP_I32,
  TP_U32,
  TP_I64,
  TP_U64,
  TP_F32,
  TP_F64,
  TP_STRCT,
  TP_PTR,
} Iris_TypeKind;

typedef struct
{
  char_t* buff;
  i64_t   len;
} Iris_Atom;

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

typedef struct
{
  Iris_Atom     atom;
  Iris_TypeKind type;
  Iris_Token*   typename;
  Iris_Node*    val;
  i64_t         offset;
} Iris_StrctPropStmt;

typedef struct
{
  Iris_StrctPropStmt* buff;
  i64_t               cap;
  i64_t               len;
} Iris_StrctProps;

typedef struct Iris_Scope
{
  struct Iris_Scope* parent;
  Iris_Nodes         nodes;
} Iris_Scope;

// Expressions

typedef struct
{
  struct Iris_StrctDefStmt* def;
  Iris_StrctProps           props;
} Iris_StrctLitExpr;

typedef union
{
  f64_t             f64;
  i64_t             i64;
  Iris_StrctLitExpr strct;
} Iris_LitExpr;

typedef struct
{
  Iris_Node* prop;
  i64_t      offset;
} Iris_StrctPropAccessExpr;

typedef struct 
{
  struct Iris_VarStmt* def;
} Iris_IdentExpr;

typedef struct 
{
  Iris_Node*   ls;
  Iris_OprKind op;
  Iris_Node*   rs;
} Iris_BinExpr;

typedef struct
{
  Iris_Node*   operand;
  Iris_OprKind op;
} Iris_UnaryExpr;

typedef struct 
{
  Iris_Atom   atom;
  Iris_Nodes  nodes;
} Iris_FnCallExpr;

// Statements

typedef struct
{
  Iris_Atom       atom;
  i64_t           size;
  Iris_StrctProps props;
} Iris_StrctDefStmt;

typedef struct 
{
  Iris_Atom     atom; 
  // Used if type is TP_STRCT.
  Iris_Token*   typename;
  Iris_Node*    val;
  i64_t         offset;
  Iris_TypeKind type;
  bool_t        is_global : 1;
} Iris_VarStmt;

typedef struct 
{
  Iris_Node* val;
} Iris_RetStmt;

typedef struct
{
  Iris_Scope scope;
  Iris_Nodes nodes;
  i64_t      memsize;
} Iris_BodyStmt;

typedef struct
{
  Iris_Node*  cond;
  Iris_Node*  body;
  Iris_Nodes  nextconds;
} Iris_IfStmt;

typedef struct
{
  Iris_Node* init;
  Iris_Node* cond;
  Iris_Node* inc;
  Iris_Node* body;
} Iris_ForStmt;

typedef struct 
{
  Iris_Atom   atom;
  Iris_Node*  body;
  i64_t       args_count;
} Iris_FnDefStmt;

typedef struct 
{
  Iris_Node*  root;
  Iris_Lexer* lex;
  Iris_Arena* arena;
} Iris_Parser;

void_t Iris_ParserNew(Iris_Parser* parser, Iris_Lexer* lex);
void_t Iris_ParserDestroy(Iris_Parser* parser);
void_t Iris_ParserParse(Iris_Parser* parser);
void_t Iris_ParserDump(const Iris_Node* node);
const char_t* Iris_ParserNodeTypeToString(Iris_NodeType type);
const char_t* Iris_ParserOprtTypeToString(Iris_OprKind type);

#endif
