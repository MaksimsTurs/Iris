#include "iris/parser.h"
#include "iris/lexer.h"
#include "iris/mem.h"
#include "iris/typedef.h"
#include "iris/utils.h"
#include <string.h>

Iris_Node* Iris_ParserParseCondExpr(Iris_Parser* parser, Iris_Node* parent, Iris_Scope* scope);
Iris_Node* Iris_ParserParseStmtOrExpr(Iris_Parser* parser, Iris_Node* fn, Iris_Node* block);

#define Iris_ParserPrint(depth, type, msg, ...) printf(IRIS_ANSI_DIM "%*s%s" IRIS_ANSI_RES ": " msg, depth, "", Iris_ParserNodeTypeToString(type), __VA_ARGS__)
#define Iris_ParserIsEof($token)  ((($token) == null) || (($token)->type == TK_EOF))

static inline bool_t Iris_ParserIsStrctPropCall(Iris_Lexer* lex, const Iris_Token* maybe_strct_ident)
{
  Iris_Token* maybe_fn_ident = Iris_LexerPickToken(lex, 2);
  Iris_Token* maybe_lparen = Iris_LexerPickToken(lex, 3);
  
  return(
    maybe_strct_ident->type == TK_IDENT && 
    !Iris_ParserIsEof(maybe_lparen) && maybe_lparen->type == TK_BPAR_L &&
    !Iris_ParserIsEof(maybe_fn_ident) && maybe_fn_ident->type == TK_IDENT
  );
}

static inline bool_t Iris_ParserIsStrctPropAccess(Iris_Lexer* lex, const Iris_Token* maybe_ident)
{
  Iris_Token* maybe_colon = Iris_LexerPickToken(lex, 1);
  return(maybe_ident->type == TK_IDENT && !Iris_ParserIsEof(maybe_colon) && maybe_colon->type == TK_COLON);
}

static inline bool_t Iris_ParserIsFnDef(Iris_Lexer* lex, const Iris_Token* maybe_ident)
{
  Iris_Token* maybe_lparen = Iris_LexerPickToken(lex, 2);
  return maybe_ident->type == TK_IDENT && !Iris_ParserIsEof(maybe_lparen) && maybe_lparen->type == TK_BPAR_L;
}

static inline bool_t Iris_ParserIsFnCallExpr(Iris_Lexer* lex, const Iris_Token* maybe_ident)
{
  Iris_Token* maybe_lparent = Iris_LexerPickToken(lex, 1);
  return maybe_ident->type == TK_IDENT && !Iris_ParserIsEof(maybe_lparent) && maybe_lparent->type == TK_BPAR_L;
}

static Iris_OprType Iris_ParserTokenTypeToOprType(Iris_TokenKind type)
{
  if(type == TK_PLUS)   return OPR_PLUS;
  if(type == TK_MINUS)  return OPR_MINUS;
  if(type == TK_MUL)    return OPR_MUL;
  if(type == TK_DIV)    return OPR_DIV;
  if(type == TK_ASSIGN) return OPR_ASSIGN;
  if(type == TK_EQ)     return OPR_EQ;
  if(type == TK_NOT_EQ) return OPR_NOT_EQ;
  if(type == TK_NOT)    return OPR_NOT;
  if(type == TK_GT)     return OPR_GT;
  if(type == TK_LT)     return OPR_LT;
  if(type == TK_EQ_GT)  return OPR_EQ_GT;
  if(type == TK_EQ_LT)  return OPR_EQ_LT;
  if(type == TK_AND)    return OPR_AND;
  if(type == TK_OR)     return OPR_OR;
  return OPR_UNKNOWN;
}

static Iris_Node* Iris_ParserCreateNode(Iris_Arena* mem, i64_t soc, Iris_Node* parent, Iris_NodeType type)
{
  Iris_Node* node = null;
  
  Iris_ArenaAlloc(mem, node, (i64_t)sizeof(Iris_Node) + soc);
  node->type = type;
  node->parent = parent;
  node->as = node + 1;

  return node;
}

static Iris_Node* Iris_ParserCreateBlockStmt(Iris_Arena* arena, Iris_Node* parent, Iris_Scope* scope_parent)
{
  Iris_Node* node = Iris_ParserCreateNode(arena, sizeof(Iris_BlockStmt), parent, NODE_BLCK);
  Iris_BlockStmt* stmt = node->as;
  
  Iris_ArrayNew(stmt->nodes,       IRIS_PARSER_DEF_NODE_ARR_CAP);
  Iris_ArrayNew(stmt->scope.nodes, IRIS_PARSER_DEF_NODE_ARR_CAP);

  stmt->scope.parent = scope_parent;

  return node;
}

static Iris_Node* Iris_ParserCreateFnDefStmt(Iris_Arena* mem, Iris_Node* parent, Iris_Token* name, Iris_Scope* scope_parent)
{
  Iris_Node* node = Iris_ParserCreateNode(mem, sizeof(Iris_FnDefStmt), parent, NODE_FN_DEF);
  Iris_Node* block = Iris_ParserCreateNode(mem, sizeof(Iris_BlockStmt), node, NODE_BLCK);
  Iris_FnDefStmt* stmt = node->as;
  Iris_BlockStmt* block_stmt = block->as;

  Iris_ArrayNew(block_stmt->nodes,       IRIS_PARSER_DEF_NODE_ARR_CAP);
  Iris_ArrayNew(block_stmt->scope.nodes, IRIS_PARSER_DEF_NODE_ARR_CAP);

  block_stmt->scope.parent = scope_parent;
  stmt->block = block;
  stmt->name = name;
  stmt->args_count = 0;
  stmt->stack_size = 0;
  stmt->has_ret = false;

  return node;
}

static Iris_Node* Iris_ParserCreateIfStmt(Iris_Arena* mem, Iris_Node* parent, Iris_Node* expr, Iris_Scope* scope_parent, Iris_NodeType type)
{
  Iris_Node* node = Iris_ParserCreateNode(mem, sizeof(Iris_IfStmt), parent, type);
  Iris_IfStmt* stmt = node->as;

  stmt->expr = expr;
  stmt->block = Iris_ParserCreateBlockStmt(mem, node, scope_parent);

  return node;
}

