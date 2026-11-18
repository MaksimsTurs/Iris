#include "iris/parser.h"

#define Iris_ParserIsEof($token) ((($token) == null) || (($token)->type == TK_EOF))
#define Iris_ParserTKList(...)   ((const Iris_TokenKind[]){ __VA_ARGS__, (Iris_TokenKind)-1 })
#define Iris_ParserReportNotFoundError($lex, $token)                  Iris_ReportNotFoundError(($lex)->src.modulepath, ($token)->start, ($token)->len, ($token)->row, ($token)->col)
#define Iris_ParserReportRedefinitionError($lex, $redef)              Iris_ReportRedefinitionError(($lex)->src.modulepath, ($redef)->start, ($redef)->len, ($redef)->row, ($redef)->col)
#define Iris_ParserReportSyntaxError($lex, $msg, $kinds, $next_token) Iris_ReportSyntaxError(($lex)->src.modulepath, ($msg), ($kinds), ($next_token)->start, ($next_token)->len, ($next_token)->row, ($next_token)->col)

Iris_Node* Iris_ParserParseAssignExpr(Iris_Parser* parser, Iris_Node* parent, Iris_Scope* scope);
Iris_Node* Iris_ParserParseStmtOrExpr(Iris_Parser* parser, Iris_Node* fn, Iris_Node* block);

static Iris_FnDefStmt* Iris_ParserScopeGetFnDef(const Iris_Scope* scope, const char_t* name, i64_t len)
{
  Iris_FnDefStmt* stmt = null;

  for(i64_t i = 0; i < scope->nodes.len; i++)
  {
    if(scope->nodes.buff[i]->type == NODE_FN_DEF)
    {
      stmt = scope->nodes.buff[i]->as;
      if(Iris_StrnCmp(stmt->atom.buff, name, Iris_Max(stmt->atom.len, len)))  return stmt;
    }
  }

  if(scope->parent) return Iris_ParserScopeGetFnDef(scope->parent, name, len);

  return null;
}

static Iris_VarStmt* Iris_ParserScopeGetVar(Iris_Scope* scope, const char_t* name, i64_t len)
{
  Iris_VarStmt* stmt = null;
  
  for(i64_t i = 0; i < scope->nodes.len; i++)
  {
    if(scope->nodes.buff[i]->type == NODE_VAR)
    {
      stmt = scope->nodes.buff[i]->as;
      if(Iris_StrnCmp(stmt->atom.buff, name, Iris_Max(stmt->atom.len, len))) return stmt;
    }
  }

  if(scope->parent) return Iris_ParserScopeGetVar(scope->parent, name, len);

  return null;
}

static Iris_StrctDefStmt* Iris_ParserScopeGetStrctDef(Iris_Scope* scope, const char_t* name, i64_t len)
{
  Iris_StrctDefStmt* stmt = null;

  for(i64_t i = 0; i < scope->nodes.len; i++)
  {
    if(scope->nodes.buff[i]->type == NODE_STRCT_DEF)
    {
      stmt = scope->nodes.buff[i]->as; 
      if(Iris_StrnCmp(stmt->atom.buff, name, Iris_Max(stmt->atom.len, len))) return stmt;
    }
  }

  if(scope->parent) return Iris_ParserScopeGetStrctDef(scope->parent, name, len);

  return null;
}

static Iris_StrctPropStmt* Iris_ParserGetStrctProp(Iris_StrctProps* props, const char_t* name, i64_t len)
{
  Iris_StrctPropStmt* prop = null;

  for(i64_t i = 0; i < props->len; i++)
  {
    prop = &props->buff[i];
    if(Iris_StrnCmp(prop->atom.buff, name, Iris_Max(prop->atom.len, len))) return prop;
  }

  return null;
}

static Iris_OprKind Iris_ParserTokenKindToOprKind(Iris_TokenKind kind)
{
  if(kind == TK_PLUS)       return OPR_PLUS;
  if(kind == TK_MINUS)      return OPR_MINUS;
  if(kind == TK_MUL)        return OPR_MUL;
  if(kind == TK_DIV)        return OPR_DIV;
  if(kind == TK_MOD)        return OPR_MOD;
  if(kind == TK_ASSIGN)     return OPR_ASSIGN;
  if(kind == TK_EQ)         return OPR_EQ;
  if(kind == TK_NOT_EQ)     return OPR_NOT_EQ;
  if(kind == TK_NOT)        return OPR_NOT;
  if(kind == TK_GT)         return OPR_GT;
  if(kind == TK_LT)         return OPR_LT;
  if(kind == TK_EQ_GT)      return OPR_EQ_GT;
  if(kind == TK_EQ_LT)      return OPR_EQ_LT;
  if(kind == TK_AND)        return OPR_AND;
  if(kind == TK_OR)         return OPR_OR;
  if(kind == TK_BIT_OR)     return OPR_BIT_OR;
  if(kind == TK_BIT_AND)    return OPR_BIT_AND;
  if(kind == TK_BIT_NOT)    return OPR_BIT_NOT;
  if(kind == TK_BIT_XOR)    return OPR_BIT_XOR;
  if(kind == TK_BIT_LSHIFT) return OPR_BIT_LSHIFT;
  if(kind == TK_BIT_RSHIFT) return OPR_BIT_RSHIFT;
  assert(false && "Unknown operator!");
}

static Iris_TypeKind Iris_ParserGetTypeKindFromTypename(const Iris_Token* token)
{
  if(token->type == TK_IDENT)                return TP_STRCT;
  if(Iris_StrnCmp(token->start, "ptr", 3))   return TP_PTR;
  if(Iris_StrnCmp(token->start, "char", 4))  return TP_CHAR;
  if(Iris_StrnCmp(token->start, "bool", 4))  return TP_BOOL;
  if(Iris_StrnCmp(token->start, "nil", 3))   return TP_NIL;
  if(Iris_StrnCmp(token->start, "nan", 3))   return TP_NAN;
  if(Iris_StrnCmp(token->start, "inf", 3))   return TP_INF;
  if(Iris_StrnCmp(token->start, "void", 4))  return TP_VOID;
  if(Iris_StrnCmp(token->start, "i8",  2))   return TP_I8;
  if(Iris_StrnCmp(token->start, "u8",  2))   return TP_U8;
  if(Iris_StrnCmp(token->start, "i16", 3))   return TP_I16;
  if(Iris_StrnCmp(token->start, "u16", 3))   return TP_U16;
  if(Iris_StrnCmp(token->start, "i32", 3))   return TP_I32;
  if(Iris_StrnCmp(token->start, "u32", 3))   return TP_U32;
  if(Iris_StrnCmp(token->start, "i64", 3))   return TP_I64;
  if(Iris_StrnCmp(token->start, "u64", 3))   return TP_U64;
  if(Iris_StrnCmp(token->start, "f32", 3))   return TP_F32;
  if(Iris_StrnCmp(token->start, "f64", 3))   return TP_F64;
  assert(false && "Unknown type!\n");
}

static i64_t Iris_ParserGetSizeOfTypeKindOrTypename(const Iris_Token* typename, Iris_TypeKind type, Iris_Scope* scope)
{
  Iris_StrctDefStmt* strct = null;

  if(type == TP_STRCT)
  {
    strct = Iris_ParserScopeGetStrctDef(scope, typename->start, typename->len);
    if(!strct) return 0;
    return strct->size;
  }
  else
  {
    switch(type)
    {
      case TP_NIL:    return 0;
      case TP_VOID:   return 0;
      case TP_BOOL:
      case TP_CHAR:
      case TP_I8:
      case TP_U8:     return 1;
      case TP_I16:
      case TP_U16:    return 2;
      case TP_I32:
      case TP_U32:    return 4;
      case TP_I64:
      case TP_U64:    return 8;
      case TP_F32:    return 4;
      case TP_INF:
      case TP_NAN:
      case TP_F64:    return 8;
      case TP_PTR:    return 8;
      case TP_STRCT:  return 0;
      default:
        assert(false && "Unknown type kind!\n");
    }
  }

  return 0;
}

static Iris_Token* Iris_ParserExpectAndConsume(Iris_Lexer* lex, const Iris_TokenKind* kinds)
{
  Iris_Token* token = Iris_LexerConsumeToken(lex);
  Iris_TokenKind kind = -1;
  if(kinds) for(i64_t i = 0; (kind = kinds[i]) != -1; i++) if(token->type == kinds[i]) return token;
  Iris_ParserReportSyntaxError(lex, "expected", kinds, token);
  return token;
}