static Iris_Node* Iris_ParserCreateForStmt(Iris_Arena* mem, Iris_Node* parent, Iris_Scope* scope_parent)
{
  Iris_Node* node = Iris_ParserCreateNode(mem, sizeof(Iris_ForStmt), parent, NODE_FOR);
  Iris_ForStmt* stmt = node->as;

  stmt->expr = null;
  stmt->inc = null;
  stmt->init = null;
  stmt->block = Iris_ParserCreateBlockStmt(mem, node, scope_parent);

  return node;
}

static Iris_Node* Iris_ParserCreateStrctDefStmt(Iris_Arena* mem, Iris_Node* parent, Iris_Token* name)
{
  Iris_Node* node = Iris_ParserCreateNode(mem, sizeof(Iris_StrctDefStmt), parent, NODE_STRCT_DEF);
  Iris_StrctDefStmt* stmt = node->as;

  Iris_ArrayNew(*stmt, IRIS_PARSER_DEF_NODE_ARR_CAP);
  stmt->name = name;
  stmt->size = 0;

  return node;
}

static Iris_Node* Iris_ParserCreateRetStmt(Iris_Arena* mem, Iris_Node* parent)
{
  Iris_Node* node = Iris_ParserCreateNode(mem, sizeof(Iris_RetStmt), parent, NODE_RET);
  Iris_RetStmt* stmt = (Iris_RetStmt*)node->as;

  stmt->val = null;

  return node;
}

static Iris_Node* Iris_ParserCreateVarStmt(Iris_Arena* mem, Iris_Node* parent, Iris_Token* name, i64_t offset)
{
  Iris_Node* node = Iris_ParserCreateNode(mem, sizeof(Iris_VarStmt), parent, NODE_VAR);
  Iris_VarStmt* stmt = (Iris_VarStmt*)node->as;

  stmt->name = name;
  stmt->offset = offset;
  stmt->val = null;

  return node;
}

static Iris_Node* Iris_ParserCreateFnCallExpr(Iris_Arena* mem, Iris_Node* parent, Iris_Token* name)
{
  Iris_Node* node = Iris_ParserCreateNode(mem, sizeof(Iris_FnCallExpr), parent, NODE_FN_CALL);
  Iris_FnCallExpr* expr = (Iris_FnCallExpr*)node->as;

  expr->name = name;

  return node;
}

static Iris_Node* Iris_ParserCreateBinExpr(Iris_Arena* mem, Iris_Node* parent, Iris_Node* ls, Iris_OprType op, Iris_Node* rs)
{
  Iris_Node* node = Iris_ParserCreateNode(mem, sizeof(Iris_BinExpr), parent, NODE_BIN);
  Iris_BinExpr* expr = (Iris_BinExpr*)node->as;

  ls->parent = node;
  rs->parent = node;
  expr->ls = ls;
  expr->rs = rs;
  expr->op = op;

  return node;
}

static Iris_Node* Iris_ParserCreateIdentExpr(Iris_Arena* mem, Iris_Node* parent, Iris_Token* name, i64_t stack_offset)
{
  Iris_Node* node = Iris_ParserCreateNode(mem, sizeof(Iris_IdentExpr), parent, NODE_IDENT);
  Iris_IdentExpr* expr = (Iris_IdentExpr*)node->as;

  expr->name = name;
  expr->stack_offset = stack_offset;

  return node;
}

static Iris_Node* Iris_ParserCreateLitExpr(Iris_Arena* mem, Iris_Node* parent, Iris_NodeType type)
{
  Iris_Node* node = Iris_ParserCreateNode(mem, sizeof(Iris_LitExpr), parent, type);
  return node;
}

static Iris_Node* Iris_ParserCreateStrctPropAccessExpr(Iris_Arena* mem, Iris_Node* parent, Iris_Node* val, i64_t instrct_offset, i64_t stack_offset)
{
  Iris_Node* node = Iris_ParserCreateNode(mem, sizeof(Iris_StrctPropAccessExpr), parent, NODE_STRCT_PROP_ACCESS);
  Iris_StrctPropAccessExpr* expr = node->as;

  expr->instrct_offset = instrct_offset;
  expr->stack_offset = stack_offset;
  expr->val = val;

  return node;
}

static Iris_FnDefStmt* Iris_ParserScopeGetFnDef(const Iris_Scope* scope, const Iris_Token* name)
{
  Iris_FnDefStmt* fn_def_stmt = null;

  for(i64_t i = 0; i < scope->nodes.len; i++)
  {
    if(scope->nodes.buff[i]->type == NODE_FN_DEF)
    {
      fn_def_stmt = (Iris_FnDefStmt*)scope->nodes.buff[i]->as;
      
      if(Iris_StrnCmp(fn_def_stmt->name->start, name->start, Iris_Max(fn_def_stmt->name->len, name->len)))
      {
        return fn_def_stmt;
      }
    }
  }

  if(scope->parent)
  {
    return Iris_ParserScopeGetFnDef(scope->parent, name);
  }

  return null;
}

static Iris_VarStmt* Iris_ParserScopeGetVar(Iris_Scope* scope, Iris_Token* name)
{
  Iris_VarStmt* var_stmt = null;

  for(i64_t i = 0; i < scope->nodes.len; i++)
  {
    if(scope->nodes.buff[i]->type == NODE_VAR)
    {
      var_stmt = (Iris_VarStmt*)scope->nodes.buff[i]->as;
      
      if(Iris_StrnCmp(var_stmt->name->start, name->start, Iris_Max(var_stmt->name->len, name->len)))
      {
        return var_stmt;
      }
    }
  }

  if(scope->parent)
  {
    return Iris_ParserScopeGetVar(scope->parent, name);
  }

  return null;
}

static Iris_StrctDefStmt* Iris_ParserScopeGetStrctDef(Iris_Scope* scope, Iris_Token* name)
{
  Iris_StrctDefStmt* strct_stmt = null;

  for(i64_t i = 0; i < scope->nodes.len; i++)
  {
    if(scope->nodes.buff[i]->type == NODE_STRCT_DEF)
    {
      strct_stmt = scope->nodes.buff[i]->as;
      
      if(Iris_StrnCmp(strct_stmt->name->start, name->start, Iris_Max(strct_stmt->name->len, name->len)))
      {
        return strct_stmt;
      }
    }
  }

  if(scope->parent)
  {
    return Iris_ParserScopeGetStrctDef(scope->parent, name);
  }

  return null;
}

static Iris_StrctProp* Iris_ParserGetStrctProp(Iris_StrctProp* buff, i64_t len, Iris_Token* key)
{
  Iris_StrctProp* prop = null;

  for(i64_t i = 0; i < len; i++)
  {
    prop = &buff[i];
    
    if(Iris_StrnCmp(prop->key->start, key->start, Iris_Max(prop->key->len, key->len)))
    {
      return prop;
    }
  }

  return null;
}

static i64_t Iris_ParserGetSizeFromTypename(Iris_Scope* scope, const Iris_Token* typename)
{
  Iris_StrctDefStmt* strct_def = null;

  if(typename->type == TK_IDENT)
  {
    strct_def = Iris_ParserScopeGetStrctDef(scope, typename);
    return strct_def->size;
  }
  else
  {
    if(Iris_StrnCmp(typename->start, "i8",  2)) return 1;
    if(Iris_StrnCmp(typename->start, "u8",  2)) return 1;
    if(Iris_StrnCmp(typename->start, "i16", 3)) return 2;
    if(Iris_StrnCmp(typename->start, "u16", 3)) return 2;
    if(Iris_StrnCmp(typename->start, "i32", 3)) return 4;
    if(Iris_StrnCmp(typename->start, "u32", 3)) return 4;
    if(Iris_StrnCmp(typename->start, "i64", 3)) return 8;
    if(Iris_StrnCmp(typename->start, "u64", 3)) return 8;
    if(Iris_StrnCmp(typename->start, "f32", 3)) return 4;
    if(Iris_StrnCmp(typename->start, "f64", 3)) return 8;
  }

  return 0;
}

static void_t Iris_ParserParseFnCallArgList(Iris_Parser* parser, Iris_Scope* scope, Iris_Node* call_node)
{
  Iris_FnCallExpr* call_expr = (Iris_FnCallExpr*)call_node->as;
  Iris_FnDefStmt* fn_def_stmt = Iris_ParserScopeGetFnDef(scope, call_expr->name);
  Iris_Node* node = null;

  Iris_LexerConsumeToken(parser->lex); // lparent
  
  for(i16_t i = 0; i < fn_def_stmt->args_count; i++)
  {
    node = Iris_ParserParseCondExpr(parser, call_node, scope);
    Iris_ArrayPush(call_expr->args, node);
  }

  Iris_LexerConsumeToken(parser->lex); // rparent
}

static Iris_Node* Iris_ParserParseFnCallExpr(Iris_Parser* parser, Iris_Node* parent, Iris_Scope* scope)
{
  Iris_Token* fn_ident = Iris_LexerPickToken(parser->lex, 0);
  Iris_Node* node = Iris_ParserCreateFnCallExpr(parser->arena, parent, fn_ident);
  
  Iris_LexerConsumeToken(parser->lex); // ident
  Iris_ParserParseFnCallArgList(parser, scope, node);
  
  return node;
}

static Iris_Node* Iris_ParserParseLit(Iris_Parser* parser, Iris_Node* parent, Iris_Scope* scope, Iris_NodeType type)
{
  Iris_Node* node = null;
  Iris_Token* token = null;
  Iris_LitExpr* lit_expr = null;
  char_t* str_num = null;
  Iris_StrctDefStmt* strct_def = null;
  Iris_StrctProp prop_desc = {0};
  Iris_StrctProp prop = {0};

  node = Iris_ParserCreateLitExpr(parser->arena, parent, type);
  lit_expr = node->as;
 
  if(type == NODE_STRCTLIT)
  {
    token = Iris_LexerPickToken(parser->lex, -2);
    Iris_ArrayNew(lit_expr->strct, IRIS_LEXER_DEF_TOKENS_CAP);
    strct_def = Iris_ParserScopeGetStrctDef(scope, token);
    lit_expr->strct.def = (ptr_t)strct_def;
    Iris_LexerConsumeToken(parser->lex);
   
    while((token = Iris_LexerPickToken(parser->lex, 0)) && token->type != TK_BBRACES_R)
    {
      prop_desc = *Iris_ParserGetStrctProp(strct_def->buff, strct_def->len, token);
      prop.key = prop_desc.key;
      prop.offset = prop_desc.offset;
      prop.size = prop_desc.size;
      prop.typename = prop_desc.typename;
      Iris_LexerConsumeToken(parser->lex);
      Iris_LexerConsumeToken(parser->lex);
      Iris_LexerPrint(Iris_LexerPickToken(parser->lex, 0));
      prop.val = Iris_ParserParseCondExpr(parser, parent, scope);
      Iris_ArrayPush(lit_expr->strct, prop);
    }

    for(i64_t i = 0; i < strct_def->len; i++)
    {
      prop_desc = strct_def->buff[i];

      if(prop_desc.typename)
      {
        prop.key = prop_desc.key;
        prop.offset = prop_desc.offset;
        prop.size = prop_desc.size;
        prop.typename = prop_desc.typename;
        prop.val = prop_desc.val;
        Iris_ArrayPush(lit_expr->strct, prop);
      }
    }
    
    Iris_LexerConsumeToken(parser->lex);
  }
  else if(type == NODE_BOOLLIT)
  {
    token = Iris_LexerPickToken(parser->lex, 0);
    
    if(Iris_StrnCmp(token->start, "true", 4))
    {
      lit_expr->bool = true;
    }
    else
    {
      lit_expr->bool = false;
    }
    
    Iris_LexerConsumeToken(parser->lex);
  }
  else if(type == NODE_STRINGLIT)
  {
    token = Iris_LexerPickToken(parser->lex, 0);
    lit_expr->str.buff = strndup(token->start, token->len);
    lit_expr->str.len = token->len;
    Iris_LexerConsumeToken(parser->lex);
  }
  else if(type == NODE_NUMLIT)
  {
    token = Iris_LexerPickToken(parser->lex, 0);
    str_num = strndup(token->start, token->len);
    str_num[token->len] = '\0';
    lit_expr->f64 = strtod(str_num, null);
    free(str_num);
    Iris_LexerConsumeToken(parser->lex);
  }
  else if(type == NODE_NULLLIT ||
          type == NODE_INFINITYLIT ||
          type == NODE_NANLIT)
  {
    Iris_LexerConsumeToken(parser->lex);
  }
  
  return node;
}