static Iris_Token* Iris_ParserExpectAndPick(Iris_Lexer* lex, i64_t offset, const Iris_TokenKind* kinds)
{
  Iris_Token* token = Iris_LexerPickToken(lex, offset);
  Iris_TokenKind kind = -1;
  if(kinds) for(i64_t i = 0; (kind = kinds[i]) != -1; i++) if(token->type == kinds[i]) return token;
  Iris_ParserReportSyntaxError(lex, "expected", kinds, token);
  return token;
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

static Iris_Atom Iris_ParserNewAtom(const Iris_Token* token)
{
  Iris_Atom atom = { .buff = null, .len = token->len };
  atom.buff = malloc((u64_t)atom.len);
  // TODO: Implement custom strncpy but for i16_t?
  strncpy(atom.buff, token->start, (u64_t)token->len);
  return atom;
}

static Iris_Atom Iris_ParserNewAtomWithPrefix(const Iris_Token* token, const char_t* prefix, i64_t len)
{
  Iris_Atom atom = { .buff = null, .len = token->len + len + 1 };
  atom.buff = malloc((u64_t)atom.len);
  // TODO: Implement custom strncpy but for i16_t?
  strncpy(atom.buff, prefix, (u64_t)len);
  atom.buff[len] = ':';
  // TODO: Implement custom strncpy but for i16_t?
  strncpy(atom.buff + len + 1, token->start, (u64_t)token->len);
  return atom;
}

static Iris_Node* Iris_ParserNewNode(Iris_Arena* arena, Iris_Node* parent, Iris_NodeType type, i64_t sizeofval)
{
  Iris_Node* node = null;
  
  Iris_ArenaAlloc(arena, node, (i64_t)sizeof(Iris_Node) + sizeofval);
  node->type = type;
  node->parent = parent;
  node->as = node + 1;

  return node;
}

static Iris_Node* Iris_ParserNewBodyStmt(Iris_Arena* arena, Iris_Node* parent, Iris_Scope* parent_scope)
{
  Iris_Node* node = Iris_ParserNewNode(arena, parent, NODE_BODY, sizeof(Iris_BodyStmt));
  
  Iris_Cast(Iris_BodyStmt*, node->as)->scope.parent = parent_scope;
  Iris_Cast(Iris_BodyStmt*, node->as)->memsize = 0;
  Iris_ArrayNew(Iris_Cast(Iris_BodyStmt*, node->as)->nodes,       IRIS_PARSER_DEF_NODE_ARR_CAP);
  if(!Iris_Cast(Iris_BodyStmt*, node->as)->nodes.buff)       Iris_ReportInternError("could not initialize body buffer");
  Iris_ArrayNew(Iris_Cast(Iris_BodyStmt*, node->as)->scope.nodes, IRIS_PARSER_DEF_NODE_ARR_CAP);
  if(!Iris_Cast(Iris_BodyStmt*, node->as)->scope.nodes.buff) Iris_ReportInternError("could not initialize body scope buffer");
 
  return node;
}

static Iris_Node* Iris_ParserNewStrctDefStmt(Iris_Arena* arena, Iris_Node* parent, Iris_Token* name)
{
  Iris_Node* node = Iris_ParserNewNode(arena, parent, NODE_STRCT_DEF, sizeof(Iris_StrctDefStmt));

  Iris_Cast(Iris_StrctDefStmt*, node->as)->atom = Iris_ParserNewAtom(name);
  Iris_Cast(Iris_StrctDefStmt*, node->as)->size = 0;
  Iris_ArrayNew(Iris_Cast(Iris_StrctDefStmt*, node->as)->props, IRIS_PARSER_DEF_NODE_ARR_CAP);
  if(!Iris_Cast(Iris_StrctDefStmt*, node->as)->props.buff) Iris_ReportInternError("could not initialize struct props buffer");
 
  return node;
}

// TODO: Maybe remove this.
// static Iris_Node* Iris_ParserNewStrctPropStmt(Iris_Arena* arena, Iris_Node* parent, Iris_Token* key, i64_t offset, Iris_Token* typename, Iris_Node* val)
// {
//   Iris_Node* node = Iris_ParserNewNode(arena, parent, NODE_STRCT_PROP, sizeof(Iris_StrctPropStmt));
//
//   Iris_Cast(Iris_StrctPropStmt*, node->as)->key = key;
//   Iris_Cast(Iris_StrctPropStmt*, node->as)->offset = offset;
//   Iris_Cast(Iris_StrctPropStmt*, node->as)->typename = typename;
//   Iris_Cast(Iris_StrctPropStmt*, node->as)->val = val;
//
//   return node;
// }

static Iris_Node* Iris_ParserNewFnDefStmt(Iris_Arena* arena, Iris_Node* parent, Iris_Atom* prefix, Iris_Token* name, Iris_Scope* parent_scope)
{
  Iris_Node* node = Iris_ParserNewNode(arena, parent, NODE_FN_DEF, sizeof(Iris_FnDefStmt));

  Iris_Cast(Iris_FnDefStmt*, node->as)->body = Iris_ParserNewBodyStmt(arena, node, parent_scope);
  Iris_Cast(Iris_FnDefStmt*, node->as)->args_count = 0;
  if(prefix)
    Iris_Cast(Iris_FnDefStmt*, node->as)->atom = Iris_ParserNewAtomWithPrefix(name, prefix->buff, prefix->len);
  else
    Iris_Cast(Iris_FnDefStmt*, node->as)->atom = Iris_ParserNewAtom(name);

  return node;
}

static Iris_Node* Iris_ParserNewVarStmt(Iris_Arena* arena, Iris_Node* parent, Iris_Token* name, Iris_Token* typename, i64_t offset)
{
  Iris_Node* node = Iris_ParserNewNode(arena, parent, NODE_VAR, sizeof(Iris_VarStmt));

  Iris_Cast(Iris_VarStmt*, node->as)->atom = Iris_ParserNewAtom(name);
  Iris_Cast(Iris_VarStmt*, node->as)->offset = offset;
  Iris_Cast(Iris_VarStmt*, node->as)->val = null;
  Iris_Cast(Iris_VarStmt*, node->as)->type = Iris_ParserGetTypeKindFromTypename(typename);
  Iris_Cast(Iris_VarStmt*, node->as)->typename = typename;
  Iris_Cast(Iris_VarStmt*, node->as)->is_global = false;

  return node;
}

static Iris_Node* Iris_ParserNewIfStmt(Iris_Arena* arena, Iris_Node* parent, Iris_Node* cond, Iris_NodeType type, Iris_Scope* parent_scope)
{
  Iris_Node* node = Iris_ParserNewNode(arena, parent, type, sizeof(Iris_IfStmt));

  Iris_Cast(Iris_IfStmt*, node->as)->cond = cond;
  Iris_Cast(Iris_IfStmt*, node->as)->body = Iris_ParserNewBodyStmt(arena, node, parent_scope);
  Iris_ArrayNew(Iris_Cast(Iris_IfStmt*, node->as)->nextconds, IRIS_PARSER_DEF_NODE_ARR_CAP);
  if(!Iris_Cast(Iris_IfStmt*, node->as)->nextconds.buff) Iris_ReportInternError("could not initialize conditions buffer");

  return node;
}

static Iris_Node* Iris_ParserNewForStmt(Iris_Arena* arena, Iris_Node* parent, Iris_Scope* parent_scope)
{
  Iris_Node* node = Iris_ParserNewNode(arena, parent, NODE_FOR, sizeof(Iris_ForStmt));

  Iris_Cast(Iris_ForStmt*, node->as)->init = null;
  Iris_Cast(Iris_ForStmt*, node->as)->cond = null;
  Iris_Cast(Iris_ForStmt*, node->as)->inc = null;
  Iris_Cast(Iris_ForStmt*, node->as)->body = Iris_ParserNewBodyStmt(arena, node, parent_scope);

  return node;
}

static Iris_Node* Iris_ParserNewRetStmt(Iris_Arena* arena, Iris_Node* parent, Iris_Node* val)
{
  Iris_Node* node = Iris_ParserNewNode(arena, parent, NODE_RET, sizeof(Iris_RetStmt));
  Iris_Cast(Iris_RetStmt*, node->as)->val = val;
  return node;
}

static Iris_Node* Iris_ParserNewLitExpr(Iris_Arena* arena, Iris_Node* parent, Iris_NodeType type)
{
  return Iris_ParserNewNode(arena, parent, type, sizeof(Iris_LitExpr));
}

static Iris_Node* Iris_ParserNewIdentExpr(Iris_Arena* arena, Iris_Node* parent, Iris_VarStmt* var)
{
  Iris_Node* node = Iris_ParserNewNode(arena, parent, NODE_IDENT, sizeof(Iris_IdentExpr));
  Iris_Cast(Iris_IdentExpr*, node->as)->def = Iris_Cast(ptr_t, var);
  return node;
}

static Iris_Node* Iris_ParserNewStrctPropAccessExpr(Iris_Arena* arena, Iris_Node* parent, Iris_Node* prop, Iris_NodeType type)
{
  Iris_Node* node = Iris_ParserNewNode(arena, parent, type, sizeof(Iris_StrctPropAccessExpr));
  Iris_Cast(Iris_StrctPropAccessExpr*, node->as)->prop = prop;
  return node;
}

static Iris_Node* Iris_ParserNewBinExpr(Iris_Arena* arena, Iris_Node* parent, Iris_Node* ls, Iris_OprKind op, Iris_Node* rs)
{
  Iris_Node* node = Iris_ParserNewNode(arena, parent, NODE_BIN, sizeof(Iris_BinExpr));

  ls->parent = node;
  rs->parent = node;
  Iris_Cast(Iris_BinExpr*, node->as)->ls = ls;
  Iris_Cast(Iris_BinExpr*, node->as)->rs = rs;
  Iris_Cast(Iris_BinExpr*, node->as)->op = op;

  return node;
}

static Iris_Node* Iris_ParserNewUnaryExpr(Iris_Arena* arena, Iris_Node* parent, Iris_OprKind op, Iris_Node* operand)
{
  Iris_Node* node = Iris_ParserNewNode(arena, parent, NODE_UNARY, sizeof(Iris_UnaryExpr));

  operand->parent = node;
  Iris_Cast(Iris_UnaryExpr*, node->as)->operand = operand;
  Iris_Cast(Iris_UnaryExpr*, node->as)->op = op;

  return node;
}

static Iris_Node* Iris_ParserNewFnCallExpr(Iris_Arena* arena, Iris_Node* parent, Iris_Token* token, Iris_Atom* atom)
{
  Iris_Node* node = Iris_ParserNewNode(arena, parent, NODE_FN_CALL, sizeof(Iris_FnCallExpr));

  Iris_Cast(Iris_FnCallExpr*, node->as)->atom = !token && atom ? *atom : Iris_ParserNewAtom(token);
  Iris_ArrayNew(Iris_Cast(Iris_FnCallExpr*, node->as)->nodes, IRIS_PARSER_DEF_NODE_ARR_CAP);
  if(!Iris_Cast(Iris_FnCallExpr*, node->as)->nodes.buff) Iris_ReportInternError("could not initialize call expr nodes buffer");

  return node;
}

static Iris_Node* Iris_ParserGetDefaultVal(Iris_Parser* parser, Iris_Node* parent, Iris_Scope* scope, Iris_TypeKind type, Iris_Token* typename)
{
  Iris_Node* val = null;
  Iris_StrctDefStmt* strct = null;
  Iris_StrctPropStmt prop = {0};
  Iris_StrctPropStmt prop_desc = {0};

  if(typename->type == TK_TYPE)
  {
    val = Iris_ParserNewLitExpr(parser->arena, parent, NODE_NUMLIT);
    
    if(type == TP_I8  ||
       type == TP_U8  ||
       type == TP_I16 ||
       type == TP_U16 ||
       type == TP_I32 ||
       type == TP_U32 ||
       type == TP_I64 ||
       type == TP_U64) Iris_Cast(Iris_LitExpr*, val->as)->i64 = 0;
    
    if(type == TP_F32 || type == TP_F64) Iris_Cast(Iris_LitExpr*, val->as)->f64 = 0;
  }
  else
  {
    val = Iris_ParserNewLitExpr(parser->arena, parent, NODE_STRCTLIT);
    strct = Iris_ParserScopeGetStrctDef(scope, typename->start, typename->len);
    if(!strct) Iris_ParserReportNotFoundError(parser->lex, typename);
    Iris_ArrayNew(Iris_Cast(Iris_LitExpr*, val->as)->strct.props, IRIS_PARSER_DEF_NODE_ARR_CAP);
    Iris_Cast(Iris_LitExpr*, val->as)->strct.def = (ptr_t)strct;

    for(i64_t i = 0; i < strct->props.len; i++)
    {
      prop_desc = strct->props.buff[i];
      prop.atom = prop_desc.atom;
      prop.offset = prop_desc.offset;
      prop.typename = prop_desc.typename;
      prop.type = prop_desc.type;

      if(prop.type != TP_PTR) prop.val = Iris_ParserGetDefaultVal(parser, val, scope, prop.type, prop.typename);
      else                    prop.val = strct->props.buff[i].val;
      
      Iris_ArrayPush(Iris_Cast(Iris_LitExpr*, val->as)->strct.props, prop);
    }
  }

  return val;
}

static void_t Iris_ParserParseFnCallArgList(Iris_Parser* parser, Iris_Scope* scope, Iris_Node* node)
{
  Iris_Node* arg = null;
  Iris_Token* token = null;

  Iris_ParserExpectAndConsume(parser->lex, Iris_ParserTKList(TK_BPAR_L));
  while((token = Iris_LexerPickToken(parser->lex, 0)) && token->type != TK_BPAR_R)
  {
    arg = Iris_ParserParseAssignExpr(parser, node, scope);
    Iris_ArrayPush(Iris_Cast(Iris_FnCallExpr*, node->as)->nodes, arg);
    if(Iris_LexerPickToken(parser->lex, 0)->type != TK_BPAR_R) Iris_ParserExpectAndConsume(parser->lex, Iris_ParserTKList(TK_COMMA));
  }
  Iris_ParserExpectAndConsume(parser->lex, Iris_ParserTKList(TK_BPAR_R));
}

static Iris_Node* Iris_ParserParseFnCallExpr(Iris_Parser* parser, Iris_Node* parent, Iris_Scope* scope)
{
  Iris_Token* token = Iris_ParserExpectAndConsume(parser->lex, Iris_ParserTKList(TK_IDENT));
  Iris_Node* node = Iris_ParserNewFnCallExpr(parser->arena, parent, token, null);
  Iris_ParserParseFnCallArgList(parser, scope, node);
  return node;
}

static Iris_Node* Iris_ParserParseStrctFnCallExpr(Iris_Parser* parser, Iris_Scope* scope, Iris_Node* parent, Iris_FnDefStmt* fn)
{
  Iris_Node* node = null;
  Iris_ParserExpectAndConsume(parser->lex, Iris_ParserTKList(TK_IDENT));  
  node = Iris_ParserNewFnCallExpr(parser->arena, parent, null, &fn->atom);
  Iris_ParserParseFnCallArgList(parser, scope, node);
  return node;
}

static Iris_Node* Iris_ParserParseStrctPropAccessExpr(Iris_Parser* parser, Iris_Node* parent, Iris_LitExpr* lit, Iris_Scope* scope)
{
  Iris_Token* token = null;
  Iris_Node* node = null;
  Iris_Node* child = null;
  Iris_StrctPropStmt* prop = null;

  if(Iris_LexerPickToken(parser->lex, 2)->type == TK_BPAR_L)
  {
    node = Iris_ParserNewStrctPropAccessExpr(parser->arena, parent, null, NODE_STRCT_FN_ACCESS);
    Iris_ParserExpectAndConsume(parser->lex, Iris_ParserTKList(TK_COLON));
    token = Iris_LexerPickToken(parser->lex, 0);
    prop = Iris_ParserGetStrctProp(&lit->strct.props, token->start, token->len);
    if(!prop) Iris_ParserReportNotFoundError(parser->lex, token);
    Iris_Cast(Iris_StrctPropAccessExpr*, node->as)->prop = Iris_ParserParseStrctFnCallExpr(parser, scope, parent, prop->val->as);
  }
  else if(Iris_LexerPickToken(parser->lex, 2)->type == TK_COLON)
  {
    node = Iris_ParserNewStrctPropAccessExpr(parser->arena, parent, null, NODE_STRCT_PROP_ACCESS);
    Iris_ParserExpectAndConsume(parser->lex, Iris_ParserTKList(TK_COLON));
    token = Iris_ParserExpectAndConsume(parser->lex, Iris_ParserTKList(TK_IDENT));
    prop = Iris_ParserGetStrctProp(&lit->strct.props, token->start, token->len);
    if(!prop) Iris_ParserReportNotFoundError(parser->lex, token);
    child = Iris_ParserParseStrctPropAccessExpr(parser, node, prop->val->as, scope);
    Iris_Cast(Iris_StrctPropAccessExpr*, node->as)->prop = child;
  }
  else
  {
    node = Iris_ParserNewStrctPropAccessExpr(parser->arena, parent, null, NODE_STRCT_PROP_ACCESS);
    Iris_ParserExpectAndConsume(parser->lex, Iris_ParserTKList(TK_COLON));
    token = Iris_ParserExpectAndConsume(parser->lex, Iris_ParserTKList(TK_IDENT));
    prop = Iris_ParserGetStrctProp(&lit->strct.props, token->start, token->len);
    if(!prop) Iris_ParserReportNotFoundError(parser->lex, token);
    Iris_Cast(Iris_StrctPropAccessExpr*, node->as)->prop = prop->val;
  }

  Iris_Cast(Iris_StrctPropAccessExpr*, node->as)->offset = prop->offset;

  return node;
}

static Iris_Node* Iris_ParserParseStrctPropAccessOrIdentExpr(Iris_Parser* parser, Iris_Node* parent, Iris_VarStmt* var, Iris_Scope* scope)
{
  Iris_Node* node = null;

  if(Iris_LexerPickToken(parser->lex, 0)->type == TK_COLON) node = Iris_ParserParseStrctPropAccessExpr(parser, parent, var->val->as, scope);
  else                                                      node = Iris_ParserNewIdentExpr(parser->arena, parent, var);

  return node;
}

static void_t Iris_ParserParseStrctLitExprProps(Iris_Parser* parser, Iris_Node* parent, Iris_Scope* scope, Iris_Node* lit)
{
  Iris_StrctDefStmt* strct = Iris_Cast(Iris_StrctDefStmt*, Iris_Cast(Iris_LitExpr*, lit->as)->strct.def);
  Iris_Token* token = null;
  Iris_Node* node = null;
  Iris_StrctDefStmt* child_strct = null;
  Iris_StrctPropStmt* prop_desc = null;
  Iris_StrctPropStmt prop = {0};

  // Collect all defined props.
  while((token = Iris_LexerPickToken(parser->lex, 0)) && token->type != TK_EOF && token->type != TK_BBRACES_R)
  {
    prop_desc = Iris_ParserGetStrctProp(&strct->props, token->start, token->len);
    if(!prop_desc) Iris_ParserReportNotFoundError(parser->lex, token);
    prop.atom = prop_desc->atom;
    prop.offset = prop_desc->offset;
    prop.type = prop_desc->type;
    prop.typename = prop_desc->typename;
    Iris_ParserExpectAndConsume(parser->lex, Iris_ParserTKList(TK_IDENT));
    Iris_ParserExpectAndConsume(parser->lex, Iris_ParserTKList(TK_COLON));
    
    if(Iris_LexerPickToken(parser->lex, 0)->type == TK_BBRACES_L)
    {
      node = Iris_ParserNewLitExpr(parser->arena, lit, NODE_STRCTLIT);
      // TODO: Extract into separate function.
      Iris_ArrayNew(Iris_Cast(Iris_LitExpr*, node->as)->strct.props, IRIS_LEXER_DEF_TOKENS_CAP);
      child_strct = Iris_ParserScopeGetStrctDef(scope, prop.typename->start, prop.typename->len);
      if(!child_strct) Iris_ParserReportNotFoundError(parser->lex, token);
      Iris_Cast(Iris_LitExpr*, node->as)->strct.def = (ptr_t)child_strct;
      Iris_ParserExpectAndConsume(parser->lex, Iris_ParserTKList(TK_BBRACES_L));
      Iris_ParserParseStrctLitExprProps(parser, lit, scope, node);
      Iris_ParserExpectAndConsume(parser->lex, Iris_ParserTKList(TK_BBRACES_R));
      prop.val = node;
    }
    else prop.val = Iris_ParserParseAssignExpr(parser, lit, scope);
    Iris_ArrayPush(Iris_Cast(Iris_LitExpr*, lit->as)->strct.props, prop);
  }

  // Collect all undefined props.
  for(i64_t i = 0; i < strct->props.len; i++)
  {
    prop_desc = &strct->props.buff[i];

    if(!Iris_ParserGetStrctProp(&Iris_Cast(Iris_LitExpr*, lit->as)->strct.props, prop_desc->atom.buff, prop_desc->atom.len))
    {
      prop.atom = prop_desc->atom;
      prop.offset = prop_desc->offset;
      prop.typename = prop_desc->typename;
      prop.type = prop_desc->type;
      
      if(prop.typename) prop.val = Iris_ParserGetDefaultVal(parser, parent, scope, prop.type, prop.typename); 
      else              prop.val = prop_desc->val;

      Iris_ArrayPush(Iris_Cast(Iris_LitExpr*, lit->as)->strct.props, prop);
    }
  }
}

static Iris_Node* Iris_ParserParseLit(Iris_Parser* parser, Iris_Node* parent, Iris_Scope* scope, Iris_NodeType type)
{
  Iris_Node* node = null;
  Iris_Token* token = null;
  Iris_LitExpr* lit_expr = null;
  char_t* str_num = null;
  Iris_StrctDefStmt* strct = null;

  node = Iris_ParserNewLitExpr(parser->arena, parent, type);
  lit_expr = node->as;
 
  if(type == NODE_STRCTLIT)
  {
    // TODO: Extract into separate function.
    token = Iris_ParserExpectAndPick(parser->lex, -2, Iris_ParserTKList(TK_IDENT));
    Iris_ArrayNew(lit_expr->strct.props, IRIS_LEXER_DEF_TOKENS_CAP);
    strct = Iris_ParserScopeGetStrctDef(scope, token->start, token->len);
    if(!strct) Iris_ParserReportNotFoundError(parser->lex, token);
    Iris_Cast(Iris_LitExpr*, node->as)->strct.def = (ptr_t)strct;
    Iris_ParserExpectAndConsume(parser->lex, Iris_ParserTKList(TK_BBRACES_L));
    Iris_ParserParseStrctLitExprProps(parser, parent, scope, node);
    Iris_ParserExpectAndConsume(parser->lex, Iris_ParserTKList(TK_BBRACES_R));
  }
  else if(type == NODE_BOOLLIT)
  {
    token = Iris_LexerPickToken(parser->lex, 0);
    lit_expr->i64 = Iris_StrnCmp(token->start, "true", 4);
    Iris_LexerConsumeToken(parser->lex);
  }
  else if(type == NODE_NUMLIT)
  {
    token = Iris_LexerPickToken(parser->lex, 0);
    // TODO: Implement custom strndup but for i16_t?
    str_num = strndup(token->start, (u64_t)token->len);
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
  Iris_Node* node = null;
  Iris_VarStmt* var = null;
  
  if(anything->type == TK_BBRACES_L)                                     node = Iris_ParserParseLit(parser, parent, scope, NODE_STRCTLIT);
  else if(anything->type == TK_STRINGLIT)                                node = Iris_ParserParseLit(parser, parent, scope, NODE_STRINGLIT);
  else if(anything->type == TK_NILLIT)                                   node = Iris_ParserParseLit(parser, parent, scope, NODE_NULLLIT);
  else if(anything->type == TK_INFINITYLIT)                              node = Iris_ParserParseLit(parser, parent, scope, NODE_INFINITYLIT);
  else if(anything->type == TK_NANLIT)                                   node = Iris_ParserParseLit(parser, parent, scope, NODE_NANLIT);
  else if(anything->type == TK_TRUELIT || anything->type == TK_FALSELIT) node = Iris_ParserParseLit(parser, parent, scope, NODE_BOOLLIT);
  else if(anything->type == TK_NUMLIT)                                   node = Iris_ParserParseLit(parser, parent, scope, NODE_NUMLIT);
  else if(Iris_ParserIsFnCallExpr(parser->lex, anything))                node = Iris_ParserParseFnCallExpr(parser, node, scope);
  else if(anything->type == TK_IDENT)
  {
    var = Iris_ParserScopeGetVar(scope, anything->start, anything->len);
    if(!var) Iris_ParserReportNotFoundError(parser->lex, anything);
    Iris_ParserExpectAndConsume(parser->lex, Iris_ParserTKList(TK_IDENT));
    node = Iris_ParserParseStrctPropAccessOrIdentExpr(parser, parent, var, scope);
  }
  else if(anything->type == TK_BPAR_L)
  {
    Iris_LexerConsumeToken(parser->lex);
    node = Iris_ParserParseAssignExpr(parser, node, scope);
    Iris_LexerConsumeToken(parser->lex);
  }
  else
  {
    Iris_ParserReportSyntaxError(parser->lex, "expected expression or statement", null, anything);
  }

  return node;
}

static Iris_Node* Iris_ParserParseUnaryExpr(Iris_Parser* parser, Iris_Node* parent, Iris_Scope* scope)
{
  Iris_Token* token = Iris_LexerPickToken(parser->lex, 0);
  Iris_Node* node = null;

  if(token->type == TK_MINUS || token->type == TK_NOT)
  {
    Iris_LexerConsumeToken(parser->lex);
    node = Iris_ParserNewUnaryExpr(parser->arena, parent, Iris_ParserTokenKindToOprKind(token->type), Iris_ParserParsePrimaryExpr(parser, parent, scope));
  }
  else
  {
    node = Iris_ParserParsePrimaryExpr(parser, parent, scope);
  }

  return node;
}

static Iris_Node* Iris_ParserParseStrongTermExpr(Iris_Parser* parser, Iris_Node* parent, Iris_Scope* scope)
{
  Iris_Node* ls = Iris_ParserParseUnaryExpr(parser, parent, scope);
  Iris_Node* rs = NULL;
  Iris_OprKind op = OPR_MUL;
  Iris_Token* token = NULL;

  while((token = Iris_LexerPickToken(parser->lex, 0)) && (token->type == TK_MUL || token->type == TK_DIV || token->type == TK_MOD)) 
  {
    op = Iris_ParserTokenKindToOprKind(token->type);
    Iris_LexerConsumeToken(parser->lex);
    rs = Iris_ParserParseUnaryExpr(parser, parent, scope);
    ls = Iris_ParserNewBinExpr(parser->arena, parent, ls, op, rs);
  }

  return ls;
}

static Iris_Node* Iris_ParserParseWeakTermExpr(Iris_Parser* parser, Iris_Node* parent, Iris_Scope* scope)
{
  Iris_Node* ls = Iris_ParserParseStrongTermExpr(parser, parent, scope);
  Iris_Node* rs = null;
  Iris_OprKind op = OPR_PLUS;
  Iris_Token* token = null;

  while((token = Iris_LexerPickToken(parser->lex, 0)) && (token->type == TK_PLUS || token->type == TK_MINUS)) 
  {
    op = Iris_ParserTokenKindToOprKind(token->type);
    Iris_LexerConsumeToken(parser->lex);
    rs = Iris_ParserParseStrongTermExpr(parser, parent, scope);
    ls = Iris_ParserNewBinExpr(parser->arena, parent, ls, op, rs);
  }

  return ls;
}

static Iris_Node* Iris_ParserParseBitExpr(Iris_Parser* parser, Iris_Node* parent, Iris_Scope* scope)
{
  Iris_Node* ls = Iris_ParserParseWeakTermExpr(parser, parent, scope);
  Iris_Node* rs = null;
  Iris_OprKind op = OPR_BIT_AND;
  Iris_Token* token = null;

  while((token = Iris_LexerPickToken(parser->lex, 0)) && 
        (token->type == TK_BIT_AND || token->type == TK_BIT_OR || token->type == TK_BIT_NOT ||
         token->type == TK_BIT_XOR || token->type == TK_BIT_LSHIFT || token->type == TK_BIT_RSHIFT))
  {
    op = Iris_ParserTokenKindToOprKind(token->type);
    Iris_LexerConsumeToken(parser->lex);
    rs = Iris_ParserParseWeakTermExpr(parser, parent, scope);
    ls = Iris_ParserNewBinExpr(parser->arena, parent, ls, op, rs);
  }

  return ls;
}

static Iris_Node* Iris_ParserParseCondExpr(Iris_Parser* parser, Iris_Node* parent, Iris_Scope* scope)
{
  Iris_Node* ls = Iris_ParserParseBitExpr(parser, parent, scope);
  Iris_Node* rs = null;
  Iris_OprKind op = OPR_EQ;
  Iris_Token* token = null;

  while((token = Iris_LexerPickToken(parser->lex, 0)) && 
        (token->type == TK_NOT_EQ || token->type == TK_EQ || token->type == TK_EQ_GT || token->type == TK_EQ_LT ||
         token->type == TK_GT || token->type == TK_LT))
  {
    op = Iris_ParserTokenKindToOprKind(token->type);
    Iris_LexerConsumeToken(parser->lex);
    rs = Iris_ParserParseBitExpr(parser, parent, scope);
    ls = Iris_ParserNewBinExpr(parser->arena, parent, ls, op, rs);
  }

  return ls;
}

static Iris_Node* Iris_ParserParseBindExpr(Iris_Parser* parser, Iris_Node* parent, Iris_Scope* scope)
{
  Iris_Node* ls = Iris_ParserParseCondExpr(parser, parent, scope);
  Iris_Node* rs = null;
  Iris_Token* token = null;
  Iris_OprKind op = OPR_AND;
  
  while((token = Iris_LexerPickToken(parser->lex, 0)) && (token->type == TK_AND || token->type == TK_OR)) 
  {
    op = Iris_ParserTokenKindToOprKind(token->type);
    Iris_LexerConsumeToken(parser->lex);
    rs = Iris_ParserParseCondExpr(parser, parent, scope);
    ls = Iris_ParserNewBinExpr(parser->arena, parent, ls, op, rs);
  }

  return ls;
}

Iris_Node* Iris_ParserParseAssignExpr(Iris_Parser* parser, Iris_Node* parent, Iris_Scope* scope)
{
  Iris_Node* ls = Iris_ParserParseBindExpr(parser, parent, scope);
  Iris_Node* rs = null;
  Iris_OprKind op = OPR_ASSIGN;
  Iris_Token* token = null;

  while((token = Iris_LexerPickToken(parser->lex, 0)) && token->type == TK_ASSIGN)
  {
    op = Iris_ParserTokenKindToOprKind(token->type);
    Iris_LexerConsumeToken(parser->lex);
    rs = Iris_ParserParseBindExpr(parser, parent, scope);
    ls = Iris_ParserNewBinExpr(parser->arena, parent, ls, op, rs);
  }

  return ls; 
}

static Iris_Node* Iris_ParserParseVarStmt(Iris_Parser* parser, Iris_Node* fn, Iris_Node* body)
{
  Iris_BodyStmt* body_stmt = fn ? Iris_Cast(Iris_FnDefStmt*, fn->as)->body->as : body->as;
  Iris_Token* token = null;
  Iris_Token* typename = null;
  Iris_Node* node = null;

  token = Iris_ParserExpectAndConsume(parser->lex, Iris_ParserTKList(TK_IDENT));

  if(Iris_ParserScopeGetVar(&Iris_Cast(Iris_BodyStmt*, body->as)->scope, token->start, token->len)) 
    Iris_ParserReportRedefinitionError(parser->lex, token);

  Iris_ParserExpectAndConsume(parser->lex, Iris_ParserTKList(TK_COLON));
  typename = Iris_ParserExpectAndConsume(parser->lex, Iris_ParserTKList(TK_TYPE, TK_IDENT));
  node = Iris_ParserNewVarStmt(parser->arena, body, token, typename, body_stmt->memsize);
  body_stmt->memsize += Iris_ParserGetSizeOfTypeKindOrTypename(
    Iris_Cast(Iris_VarStmt*, node->as)->typename, 
    Iris_Cast(Iris_VarStmt*, node->as)->type,
    &Iris_Cast(Iris_BodyStmt*, body->as)->scope);
  Iris_ParserExpectAndConsume(parser->lex, Iris_ParserTKList(TK_ASSIGN));
  Iris_Cast(Iris_VarStmt*, node->as)->val = Iris_ParserParseAssignExpr(parser, body, &Iris_Cast(Iris_BodyStmt*, body->as)->scope);
  Iris_ArrayPush(Iris_Cast(Iris_BodyStmt*, body->as)->scope.nodes, node);

  return node;
}

static Iris_Node* Iris_ParserParseRetStmt(Iris_Parser* parser, Iris_Node* fn, Iris_Node* body)
{
  return Iris_ParserNewRetStmt(parser->arena, body, Iris_ParserParseAssignExpr(parser, fn, &Iris_Cast(Iris_BodyStmt*, body->as)->scope));
}

static void_t Iris_ParserParseFnDefStmtArgs(Iris_Parser* parser, Iris_Node* fn)
{
  Iris_Node* arg = null;
  Iris_Token* token = null;
  Iris_Token* typename = null;
  Iris_BodyStmt* self_body_stmt = Iris_Cast(Iris_FnDefStmt*, fn->as)->body->as;

  Iris_ParserExpectAndConsume(parser->lex, Iris_ParserTKList(TK_BPAR_L));
  // TODO: Maybe remove token->type != TK_BPAR_R check.
  while((token = Iris_LexerPickToken(parser->lex, 0)) && token->type != TK_EOF && token->type != TK_BPAR_R)
  {
    Iris_Cast(Iris_FnDefStmt*, fn->as)->args_count++;
    Iris_ParserExpectAndConsume(parser->lex, Iris_ParserTKList(TK_IDENT));
    Iris_ParserExpectAndConsume(parser->lex, Iris_ParserTKList(TK_COLON));
    typename = Iris_ParserExpectAndConsume(parser->lex, Iris_ParserTKList(TK_TYPE, TK_IDENT));
    arg = Iris_ParserNewVarStmt(parser->arena, fn, token, typename, self_body_stmt->memsize);
    self_body_stmt->memsize += Iris_ParserGetSizeOfTypeKindOrTypename(
      Iris_Cast(Iris_VarStmt*, arg->as)->typename, 
      Iris_Cast(Iris_VarStmt*, arg->as)->type,
      &self_body_stmt->scope);
    Iris_ArrayPush(self_body_stmt->scope.nodes, arg);
    Iris_ArrayPush(self_body_stmt->nodes, arg);

    if(Iris_LexerPickToken(parser->lex, 0)->type == TK_COMMA)
    {
      Iris_ParserExpectAndConsume(parser->lex, Iris_ParserTKList(TK_COMMA));
      Iris_ParserExpectAndPick(parser->lex, 0, Iris_ParserTKList(TK_IDENT));
    }
    else
    {
      Iris_ParserExpectAndPick(parser->lex, 0, Iris_ParserTKList(TK_BPAR_R));
    }
  }

  Iris_ParserExpectAndConsume(parser->lex, Iris_ParserTKList(TK_BPAR_R));
  Iris_ParserExpectAndConsume(parser->lex, Iris_ParserTKList(TK_COLON));
  Iris_ParserExpectAndConsume(parser->lex, Iris_ParserTKList(TK_TYPE, TK_IDENT));
}

static void_t Iris_ParserParseFnDefStmtBody(Iris_Parser* parser, Iris_Node* fn)
{
  Iris_Token* token = null;
  Iris_Node* child = null;
  Iris_BodyStmt* self_body_stmt = Iris_Cast(Iris_FnDefStmt*, fn->as)->body->as;

  Iris_ParserExpectAndConsume(parser->lex, Iris_ParserTKList(TK_BBRACES_L));
  while((token = Iris_LexerPickToken(parser->lex, 0)) && token->type != TK_EOF && token->type != TK_BBRACES_R)
  {
    child = Iris_ParserParseStmtOrExpr(parser, fn, Iris_Cast(Iris_FnDefStmt*, fn->as)->body);
    Iris_ArrayPush(self_body_stmt->nodes, child);
  }
  Iris_ParserExpectAndConsume(parser->lex, Iris_ParserTKList(TK_BBRACES_R));
}

static Iris_Node* Iris_ParserParseFnDefStmt(Iris_Parser* parser, Iris_Atom* prefix, Iris_Node* root)
{
  Iris_Token* token = null;
  Iris_Node* node = null;

  token = Iris_ParserExpectAndConsume(parser->lex, Iris_ParserTKList(TK_IDENT));

  if(Iris_ParserScopeGetFnDef(&Iris_Cast(Iris_BodyStmt*, root->as)->scope, token->start, token->len)) 
    Iris_ParserReportRedefinitionError(parser->lex, token);
  
  Iris_ParserExpectAndConsume(parser->lex, Iris_ParserTKList(TK_COLON));
  node = Iris_ParserNewFnDefStmt(parser->arena, root, prefix, token, &Iris_Cast(Iris_BodyStmt*, root->as)->scope);
  Iris_Cast(Iris_BodyStmt*, Iris_Cast(Iris_FnDefStmt*, node->as)->body->as)->memsize += 32;
  Iris_ParserParseFnDefStmtArgs(parser, node);
  Iris_ParserParseFnDefStmtBody(parser, node);
  Iris_ArrayPush(Iris_Cast(Iris_BodyStmt*, root->as)->scope.nodes, node);
  
  return node;
}

static Iris_StrctPropStmt Iris_ParserParseStrctStmtProp(Iris_Parser* parser, Iris_Node* body, Iris_StrctDefStmt* strct)
{
  Iris_Token* token = Iris_ParserExpectAndPick(parser->lex, 0, Iris_ParserTKList(TK_IDENT));
  Iris_Node* node = null;
  Iris_StrctPropStmt prop = {0};

  if(Iris_ParserGetStrctProp(&strct->props, token->start, token->len)) 
    Iris_ParserReportRedefinitionError(parser->lex, token);
  
  prop.atom = Iris_ParserNewAtom(token);
  prop.offset = strct->size;
  prop.typename = null;
  prop.type = TP_I8;
  prop.val = null;

  if(Iris_ParserIsFnDef(parser->lex, token))
  {
    node = Iris_ParserParseFnDefStmt(parser, &strct->atom, body);
    Iris_ArrayPush(Iris_Cast(Iris_BodyStmt*, body->as)->nodes, node);
    Iris_ArrayPush(Iris_Cast(Iris_BodyStmt*, body->as)->scope.nodes, node);
    prop.val = node;
    prop.type = TP_PTR;
  }
  else
  {
    Iris_ParserExpectAndConsume(parser->lex, Iris_ParserTKList(TK_IDENT));
    Iris_ParserExpectAndConsume(parser->lex, Iris_ParserTKList(TK_COLON));
    token = Iris_ParserExpectAndConsume(parser->lex, Iris_ParserTKList(TK_TYPE, TK_IDENT));
    prop.typename = token;
    prop.type = Iris_ParserGetTypeKindFromTypename(token);
  }

  strct->size += Iris_ParserGetSizeOfTypeKindOrTypename(prop.typename, prop.type, &Iris_Cast(Iris_BodyStmt*, body->as)->scope);

  return prop;
}

static Iris_Node* Iris_ParserParseStrctStmt(Iris_Parser* parser, Iris_Node* body)
{
  Iris_Token* token = Iris_ParserExpectAndConsume(parser->lex, Iris_ParserTKList(TK_IDENT));
  Iris_Node* node = null;
  Iris_StrctPropStmt prop = {0};

  if(Iris_ParserScopeGetStrctDef(&Iris_Cast(Iris_BodyStmt*, body->as)->scope, token->start, token->len))
    Iris_ParserReportRedefinitionError(parser->lex, token);
  
  node = Iris_ParserNewStrctDefStmt(parser->arena, body, token);
  Iris_ParserExpectAndConsume(parser->lex, Iris_ParserTKList(TK_BBRACES_L));
  // TODO: Rewrite this in separate function/macro.
  while((token = Iris_LexerPickToken(parser->lex, 0)) && token->type != TK_BBRACES_R && token->type != TK_EOF)
  {
    prop = Iris_ParserParseStrctStmtProp(parser, body, node->as);
    Iris_ArrayPush(Iris_Cast(Iris_StrctDefStmt*, node->as)->props, prop);
  }
 
  Iris_ParserExpectAndConsume(parser->lex, Iris_ParserTKList(TK_BBRACES_R));
  Iris_ArrayPush(Iris_Cast(Iris_BodyStmt*, body->as)->scope.nodes, node);
  
  return node;
}

static Iris_Node* Iris_ParserParseIfStmt(Iris_Parser* parser, Iris_Node* fn, Iris_Node* body)
{
  Iris_Token* token = null;
  Iris_Node* node = null;
  Iris_Node* next_node = null;
  Iris_Node* expr = null;
  Iris_Node* child = null;
  Iris_NodeType kind = NODE_IF;

  node = Iris_ParserNewIfStmt(parser->arena, body, Iris_ParserParseAssignExpr(parser, fn, &Iris_Cast(Iris_BodyStmt*, body->as)->scope), NODE_IF, &Iris_Cast(Iris_BodyStmt*, body->as)->scope);
  Iris_ParserExpectAndConsume(parser->lex, Iris_ParserTKList(TK_BBRACES_L));
  
  while((token = Iris_LexerPickToken(parser->lex, 0)) && token->type != TK_BBRACES_R)
  {
    child = Iris_ParserParseStmtOrExpr(parser, fn, Iris_Cast(Iris_IfStmt*, node->as)->body);
    Iris_ArrayPush(Iris_Cast(Iris_BodyStmt*, Iris_Cast(Iris_IfStmt*, node->as)->body->as)->nodes, child);
  }
  
  Iris_ParserExpectAndConsume(parser->lex, Iris_ParserTKList(TK_BBRACES_R));
  
  while((token = Iris_LexerPickToken(parser->lex, 0)) && (token->type == TK_ELSE || token->type == TK_ELIF))
  {
    Iris_LexerConsumeToken(parser->lex);
    kind = token->type == TK_ELSE ? NODE_ELSE : NODE_ELIF;
    next_node = Iris_ParserNewIfStmt(parser->arena, body, null, kind, &Iris_Cast(Iris_BodyStmt*, body->as)->scope);
    expr = kind == NODE_ELSE ? null : Iris_ParserParseAssignExpr(parser, next_node, &Iris_Cast(Iris_BodyStmt*, body->as)->scope);
    Iris_Cast(Iris_IfStmt*, next_node->as)->cond = expr;
    Iris_ParserExpectAndConsume(parser->lex, Iris_ParserTKList(TK_BBRACES_L));

    while((token = Iris_LexerPickToken(parser->lex, 0)) && token->type != TK_BBRACES_R)
    {
      child = Iris_ParserParseStmtOrExpr(parser, fn, Iris_Cast(Iris_IfStmt*, next_node->as)->body);
      Iris_ArrayPush(Iris_Cast(Iris_BodyStmt*, Iris_Cast(Iris_IfStmt*, next_node->as)->body->as)->nodes, child);
    }
  
    Iris_ArrayPush(Iris_Cast(Iris_IfStmt*, node->as)->nextconds, next_node);
    Iris_ParserExpectAndConsume(parser->lex, Iris_ParserTKList(TK_BBRACES_R));
  }

  return node;
}

static Iris_Node* Iris_ParserParseForStmt(Iris_Parser* parser, Iris_Node* fn, Iris_Node* body)
{
  Iris_Token* token = null;
  Iris_Token* next = null;
  Iris_Node* node = null;
  Iris_Node* child = null;
  Iris_BodyStmt* self_body_stmt = null;

  node = Iris_ParserNewForStmt(parser->arena, body, &Iris_Cast(Iris_BodyStmt*, body->as)->scope);
  self_body_stmt = Iris_Cast(Iris_ForStmt*, node->as)->body->as;
  Iris_Cast(Iris_ForStmt*, node->as)->init = Iris_ParserParseStmtOrExpr(parser, fn, Iris_Cast(Iris_ForStmt*, node->as)->body);
  Iris_ParserExpectAndConsume(parser->lex, Iris_ParserTKList(TK_SEMI));
  Iris_Cast(Iris_ForStmt*, node->as)->cond = Iris_ParserParseAssignExpr(parser, node, &self_body_stmt->scope);
  Iris_ParserExpectAndConsume(parser->lex, Iris_ParserTKList(TK_SEMI));
  Iris_Cast(Iris_ForStmt*, node->as)->inc = Iris_ParserParseAssignExpr(parser, node, &self_body_stmt->scope);
  if(Iris_Cast(Iris_ForStmt*, node->as)->inc->type != NODE_BIN ||
     (Iris_Cast(Iris_ForStmt*, node->as)->inc->type == NODE_BIN &&
     Iris_Cast(Iris_BinExpr*, Iris_Cast(Iris_ForStmt*, node->as)->inc->as)->op != OPR_ASSIGN))
  {
    token = Iris_LexerPickToken(parser->lex, 0);
    next = Iris_LexerPickToken(parser->lex, 1);
    Iris_ReportSyntaxError(parser->lex->src.modulepath, "expected assign expression", null, next->start, next->len, token->row, token->col);

  }

  Iris_ParserExpectAndConsume(parser->lex, Iris_ParserTKList(TK_SEMI));
  Iris_ParserExpectAndConsume(parser->lex, Iris_ParserTKList(TK_BBRACES_L));
  
  while((token = Iris_LexerPickToken(parser->lex, 0)) && token->type != TK_BBRACES_R)
  {
    child = Iris_ParserParseStmtOrExpr(parser, fn, Iris_Cast(Iris_ForStmt*, node->as)->body);
    Iris_ArrayPush(self_body_stmt->nodes, child);
  }

  Iris_ParserExpectAndConsume(parser->lex, Iris_ParserTKList(TK_BBRACES_R));

  return node;
}

Iris_Node* Iris_ParserParseStmtOrExpr(Iris_Parser* parser, Iris_Node* fn, Iris_Node* body)
{
  Iris_Token* token = Iris_LexerPickToken(parser->lex, 0);

  if(token->type == TK_STRCT)
  {
    Iris_LexerConsumeToken(parser->lex);
    return Iris_ParserParseStrctStmt(parser, body);
  }
  else if(token->type == TK_VAR || token->type == TK_CONST)
  {
    Iris_LexerConsumeToken(parser->lex);
    return Iris_ParserParseVarStmt(parser, fn, body);
  }
  else if(token->type == TK_RET)
  {
    Iris_LexerConsumeToken(parser->lex);
    return Iris_ParserParseRetStmt(parser, fn, body);
  }
  else if(token->type == TK_IF || token->type == TK_ELSE || token->type == TK_ELSE)
  {
    Iris_LexerConsumeToken(parser->lex);
    return Iris_ParserParseIfStmt(parser, fn, body);
  }
  else if(token->type == TK_FOR)
  {
    Iris_LexerConsumeToken(parser->lex);
    return Iris_ParserParseForStmt(parser, fn, body);
  }
  else if(token->type == TK_IDENT)
  {
    if(Iris_ParserIsFnDef(parser->lex, token))
    {
      return Iris_ParserParseFnDefStmt(parser, null, body);
    }
    else
    {
      return Iris_ParserParseAssignExpr(parser, body, &Iris_Cast(Iris_BodyStmt*, body->as)->scope);
    }
  }
  else
  {
    return Iris_ParserParseAssignExpr(parser, body, &Iris_Cast(Iris_BodyStmt*, body->as)->scope);
  }
}

void_t Iris_ParserNew(Iris_Parser* parser, Iris_Lexer* lex)
{
  parser->lex = lex;
  Iris_ArenaNew(parser->arena, IRIS_PARSER_DEF_ARENA_SIZE);
  
  if(!parser->arena)
  {
    Iris_ReportInternError("could not allocate memory for parser arena");
  }
}

void_t Iris_ParserParse(Iris_Parser* parser)
{
  Iris_Node* body = Iris_ParserNewBodyStmt(parser->arena, null, null);
  Iris_Token* token = null;
  Iris_Node* child = null;

  while((token = Iris_LexerPickToken(parser->lex, 0)) && token->type != TK_EOF)
  {
    child = Iris_ParserParseStmtOrExpr(parser, null, body);
    if(child->type == NODE_VAR) Iris_Cast(Iris_VarStmt*, child->as)->is_global = true;
    Iris_ArrayPush(Iris_Cast(Iris_BodyStmt*, body->as)->nodes, child);
  }

  parser->root = body;
}

void_t Iris_ParserDump(const Iris_Node* node)
{
  const i32_t depth_step = 2;
  static i32_t depth = 0;
 
  printf(IRIS_ANSI_DIM "%*s%s" IRIS_ANSI_RES ": ", depth, "", Iris_ParserNodeTypeToString(node->type));

  switch(node->type)
  {
    case NODE_FN_DEF:
    {
      Iris_FnDefStmt* stmt = node->as;
      Iris_BodyStmt* body = stmt->body->as;
      printf("%.*s(argc: %li blksz: %li scpsz: %li)\n", (i32_t)stmt->atom.len, stmt->atom.buff, stmt->args_count, body->nodes.len, body->scope.nodes.len);
      depth += depth_step;
      Iris_ParserDump(stmt->body);
      depth -= depth_step;
    }
      break;
    case NODE_FN_CALL:
    {
      Iris_FnCallExpr* expr = node->as;
      printf("%.*s\n", (i32_t)expr->atom.len, expr->atom.buff);
      depth += depth_step;
      for(i64_t i = 0; i < expr->nodes.len; i++) Iris_ParserDump(expr->nodes.buff[i]);
      depth -= depth_step;
    }
      break;
    case NODE_STRCT_DEF:
    {
      Iris_StrctDefStmt* stmt = node->as;
      printf("%.*s\n", (i32_t)stmt->atom.len, stmt->atom.buff);
    }
      break;
    case NODE_STRCT_PROP_ACCESS:
    {
      Iris_StrctPropAccessExpr* expr = node->as;
      printf("[%li]\n", expr->offset);
      depth += depth_step;
      Iris_ParserDump(expr->prop);
      depth -= depth_step;
    }
      break;
    case NODE_STRCT_FN_ACCESS:
    {
      Iris_StrctPropAccessExpr* expr = node->as;
      printf("[%li]\n", expr->offset);
      depth += depth_step;
      Iris_ParserDump(expr->prop);
      depth -= depth_step; 
    }
      break;
    case NODE_BODY:
    {
      Iris_BodyStmt* stmt = node->as;
      printf("%li\n", stmt->nodes.len);
      depth += depth_step;
      for(i64_t i = 0; i < stmt->nodes.len; i++) Iris_ParserDump(stmt->nodes.buff[i]);
      depth -= depth_step;
    }
      break;
    case NODE_VAR:
    {
      Iris_VarStmt* stmt = node->as;
      printf("%.*s[%li]\n", (i32_t)stmt->atom.len, stmt->atom.buff, stmt->offset);

      if(stmt->val)
      {
        depth += depth_step;
        Iris_ParserDump(stmt->val);
        depth -= depth_step;
      }
    }
      break;
    case NODE_RET:
    {
      Iris_RetStmt* stmt = node->as;
      printf("\n");
      depth += depth_step;
      Iris_ParserDump(stmt->val);
      depth -= depth_step;
    }
      break;
    case NODE_IF:
    {
      Iris_IfStmt* stmt = node->as;
      printf("\n", null);
      depth += depth_step;
      Iris_ParserDump(stmt->cond);
      depth -= depth_step;
      depth += depth_step;
      Iris_ParserDump(stmt->body);
      depth -= depth_step;
      for(i64_t i = 0; i < stmt->nextconds.len; i++) Iris_ParserDump(((Iris_Node*)stmt->nextconds.buff[i]));
    }
      break;
    case NODE_ELSE:
    {
      Iris_IfStmt* stmt = node->as;
      printf("\n", null);
      depth += depth_step;
      Iris_ParserDump(stmt->body);
      depth -= depth_step;
    }
      break;
    case NODE_ELIF:
    {
      Iris_IfStmt* stmt = node->as;
      printf("\n", null);
      depth += depth_step;
      Iris_ParserDump(stmt->cond);
      depth -= depth_step;
      depth += depth_step;
      Iris_ParserDump(stmt->body);
      depth -= depth_step;
    }
      break;
    case NODE_FOR:
    {
      Iris_ForStmt* stmt = node->as;
      printf("\n", null);
      depth += depth_step;
      Iris_ParserDump(stmt->init);
      depth -= depth_step;
      depth += depth_step;
      Iris_ParserDump(stmt->cond);
      depth -= depth_step;
      depth += depth_step;
      Iris_ParserDump(stmt->inc);
      depth -= depth_step;
      depth += depth_step;
      Iris_ParserDump(stmt->body);
      depth -= depth_step;
    }
      break;
    case NODE_BIN:
    {
      Iris_BinExpr* expr = node->as;
      printf("%s\n", Iris_ParserOprtTypeToString(expr->op));
      depth += depth_step;
      Iris_ParserDump(expr->ls);
      depth -= depth_step;
      depth += depth_step;
      Iris_ParserDump(expr->rs);
      depth -= depth_step;
    }
      break;
    case NODE_UNARY:
    {
      Iris_UnaryExpr* expr = node->as;
      printf("%s\n", Iris_ParserOprtTypeToString(expr->op));
      depth += depth_step;
      Iris_ParserDump(expr->operand);
      depth -= depth_step;
    }
      break;
    case NODE_IDENT:
    {
      Iris_IdentExpr* expr = node->as;
      Iris_VarStmt* var = Iris_Cast(Iris_VarStmt*, expr->def);
      printf("%.*s[%li]\n", (i32_t)var->atom.len, var->atom.buff, var->offset);
    }
      break;
    case NODE_NUMLIT:
    {
      Iris_LitExpr* expr = node->as;
      printf("%.12f\n", expr->f64);
    }
      break;
    case NODE_BOOLLIT:
    {
      Iris_LitExpr* expr = node->as;
      printf("%s\n", expr->i64 ? "true" : "false");
    }
      break;
    case NODE_NULLLIT:
      printf("nil\n", null);
      break;
    case NODE_NANLIT:
      printf("nan\n", null);
      break;
    case NODE_INFINITYLIT:
      printf("infinity\n", null);
      break;
    case NODE_STRCTLIT:
    {
      Iris_StrctLitExpr* expr = node->as;
      Iris_StrctDefStmt* strct = Iris_Cast(Iris_StrctDefStmt*, expr->def);
      printf("%.*s[%li][%li]\n", (i32_t)strct->atom.len, strct->atom.buff, expr->props.len, strct->size);
      depth += depth_step;
      for(i64_t i = 0; i < expr->props.len; i++) Iris_ParserDump(expr->props.buff[i].val);
      depth -= depth_step;
    }
      break;
    case NODE_STRCT_PROP: break;
    case NODE_STRINGLIT:  break;
    default: { /* Skip */ }
  }
}

const i8_t* Iris_ParserNodeTypeToString(Iris_NodeType type)
{
  switch(type) 
  {
    case NODE_BODY:              return "Body";
    case NODE_BIN:               return "Bin";
    case NODE_UNARY:             return "Unary"; 
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
    case NODE_STRCT_FN_ACCESS:   return "StrctFnAccess";
    case NODE_STRCT_PROP:        return "StrctProp";
    default:
      return "UnknownNode";
  }
}

const char_t* Iris_ParserOprtTypeToString(Iris_OprKind kind)
{
  switch(kind)
  {
    case OPR_PLUS:        return "+";
    case OPR_MINUS:       return "-";
    case OPR_MUL:         return "*";
    case OPR_DIV:         return "/";
    case OPR_MOD:         return "%";
    case OPR_ASSIGN:      return "=";
    case OPR_EQ:          return "==";
    case OPR_NOT_EQ:      return "!=";
    case OPR_EQ_GT:       return ">=";
    case OPR_EQ_LT:       return "<=";
    case OPR_GT:          return ">";
    case OPR_LT:          return "<";
    case OPR_NOT:         return "!";
    case OPR_AND:         return "&&";
    case OPR_OR:          return "||";
    case OPR_BIT_AND:     return "&";
    case OPR_BIT_OR:      return "|";
    case OPR_BIT_NOT:     return "~";
    case OPR_BIT_XOR:     return "^";
    case OPR_BIT_LSHIFT:  return "<<";
    case OPR_BIT_RSHIFT:  return ">>";
    default:
      return "UnknownOpr";
  }
}