static Iris_Node* Iris_ParserParsePrimaryExpr(Iris_Parser* parser, Iris_Node* parent, Iris_Scope* scope)
{
  Iris_Token* anything = Iris_LexerPickToken(parser->lex, 0);
  Iris_Token* tmp_token = null;
  Iris_Node* node = null;
  Iris_Node* fn_call = null;
  Iris_Node* tmp_node = null;
  Iris_VarStmt* var_stmt = null;
  Iris_LitExpr* lit_expr = null;
  Iris_StrctProp* prop = null;
  i8_t* str_num = null;

  if(anything->type == TK_BBRACES_L)
  {
    node = Iris_ParserParseLit(parser, parent, scope, NODE_STRCTLIT);
  }
  else if(anything->type == TK_STRINGLIT)
  {
    node = Iris_ParserParseLit(parser, parent, scope, NODE_STRINGLIT);
  }
  else if(anything->type == TK_NULLLIT)
  {
    node = Iris_ParserParseLit(parser, parent, scope, NODE_NULLLIT);
  }
  else if(anything->type == TK_INFINITYLIT)
  {
    node = Iris_ParserParseLit(parser, parent, scope, NODE_INFINITYLIT);
  }
  else if(anything->type == TK_NANLIT)
  {
    node = Iris_ParserParseLit(parser, parent, scope, NODE_NANLIT);
  }
  else if(anything->type == TK_TRUELIT || anything->type == TK_FALSELIT)
  {
    node = Iris_ParserParseLit(parser, parent, scope, NODE_BOOLLIT);
  }
  else if(anything->type == TK_NUMLIT)
  {
    node = Iris_ParserParseLit(parser, parent, scope, NODE_NUMLIT);
  }
  else if(Iris_ParserIsFnCallExpr(parser->lex, anything))
  {
    node = Iris_ParserParseFnCallExpr(parser, node, scope);
  }
  else if(anything->type == TK_IDENT)
  {
    var_stmt = Iris_ParserScopeGetVar(scope, anything);
 
    if(Iris_ParserIsStrctPropCall(parser->lex, anything))
    {
      Iris_LexerConsumeToken(parser->lex);
      Iris_LexerConsumeToken(parser->lex);
      anything = Iris_LexerPickToken(parser->lex, 0);
      lit_expr = var_stmt->val->as;
      prop = Iris_ParserGetStrctProp(((Iris_StrctDefStmt*)lit_expr->strct.def)->buff, ((Iris_StrctDefStmt*)lit_expr->strct.def)->len, anything);
      node = Iris_ParserCreateStrctPropAccessExpr(parser->arena, parent, null, prop->offset, var_stmt->offset);
      fn_call = Iris_ParserParseFnCallExpr(parser, node, scope);
      ((Iris_StrctPropAccessExpr*)node->as)->val = fn_call;
    }
    else if(Iris_ParserIsStrctPropAccess(parser->lex, anything))
    {
      Iris_LexerConsumeToken(parser->lex);
      Iris_LexerConsumeToken(parser->lex);
      anything = Iris_LexerConsumeToken(parser->lex);
      lit_expr = var_stmt->val->as;
      prop = Iris_ParserGetStrctProp(lit_expr->strct.buff, lit_expr->strct.len, anything);
      node = Iris_ParserCreateStrctPropAccessExpr(parser->arena, parent, prop->val, prop->offset, var_stmt->offset);
    }
    else
    {
      node = Iris_ParserCreateIdentExpr(parser->arena, parent, anything, var_stmt->offset);
      Iris_LexerConsumeToken(parser->lex);
    }
  }
  else if(anything->type == TK_BPAR_L)
  {
    Iris_LexerConsumeToken(parser->lex);
    node = Iris_ParserParseCondExpr(parser, node, scope);
    Iris_LexerConsumeToken(parser->lex);
  }

  return node;
}

static Iris_Node* Iris_ParserParseStrongTermExpr(Iris_Parser* parser, Iris_Node* parent, Iris_Scope* scope)
{
  Iris_Node* ls = Iris_ParserParsePrimaryExpr(parser, parent, scope);
  Iris_Node* rs = NULL;
  Iris_OprType op = OPR_UNKNOWN;
  Iris_Token* token = NULL;

  while((token = Iris_LexerPickToken(parser->lex, 0)) && (token->type == TK_MUL || token->type == TK_DIV)) 
  {
    op = Iris_ParserTokenTypeToOprType(token->type);
    Iris_LexerConsumeToken(parser->lex);
    rs = Iris_ParserParsePrimaryExpr(parser, parent, scope);
    ls = Iris_ParserCreateBinExpr(parser->arena, parent, ls, op, rs);
  }

  return ls;
}

static Iris_Node* Iris_ParserParseWeakTermExpr(Iris_Parser* parser, Iris_Node* parent, Iris_Scope* scope)
{
  Iris_Node* ls = Iris_ParserParseStrongTermExpr(parser, parent, scope);
  Iris_Node* rs = null;
  Iris_OprType op = OPR_UNKNOWN;
  Iris_Token* token = null;

  while((token = Iris_LexerPickToken(parser->lex, 0)) && 
        (token->type == TK_PLUS || token->type == TK_MINUS ||
         token->type == TK_AND || token->type == TK_OR)) 
  {
    op = Iris_ParserTokenTypeToOprType(token->type);
    Iris_LexerConsumeToken(parser->lex);
    rs = Iris_ParserParseStrongTermExpr(parser, parent, scope);
    ls = Iris_ParserCreateBinExpr(parser->arena, parent, ls, op, rs);
  }

  return ls;
}

Iris_Node* Iris_ParserParseCondExpr(Iris_Parser* parser, Iris_Node* parent, Iris_Scope* scope)
{
  Iris_Node* ls = Iris_ParserParseWeakTermExpr(parser, parent, scope);
  Iris_Node* rs = NULL;
  Iris_OprType op = OPR_UNKNOWN;
  Iris_Token* token = NULL;
  
  while((token = Iris_LexerPickToken(parser->lex, 0)) && (
        token->type == TK_EQ || token->type == TK_EQ_GT || token->type == TK_EQ_LT ||
        token->type == TK_GT || token->type == TK_LT ||
        token->type == TK_ASSIGN)) 
  {
    op = Iris_ParserTokenTypeToOprType(token->type);
    Iris_LexerConsumeToken(parser->lex);
    rs = Iris_ParserParseWeakTermExpr(parser, parent, scope);
    ls = Iris_ParserCreateBinExpr(parser->arena, parent, ls, op, rs);
  }

  return ls;
}

static Iris_Node* Iris_ParserParseVarStmt(Iris_Parser* parser, Iris_Node* fn, Iris_Node* block)
{
  Iris_Token* name = null;
  Iris_Token* typename = null;
  Iris_Node* var = null;
  Iris_VarStmt* var_stmt = null;
  Iris_FnDefStmt* fn_def = null;
  Iris_BlockStmt* block_stmt = block->as;
  i64_t offset = 0;

  Iris_LexerConsumeToken(parser->lex);            // keyword
  name = Iris_LexerConsumeToken(parser->lex);     // ident
  Iris_LexerConsumeToken(parser->lex);            // colon
  typename = Iris_LexerConsumeToken(parser->lex); // typename

  if(fn)
  {
    fn_def = fn->as;
    offset = fn_def->stack_size;
    fn_def->stack_size += Iris_ParserGetSizeFromTypename(&block_stmt->scope, typename);
  }

  var = Iris_ParserCreateVarStmt(parser->arena, block, name, offset);
  var_stmt = var->as;
  Iris_LexerConsumeToken(parser->lex); // assign  
  var_stmt->val = Iris_ParserParseCondExpr(parser, block, &block_stmt->scope);
  Iris_ArrayPush(block_stmt->scope.nodes, var);

  return var;
}

static Iris_Node* Iris_ParserParseRetStmt(Iris_Parser* parser, Iris_Node* fn, Iris_Node* block)
{
  Iris_Node* ret = Iris_ParserCreateRetStmt(parser->arena, block);
  Iris_RetStmt* ret_stmt = ret->as;
  Iris_BlockStmt* block_stmt = block->as;

  Iris_LexerConsumeToken(parser->lex); // keyword
  ret_stmt->val = Iris_ParserParseCondExpr(parser, fn, &block_stmt->scope);
  
  return ret;
}

static void_t Iris_ParserParseFnDefStmtArgs(Iris_Parser* parser, Iris_Node* fn_node)
{
  Iris_FnDefStmt* fn_def_stmt = fn_node->as;
  Iris_BlockStmt* block_stmt = fn_def_stmt->block->as;
  Iris_Node* arg = null;
  Iris_VarStmt* arg_stmt = null;
  Iris_Token* token = null;
  Iris_Token* typename = null;

  Iris_LexerConsumeToken(parser->lex); // lparenthesis

  while((token = Iris_LexerPickToken(parser->lex, 0)) && token->type != TK_BPAR_R)
  {
    fn_def_stmt->args_count++;
    arg = Iris_ParserCreateVarStmt(parser->arena, fn_node, token, fn_def_stmt->stack_size);
    arg_stmt = arg->as;
    Iris_ArrayPush(block_stmt->scope.nodes, arg);
    Iris_LexerConsumeToken(parser->lex);            // ident
    Iris_LexerConsumeToken(parser->lex);            // colon
    typename = Iris_LexerConsumeToken(parser->lex); // typename
    fn_def_stmt->stack_size += Iris_ParserGetSizeFromTypename(&block_stmt->scope, typename);
  }
 
  Iris_LexerConsumeToken(parser->lex); // colon
  Iris_LexerConsumeToken(parser->lex); // rettype
  Iris_LexerConsumeToken(parser->lex); // rparenthesis
}

static void_t Iris_ParserParseFnDefStmtBody(Iris_Parser* parser, Iris_Node* fn)
{
  Iris_FnDefStmt* fn_def = fn->as;
  Iris_BlockStmt* block_stmt = fn_def->block->as;
  Iris_Token* token = null;
  Iris_Node* node = null;
 
  Iris_LexerConsumeToken(parser->lex); //lbrace

  while((token = Iris_LexerPickToken(parser->lex, 0)) && token->type != TK_BBRACES_R)
  {
    node = Iris_ParserParseStmtOrExpr(parser, fn, fn_def->block);
    Iris_ArrayPush(block_stmt->nodes, node);
  }

  Iris_LexerConsumeToken(parser->lex); // rbrace
}

static Iris_Node* Iris_ParserParseFnDefStmt(Iris_Parser* parser, Iris_Node* root)
{
  Iris_BlockStmt* block_stmt = root->as;
  Iris_FnDefStmt* fn_def = null;
  Iris_Token* token = null;
  Iris_Node* node = null;

  token = Iris_LexerConsumeToken(parser->lex);  // ident
  Iris_LexerConsumeToken(parser->lex);          // colon
  node = Iris_ParserCreateFnDefStmt(parser->arena, root, token, &block_stmt->scope);
  fn_def = node->as;
  fn_def->stack_size += (i16_t)sizeof(i64_t) * 4;
  Iris_ParserParseFnDefStmtArgs(parser, node);
  Iris_ParserParseFnDefStmtBody(parser, node);
  Iris_ArrayPush(block_stmt->scope.nodes, node);
  
  return node;
}

static Iris_StrctProp Iris_ParserParseStrctStmtProp(Iris_Parser* parser, Iris_Node* block, i64_t* offset)
{
  Iris_Token* token = Iris_LexerPickToken(parser->lex, 0);
  Iris_Node* node = null;
  Iris_BlockStmt* block_stmt = block->as;
  Iris_StrctProp prop = {0};

  prop.key = token;
  prop.offset = *offset;
  prop.val = null;

  if(Iris_ParserIsFnDef(parser->lex, token))
  {
    node = Iris_ParserParseFnDefStmt(parser, block);
    Iris_ArrayPush(block_stmt->nodes, node);
    Iris_ArrayPush(block_stmt->scope.nodes, node);
    prop.size = 8;
    prop.typename = null;
    prop.val = node;
  }
  else
  {
    Iris_LexerConsumeToken(parser->lex);
    Iris_LexerConsumeToken(parser->lex);
    token = Iris_LexerConsumeToken(parser->lex);
    prop.typename = token;
    prop.size = Iris_ParserGetSizeFromTypename(&block_stmt->scope, token);
  }

  *offset += prop.size;

  return prop;
}

static Iris_Node* Iris_ParserParseStrctStmt(Iris_Parser* parser, Iris_Node* block)
{
  Iris_Token* token = null;
  Iris_Node* node = null;
  Iris_StrctDefStmt* strct_def = null;
  Iris_StrctProp prop = {0};
  Iris_BlockStmt* block_stmt = block->as;

  Iris_LexerConsumeToken(parser->lex);
  token = Iris_LexerConsumeToken(parser->lex);
  node = Iris_ParserCreateStrctDefStmt(parser->arena, block, token);
  strct_def = node->as;

  Iris_LexerConsumeToken(parser->lex);

  while((token = Iris_LexerPickToken(parser->lex, 0)) && token->type != TK_BBRACES_R)
  {
    prop = Iris_ParserParseStrctStmtProp(parser, block, &strct_def->size);
    Iris_ArrayPush(*strct_def, prop);
  }

  Iris_LexerConsumeToken(parser->lex);
  Iris_ArrayPush(block_stmt->scope.nodes, node);

  return node;
}

static Iris_Node* Iris_ParserParseIfStmt(Iris_Parser* parser, Iris_Node* fn, Iris_Node* block)
{
  Iris_BlockStmt* block_stmt = block->as;
  Iris_Token* token = null;
  Iris_IfStmt* stmt = null;
  Iris_Node* node = null;
  Iris_Node* expr = null;
  Iris_Node* child = null;
  Iris_NodeType kind = NODE_UNKNOWN;

  token = Iris_LexerConsumeToken(parser->lex);
  kind = Iris_StrnCmp(token->start, "else", 4) ? NODE_ELSE :
    Iris_StrnCmp(token->start, "if", 2) ? NODE_IF : NODE_ELIF;
  node = Iris_ParserCreateIfStmt(parser->arena, block, null, &block_stmt->scope, kind);
  expr = kind == NODE_ELSE ? null : Iris_ParserParseCondExpr(parser, fn, &block_stmt->scope);
  stmt = node->as;
  stmt->expr = expr;
  block_stmt = stmt->block->as;
  Iris_LexerConsumeToken(parser->lex);
  while((token = Iris_LexerPickToken(parser->lex, 0)) && token->type != TK_BBRACES_R)
  {
    child = Iris_ParserParseStmtOrExpr(parser, fn, stmt->block);
    Iris_ArrayPush(block_stmt->nodes, child);
  }
  Iris_LexerConsumeToken(parser->lex);

  return node;
}

static Iris_Node* Iris_ParserParseForStmt(Iris_Parser* parser, Iris_Node* fn, Iris_Node* block)
{
  Iris_Token* token = null;
  Iris_Node* node = null;
  Iris_Node* child = null;
  Iris_ForStmt* stmt = null;
  Iris_BlockStmt* block_stmt = null;
  Iris_Node* block_node = null;

  node = Iris_ParserCreateForStmt(parser->arena, block, &((Iris_BlockStmt*)block->as)->scope);
  stmt = node->as;
  block_stmt = stmt->block->as;
  Iris_LexerConsumeToken(parser->lex);

  stmt->init = Iris_ParserParseStmtOrExpr(parser, fn, stmt->block);
  Iris_LexerConsumeToken(parser->lex);
  stmt->expr = Iris_ParserParseCondExpr(parser, fn, &block_stmt->scope);
  Iris_LexerConsumeToken(parser->lex);
  stmt->inc = Iris_ParserParseCondExpr(parser, fn, &block_stmt->scope);
  Iris_LexerConsumeToken(parser->lex);
  while((token = Iris_LexerPickToken(parser->lex, 0)) && token->type != TK_BBRACES_R)
  {
    child = Iris_ParserParseStmtOrExpr(parser, fn, stmt->block);
    Iris_ArrayPush(block_stmt->nodes, child);
  }
  Iris_LexerConsumeToken(parser->lex);

  return node;
}

static Iris_Node* Iris_ParserParseKeyword(Iris_Parser* parser, Iris_Node* fn, Iris_Node* block)
{
  const Iris_Token* keyword = Iris_LexerPickToken(parser->lex, 0);

  if(Iris_StrnCmp(keyword->start, "strct", Iris_Max(keyword->len, 5)))
  {
    return Iris_ParserParseStrctStmt(parser, block);
  }
  else if(Iris_StrnCmp(keyword->start, "if", 2) ||
          Iris_StrnCmp(keyword->start, "else", 4) ||
          Iris_StrnCmp(keyword->start, "elif", 4))
  {
    return Iris_ParserParseIfStmt(parser, fn, block);
  }
  else if(Iris_StrnCmp(keyword->start, "for", 3))
  {
    return Iris_ParserParseForStmt(parser, fn, block);
  }
  else if(Iris_StrnCmp(keyword->start, "for", 3))
  {
    return Iris_ParserParseForStmt(parser, fn, block);
  }
  else if(Iris_StrnCmp(keyword->start, "var", Iris_Max(keyword->len, 3)))
  {
    return Iris_ParserParseVarStmt(parser, fn, block);
  }
  else if(Iris_StrnCmp(keyword->start, "ret", Iris_Max(keyword->len, 3)))
  {
    return Iris_ParserParseRetStmt(parser, fn, block);    
  }

  return null;
}

Iris_Node* Iris_ParserParseStmtOrExpr(Iris_Parser* parser, Iris_Node* fn, Iris_Node* block)
{
  Iris_BlockStmt* block_stmt = block->as;
  Iris_Token* token = Iris_LexerPickToken(parser->lex, 0);
  
  if(token->type == TK_KEYWORD)
  {
    return Iris_ParserParseKeyword(parser, fn, block); 
  }
  else if(token->type == TK_IDENT)
  {
    if(Iris_ParserIsFnDef(parser->lex, token))
    {
      return Iris_ParserParseFnDefStmt(parser, block);
    }
    else
    {
      return Iris_ParserParseCondExpr(parser, block, &block_stmt->scope);
    }
  }
  else
  {
    return Iris_ParserParseCondExpr(parser, block, &block_stmt->scope);
  }
}

void_t Iris_ParserNew(Iris_Parser* parser, Iris_Lexer* lex)
{
  parser->lex = lex;
  Iris_ArenaNew(parser->arena, IRIS_PARSER_DEF_ARENA_SIZE);
}

void_t Iris_ParserParse(Iris_Parser* parser)
{
  Iris_Node* block = Iris_ParserCreateBlockStmt(parser->arena, null, null);
  Iris_Token* token = null;
  Iris_Node* node = null;

  while((token = Iris_LexerPickToken(parser->lex, 0)))
  {
    node = Iris_ParserParseStmtOrExpr(parser, null, block);
    
    if(node->type != NODE_STRCT_DEF)
    {
      Iris_ArrayPush(((Iris_BlockStmt*)block->as)->nodes, node);
    }
  }

  parser->root = block;
}

void_t Iris_ParserDump(const Iris_Node* node, i64_t max_depth)
{
  const i64_t depth_step = 2;
  static i64_t depth = 0;
 
  if(depth != -1 && depth == max_depth)
  {
    return;
  }

  switch(node->type)
  {
    case NODE_FN_DEF:
    {
      Iris_FnDefStmt* fn_def = node->as;
      Iris_BlockStmt* block = fn_def->block->as;
      Iris_ParserPrint(depth, node->type, "%.*s(argc: %i stksz: %i blksz: %li scpsz: %li)\n",
        fn_def->name->len, fn_def->name->start,
        fn_def->args_count,
        fn_def->stack_size,
        block->nodes.len,
        block->scope.nodes.len);
      
      depth += depth_step;
      Iris_ParserDump(fn_def->block, max_depth);
      depth -= depth_step;
    }
      break;
    case NODE_FN_CALL:
    {
      Iris_FnCallExpr* call_expr = node->as;
      Iris_ParserPrint(depth, node->type, "%.*s\n", call_expr->name->len, call_expr->name->start);

      depth += depth_step;
      for(i64_t i = 0; i < call_expr->args.len; i++)
      {
        Iris_ParserDump(call_expr->args.buff[i], max_depth);
      }
      depth -= depth_step;
    }
      break;
    case NODE_BLCK:
    {
      Iris_BlockStmt* block_stmt = node->as;
      Iris_ParserPrint(depth, node->type, "%li\n", block_stmt->nodes.len);

      depth += depth_step;
      for(i64_t i = 0; i < block_stmt->nodes.len; i++)
      {
        Iris_ParserDump(block_stmt->nodes.buff[i], max_depth);
      }
      depth -= depth_step;
    }
      break;
    case NODE_VAR:
    {
      Iris_VarStmt* var_stmt = node->as;
      Iris_ParserPrint(depth, node->type, "%.*s[%li]\n", var_stmt->name->len, var_stmt->name->start, var_stmt->offset);

      if(var_stmt->val)
      {
        depth += depth_step;
        Iris_ParserDump(var_stmt->val, max_depth);
        depth -= depth_step;
      }
    }
      break;
    case NODE_BIN:
    {
      Iris_BinExpr* bin_expr = node->as;
      Iris_ParserPrint(depth, node->type, "%s\n", Iris_ParserOprtTypeToString(bin_expr->op));
      depth += depth_step;
      Iris_ParserDump(bin_expr->ls, max_depth);
      depth -= depth_step;
      depth += depth_step;
      Iris_ParserDump(bin_expr->rs, max_depth);
      depth -= depth_step;
    }
      break;
    case NODE_IDENT:
    {
      Iris_IdentExpr* ident_expr = node->as;
      Iris_ParserPrint(depth, node->type, "%.*s[%li]\n",
        ident_expr->name->len, ident_expr->name->start, 
        ident_expr->stack_offset
      );
    }
      break;
    case NODE_RET:
    {
      Iris_RetStmt* ret_stmt = node->as;
      Iris_ParserPrint(depth, node->type, "\n", null);
      depth += depth_step;
      Iris_ParserDump(ret_stmt->val, max_depth);
      depth -= depth_step;
    }
      break;
    case NODE_STRCT_DEF:
    {
    }
      break;
    case NODE_STRCT_PROP_ACCESS:
    {
      Iris_StrctPropAccessExpr* access = node->as;
      Iris_ParserPrint(depth, NODE_STRCT_PROP_ACCESS, "[%li]:[%li]\n", access->stack_offset, access->instrct_offset);
      depth += depth_step;
      Iris_ParserDump(access->val, max_depth);
      depth -= depth_step;
    }
      break;
    case NODE_NUMLIT:
    {
      Iris_LitExpr* lit_expr = node->as;
      Iris_ParserPrint(depth, node->type, "%.12f\n", lit_expr->f64);
    }
      break;
    case NODE_IF:
    {
      Iris_IfStmt* stmt = node->as;
      Iris_ParserPrint(depth, NODE_IF, "\n", null);
      depth += depth_step;
      Iris_ParserDump(stmt->expr, max_depth);
      depth -= depth_step;
      depth += depth_step;
      Iris_ParserDump(stmt->block, max_depth);
      depth -= depth_step;
    }
      break;
    case NODE_ELSE:
    {
      Iris_IfStmt* stmt = node->as;
      Iris_ParserPrint(depth, NODE_ELSE, "\n", null);
      depth += depth_step;
      Iris_ParserDump(stmt->block, max_depth);
      depth -= depth_step;
    }
      break;
    case NODE_ELIF:
    {
      Iris_IfStmt* stmt = node->as;
      Iris_ParserPrint(depth, NODE_ELIF, "\n", null);
      depth += depth_step;
      Iris_ParserDump(stmt->expr, max_depth);
      depth -= depth_step;
      depth += depth_step;
      Iris_ParserDump(stmt->block, max_depth);
      depth -= depth_step;
    }
      break;
    case NODE_FOR:
    {
      Iris_ForStmt* stmt = node->as;
      Iris_ParserPrint(depth, NODE_FOR, "\n", null);
      depth += depth_step;
      Iris_ParserDump(stmt->init, max_depth);
      depth -= depth_step;
      depth += depth_step;
      Iris_ParserDump(stmt->expr, max_depth);
      depth -= depth_step;
      depth += depth_step;
      Iris_ParserDump(stmt->inc, max_depth);
      depth -= depth_step;
      depth += depth_step;
      Iris_ParserDump(stmt->block, max_depth);
      depth -= depth_step;
    }
      break;
    case NODE_BOOLLIT:
    {
      Iris_LitExpr* expr = node->as;
      Iris_ParserPrint(depth, NODE_BOOLLIT, "%s\n", expr->bool ? "true" : "false");
    }
      break;
    case NODE_STRINGLIT:
    {
      Iris_LitExpr* lit_expr = node->as;
      Iris_ParserPrint(depth, node->type, "%.*s\n", lit_expr->str.len, lit_expr->str.buff);
    }
      break;
    case NODE_NULLLIT:
      Iris_ParserPrint(depth, NODE_NULLLIT, "nil\n", null);
      break;
    case NODE_NANLIT:
      Iris_ParserPrint(depth, NODE_NANLIT, "nan\n", null);
      break;
    case NODE_INFINITYLIT:
      Iris_ParserPrint(depth, NODE_INFINITYLIT, "infinity\n", null);
      break;
    case NODE_STRCTLIT:
    {
      Iris_Strct* lit = node->as;
      Iris_StrctDefStmt* stmt = (ptr_t)lit->def;
      Iris_ParserPrint(depth, NODE_STRCTLIT, "%.*s[%i]\n", stmt->name->len, stmt->name->start, stmt->size);
      depth += depth_step;
      for(i64_t i = 0; i < lit->len; i++)
      {
        Iris_ParserDump(lit->buff[i].val, max_depth);
      }
      depth -= depth_step;
    }
      break;
    case NODE_UNKNOWN:
    default:
      printf(IRIS_ANSI_DIM"Unknown Node\n"IRIS_ANSI_RES);
  }
}

const i8_t* Iris_ParserNodeTypeToString(Iris_NodeType type)
{
  switch(type) 
  {
    case NODE_BLCK:              return "Block";
    case NODE_BIN:               return "Bin";
    case NODE_NUMLIT:            return "NumLit";
    case NODE_VAR:               return "Var";
    case NODE_FN_DEF:            return "FnDef";
    case NODE_FN_CALL:           return "FnCall";
    case NODE_RET:               return "Ret";
    case NODE_IF:                return "If";
    case NODE_ELSE:              return "Else";
    case NODE_ELIF:              return "Elif";
    case NODE_FOR:               return "For";
    case NODE_IDENT:             return "Ident";
    case NODE_STRCT_DEF:         return "StrctDef";
    case NODE_STRCTLIT:          return "StrctLit";
    case NODE_STRINGLIT:         return "StringLit";
    case NODE_BOOLLIT:           return "BoolLit";
    case NODE_NANLIT:            return "NanLit";
    case NODE_INFINITYLIT:       return "InfinityLit";
    case NODE_NULLLIT:           return "NilLit";
    case NODE_STRCT_PROP_ACCESS: return "StrctPropAccess";
    case NODE_STRCT_PROP:        return "StrctProp";
    case NODE_UNKNOWN:
    default:
      return "Unknown";
  }
}

const i8_t* Iris_ParserOprtTypeToString(Iris_OprType type)
{
  switch(type)
  {
    case OPR_PLUS:    return "+";
    case OPR_MINUS:   return "-";
    case OPR_MUL:     return "*";
    case OPR_DIV:     return "/";
    case OPR_ASSIGN:  return "=";
    case OPR_EQ:      return "==";
    case OPR_NOT_EQ:  return "!=";
    case OPR_EQ_GT:   return ">=";
    case OPR_EQ_LT:   return "<=";
    case OPR_GT:      return ">";
    case OPR_LT:      return "<";
    case OPR_AND:     return "&&";
    case OPR_OR:      return "||";
    case OPR_UNKNOWN: 
    default:
      return "???";
  }
}
