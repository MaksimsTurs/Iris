#include "iris/bytecode.h"
#include "iris/opcodes.h"
#include "iris/parser.h"
#include "iris/typedef.h"
#include "iris/utils.h"

#define Iris_BytecodeHasRetStmt($body) (($body->nodes.len > 0) && ($body->nodes.buff[body->nodes.len - 1]->type == NODE_RET))
#define Iris_BytecodeIsPrimitiveNode($node) (($node)->type == NODE_NUMLIT || ($node)->type == NODE_BOOLLIT || ($node)->type == NODE_NULLLIT || ($node)->type == NODE_INFINITYLIT)
#define Iris_BytecodeIsLit($node) (($node)->type == NODE_NULLLIT || ($node)->type == NODE_NANLIT || ($node)->type == NODE_INFINITYLIT || ($node)->type == NODE_BOOLLIT || ($node)->type == NODE_NUMLIT || ($node)->type == NODE_STRINGLIT || ($node)->type == NODE_STRCTLIT)
#define Iris_BytecodeIsArg($var) (!($var)->val)

void_t Iris_BytecodeGenerateStmtOrExpr(Iris_Bytecode* bytecode, Iris_Node* expr);

static Iris_Opcode Iris_BytecodeOprTypeToOpcode(Iris_OprKind type)
{
  if(type == OPR_PLUS)        return OPC_ADD;
  if(type == OPR_MINUS)       return OPC_SUB;
  if(type == OPR_MUL)         return OPC_MUL;
  if(type == OPR_DIV)         return OPC_DIV;
  if(type == OPR_MOD)         return OPC_MOD;
  if(type == OPR_EQ)          return OPC_CMPEQ;
  if(type == OPR_NOT_EQ)      return OPC_CMPNEQ;
  if(type == OPR_GT)          return OPC_CMPGT;
  if(type == OPR_LT)          return OPC_CMPLT;
  if(type == OPR_EQ_GT)       return OPC_CMPEGT;
  if(type == OPR_EQ_LT)       return OPC_CMPELT;
  if(type == OPR_OR)          return OPC_OR;
  if(type == OPR_AND)         return OPC_AND;
  if(type == OPR_NOT)         return OPC_NOT;
  if(type == OPR_BIT_OR)      return OPC_BIT_OR;
  if(type == OPR_BIT_XOR)     return OPC_BIT_XOR;
  if(type == OPR_BIT_AND)     return OPC_BIT_AND;
  if(type == OPR_BIT_NOT)     return OPC_BIT_NOT;
  if(type == OPR_BIT_LSHIFT)  return OPC_BIT_LSHIFT;
  if(type == OPR_BIT_RSHIFT)  return OPC_BIT_RSHIFT;
  assert(false && "Unknown operator!");
}

static const char_t* Iris_BytecodeTypeToString(Iris_TypeKind type)
{
  switch(type)
  {
    case TP_BOOL:  return "Bool";
    case TP_CHAR:  return "Char";
    case TP_I8:    return "Int8";
    case TP_U8:    return "Uint8";
    case TP_I16:   return "Int16";
    case TP_U16:   return "Uint16";
    case TP_I32:   return "Int32";
    case TP_U32:   return "Uint32";
    case TP_I64:   return "Int64";
    case TP_U64:   return "Uint64";
    case TP_F32:   return "Float32";
    case TP_F64:   return "Float64";
    case TP_INF:   return "Inf";
    case TP_NAN:   return "Nan";
    case TP_NIL:   return "Nil";
    case TP_VOID:  return "Void";
    case TP_PTR:   return "Ptr";
    case TP_STRCT: return "Strct";
    default: assert(false && "Unknown type!");
  }
}

static const i8_t* Iris_BytecodeOpcToString(Iris_Opcode opcode)
{
  switch(opcode) 
  {
    case OPC_FN:          return "Fn";
    case OPC_FN_CALL:     return "Call";
    case OPC_FN_RETVOID:  return "FnRet[Void]";
    case OPC_FN_RETVAL:   return "FnRet[Val]";
    case OPC_LOADLT:      return "Load[LT]";
    case OPC_LOADLC:      return "Load[LC]";
    case OPC_LOADGC:      return "Load[GC]";
    case OPC_MOVLC:       return "Mov[LC]";
    case OPC_MOVGC:       return "Mov[GC]";
    case OPC_JMP:         return "Jmp";
    case OPC_JMPIF:       return "JmpIf";
    case OPC_JMPNIF:      return "JmpNIf";
    case OPC_CMPEQ:       return "CmpEq";
    case OPC_CMPNEQ:      return "CmpNeq";
    case OPC_CMPGT:       return "CmpGt";
    case OPC_CMPLT:       return "CmpLt";
    case OPC_CMPEGT:      return "CmpEGt";
    case OPC_CMPELT:      return "CmpELt";
    case OPC_ADD:         return "Add";
    case OPC_SUB:         return "Sub";
    case OPC_MUL:         return "Mul";
    case OPC_DIV:         return "Div";
    case OPC_MOD:         return "Mod";
    case OPC_OR:          return "Or";
    case OPC_AND:         return "And";
    case OPC_NOT:         return "Not";
    case OPC_BIT_AND:     return "BitAnd";
    case OPC_BIT_NOT:     return "BitNot";
    case OPC_BIT_OR:      return "BitOr";
    case OPC_BIT_XOR:     return "BitXor";
    case OPC_BIT_LSHIFT:  return "BitLShift";
    case OPC_BIT_RSHIFT:  return "BitRShift";
    default:
      return "UnknownOpcode";
  }
}

static void_t Iris_BytecodeFindBottom(Iris_Node* node, Iris_NodeDepth* deepest, i64_t parent_depth)
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
    bin_expr = node->as;
    if(bin_expr->ls->type == NODE_BIN) Iris_BytecodeFindBottom(bin_expr->ls, deepest, curr_node.depth);
    if(bin_expr->rs->type == NODE_BIN) Iris_BytecodeFindBottom(bin_expr->rs, deepest, curr_node.depth);
  }
  else if(node->type == NODE_FN_CALL)
  {
    fn_call_expr = node->as;
    for(i64_t i = 0; i < fn_call_expr->nodes.len; i++)
      if(fn_call_expr->nodes.buff[i]->type == NODE_BIN) Iris_BytecodeFindBottom(fn_call_expr->nodes.buff[i], deepest, curr_node.depth);
  }
  else if(node->type == NODE_VAR)
  {
    var_stmt = node->as;
    if(var_stmt->val && var_stmt->val->type == NODE_BIN) Iris_BytecodeFindBottom(var_stmt->val, deepest, curr_node.depth);
  }
}

static Iris_Node* Iris_BytecodePrepareBinExpr(Iris_Node* top)
{
  Iris_NodeDepth deepest = {0};
  Iris_NodeDepth node_depth = {0};

  node_depth.node = top;
  node_depth.depth = 0;
  deepest.depth = 0;
  deepest.node = top;
  Iris_BytecodeFindBottom(top, &deepest, 0);

  return deepest.node;
}

static void_t Iris_JMPAddEntry(Iris_JMPEntryArray* jmps, Iris_JMPEntry* entry)
{
  if(jmps->len >= jmps->cap) Iris_ArrayExpand(*jmps, jmps->cap * 2);
  if(!jmps->buff) Iris_ReportInternError("could not expand jmps table buffer");
  Iris_ArrayPush(*jmps, *entry);
}

static Iris_JMPEntry* Iris_JMPGetFnEntry(Iris_JMPEntryArray* jmps, const Iris_Atom* name)
{
  Iris_JMPEntry* entry = null;

  for(i64_t i = 0; i < jmps->len; i++)
  {
    entry = Iris_ArrayAt(*jmps, i);
    if(Iris_StrnCmp(name->buff, entry->fn->atom.buff, Iris_Max(name->len, entry->fn->atom.len))) return entry;
  }

  return null;
}

static void_t Iris_BytecodeWriteOpcode(Iris_Bytecode* bytecode, Iris_Opcode opcode)
{
  if(bytecode->len >= bytecode->cap) Iris_ArrayExpand(*bytecode, bytecode->cap * 2);
  if(!bytecode->buff) Iris_ReportInternError("could not expand code buffer");

  *Iris_Cast(Iris_Opcode*, (bytecode->buff + bytecode->len)) = opcode;
  bytecode->len += sizeof(Iris_Opcode);
}

static void_t Iris_BytecodeWriteType(Iris_Bytecode* bytecode, Iris_TypeKind type)
{
  if(bytecode->len >= bytecode->cap) Iris_ArrayExpand(*bytecode, bytecode->cap * 2);
  if(!bytecode->buff) Iris_ReportInternError("could not expand code buffer");
  *Iris_Cast(Iris_TypeKind*, (bytecode->buff + bytecode->len)) = type;
  bytecode->len += sizeof(Iris_TypeKind);
}

static void_t Iris_BytecodeWriteInt64(Iris_Bytecode* bytecode, i64_t num)
{
  if(bytecode->len >= bytecode->cap) Iris_ArrayExpand(*bytecode, bytecode->cap * 2);
  if(!bytecode->buff) Iris_ReportInternError("could not expand code buffer");
  *Iris_Cast(i64_t*, (bytecode->buff + bytecode->len)) = num;
  bytecode->len += sizeof(num);
}

static void_t Iris_BytecodeWriteFloat64(Iris_Bytecode* bytecode, f64_t num)
{
  if(bytecode->len >= bytecode->cap) Iris_ArrayExpand(*bytecode, bytecode->cap * 2);
  if(!bytecode->buff) Iris_ReportInternError("could not expand code buffer");
  *Iris_Cast(f64_t*, (bytecode->buff + bytecode->len)) = num;
  bytecode->len += sizeof(num);
}

static void_t Iris_BytecodePatchInt64(Iris_Bytecode* bytecode, i64_t offset, i64_t num)
{
  if(offset > bytecode->len) Iris_ReportInternError("can not path \"Int64\" on offset \"%li\", code len is \"%li\"", offset, bytecode->len);
  *Iris_Cast(i64_t*, (bytecode->buff + offset)) = num;
}

// TODO: Implement this.
// static void_t Iris_BytecodeGenerateStrctPropAccessExpr(Iris_Bytecode* bytecode, const Iris_Node* node) {}

// TODO: Implement this.
// static void Iris_BytecodeGenerateStrctLitExpr(Iris_Bytecode* bytecode, const Iris_Node* node, i64_t base_offset, Iris_Opcode opcode) {}

static void_t Iris_BytecodeGenerateBodyStmt(Iris_Bytecode* bytecode, Iris_BodyStmt* body)
{
  for(i64_t i = 0; i < body->nodes.len; i++) Iris_BytecodeGenerateStmtOrExpr(bytecode, body->nodes.buff[i]);
}

static void Iris_BytecodeGenerateFnCallExpr(Iris_Bytecode* bytecode, const Iris_FnCallExpr* expr)
{
  Iris_JMPEntry* jmp_entry = null;

  for(i64_t i = expr->nodes.len - 1; i >= 0; i--) Iris_BytecodeGenerateStmtOrExpr(bytecode, expr->nodes.buff[i]);
  
  jmp_entry = Iris_JMPGetFnEntry(&bytecode->jmps, &expr->atom);
  Iris_BytecodeWriteOpcode(bytecode, OPC_FN_CALL);
  Iris_BytecodeWriteInt64(bytecode, jmp_entry->start_offset);
}

static void_t Iris_BytecodeGeneratePrimitiveLitExpr(Iris_Bytecode* bytecode, Iris_LitExpr* expr, Iris_TypeKind type)
{
  if(type == TP_F64) Iris_BytecodeWriteFloat64(bytecode, expr->f64);
  // TODO: Maybe separate TK_NUMBER into TK_NUMFLOAT, TK_NUMINT?
  if(type == TP_I64) Iris_BytecodeWriteInt64(bytecode, (i64_t)expr->f64);
}

static void_t Iris_BytecodeGenerateBinExprNode(Iris_Bytecode* bytecode, Iris_Node* node)
{
  if(node->type == NODE_BIN) Iris_BytecodeGenerateStmtOrExpr(bytecode, Iris_BytecodePrepareBinExpr(node));
  else                       Iris_BytecodeGenerateStmtOrExpr(bytecode, node);
}

static void_t Iris_BytecodeGenerateBinExpr(Iris_Bytecode* bytecode, Iris_Node* expr)
{
  Iris_BinExpr* bin_expr = expr->as;
  Iris_VarStmt* var_stmt = null;

  if(bin_expr->op == OPR_ASSIGN)
  {
    Iris_BytecodeGenerateBinExprNode(bytecode, bin_expr->rs);
    var_stmt = Iris_Cast(Iris_VarStmt*, Iris_Cast(Iris_IdentExpr*, bin_expr->ls->as)->def);
    Iris_BytecodeWriteOpcode(bytecode, var_stmt->is_global ? OPC_MOVGC : OPC_MOVLC);
    Iris_BytecodeWriteType(bytecode, var_stmt->type);
    Iris_BytecodeWriteInt64(bytecode, var_stmt->offset);
  }
  else
  {
    Iris_BytecodeGenerateBinExprNode(bytecode, bin_expr->ls);
    Iris_BytecodeGenerateBinExprNode(bytecode, bin_expr->rs);
    Iris_BytecodeWriteOpcode(bytecode, Iris_BytecodeOprTypeToOpcode(bin_expr->op));
  }
  
  if(expr->parent) Iris_BytecodeGenerateStmtOrExpr(bytecode, expr->parent);
}

// TODO: Add strct prop access and prop call.
static void_t Iris_BytecodeGenerateVarStmt(Iris_Bytecode* bytecode, Iris_Node* node)
{
  Iris_VarStmt* stmt = node->as;
  Iris_VarStmt* val_var = null;

  if(Iris_BytecodeIsLit(stmt->val))
  {
    Iris_BytecodeWriteOpcode(bytecode, OPC_LOADLT);
    Iris_BytecodeWriteType(bytecode, stmt->type);
    Iris_BytecodeGeneratePrimitiveLitExpr(bytecode, stmt->val->as, stmt->type);
  }
  else if(stmt->val->type == NODE_IDENT)
  {
    val_var = Iris_Cast(Iris_VarStmt*, Iris_Cast(Iris_IdentExpr*, stmt->val->as)->def);
    Iris_BytecodeWriteOpcode(bytecode, val_var->is_global ? OPC_LOADGC : OPC_LOADLC);
    Iris_BytecodeWriteType(bytecode, val_var->type);
    Iris_BytecodeWriteInt64(bytecode, val_var->offset);
  }
  else
  {
    Iris_BytecodeGenerateStmtOrExpr(bytecode, stmt->val);
  }

  Iris_BytecodeWriteOpcode(bytecode, stmt->is_global ? OPC_MOVGC : OPC_MOVLC);
  Iris_BytecodeWriteType(bytecode, stmt->type);
  Iris_BytecodeWriteInt64(bytecode, stmt->offset);
}

static void_t Iris_BytecodeGenerateIfStmt(Iris_Bytecode* bytecode, const Iris_IfStmt* stmt)
{
  static i64_t patch_offsets[32] = {0};
  i64_t patch_len = 0;
  i64_t apatch = 0;
  i64_t bpatch = 0;
  Iris_IfStmt* elsif = null;

  Iris_BytecodeGenerateStmtOrExpr(bytecode, stmt->cond);
  Iris_BytecodeWriteOpcode(bytecode, OPC_JMPNIF);
  apatch = bytecode->len;
  Iris_BytecodeWriteInt64(bytecode, 0);
  Iris_BytecodeGenerateBodyStmt(bytecode, stmt->body->as);
  Iris_BytecodeWriteOpcode(bytecode, OPC_JMP);
  patch_offsets[patch_len++] = bytecode->len;
  Iris_BytecodeWriteInt64(bytecode, 0);
  Iris_BytecodePatchInt64(bytecode, apatch, bytecode->len);

  for(i64_t i = 0; i < stmt->nextconds.len; i++)
  {
    elsif = stmt->nextconds.buff[i]->as;

    if(stmt->nextconds.buff[i]->type == NODE_ELSE)
    {
      Iris_BytecodeWriteOpcode(bytecode, OPC_JMP);
      bpatch = bytecode->len;
      Iris_BytecodeWriteInt64(bytecode, 0);
      Iris_BytecodePatchInt64(bytecode, apatch, bytecode->len);
      Iris_BytecodeGenerateBodyStmt(bytecode, elsif->body->as);
      Iris_BytecodePatchInt64(bytecode, bpatch, bytecode->len);
    }
    else
    {
      Iris_BytecodeGenerateStmtOrExpr(bytecode, elsif->cond);
      Iris_BytecodeWriteOpcode(bytecode, OPC_JMPNIF);
      apatch = bytecode->len;
      Iris_BytecodeWriteInt64(bytecode, 0);
      Iris_BytecodeGenerateBodyStmt(bytecode, elsif->body->as);
      Iris_BytecodeWriteOpcode(bytecode, OPC_JMP);
      patch_offsets[patch_len++] = bytecode->len;
      Iris_BytecodeWriteInt64(bytecode, 0);
      Iris_BytecodePatchInt64(bytecode, apatch, bytecode->len);
    }
  }

  for(i64_t i = 0; i < patch_len; i++)
  {
    Iris_BytecodePatchInt64(bytecode, patch_offsets[i], bytecode->len);
  }
}

static void_t Iris_BytecodeGenerateForStmt(Iris_Bytecode* bytecode, const Iris_ForStmt* stmt)
{
  i64_t expr_offset = 0;
  i64_t exit_jmp_offset = 0;

  // Init section
  Iris_BytecodeGenerateStmtOrExpr(bytecode, stmt->init);
  expr_offset = bytecode->len;
  // Expression section
  Iris_BytecodeGenerateStmtOrExpr(bytecode, stmt->cond);
  Iris_BytecodeWriteOpcode(bytecode, OPC_JMPNIF);
  exit_jmp_offset = bytecode->len;
  Iris_BytecodeWriteInt64(bytecode, 0);
  // Body and Increment section
  Iris_BytecodeGenerateBodyStmt(bytecode, stmt->body->as);
  Iris_BytecodeGenerateStmtOrExpr(bytecode, stmt->inc);
  Iris_BytecodeWriteOpcode(bytecode, OPC_JMP);
  Iris_BytecodeWriteInt64(bytecode, expr_offset);
  Iris_BytecodePatchInt64(bytecode, exit_jmp_offset, bytecode->len);
}

static void_t Iris_BytecodeGenerateRetStmt(Iris_Bytecode* bytecode, const Iris_RetStmt* stmt)
{
  if(stmt->val) Iris_BytecodeGenerateStmtOrExpr(bytecode, stmt->val);
  Iris_BytecodeWriteOpcode(bytecode, OPC_FN_RETVOID);
}

void_t Iris_BytecodeGenerateStmtOrExpr(Iris_Bytecode* bytecode, Iris_Node* stmt_or_expr)
{
  Iris_VarStmt* val_var = null;

  if(stmt_or_expr->is_visited) return;

  stmt_or_expr->is_visited = true;

  if(stmt_or_expr->type == NODE_IF) Iris_BytecodeGenerateIfStmt(bytecode, stmt_or_expr->as);
  else if(stmt_or_expr->type == NODE_FOR) Iris_BytecodeGenerateForStmt(bytecode, stmt_or_expr->as);
  else if(stmt_or_expr->type == NODE_FN_CALL) Iris_BytecodeGenerateFnCallExpr(bytecode, stmt_or_expr->as);
  else if(stmt_or_expr->type == NODE_RET) Iris_BytecodeGenerateRetStmt(bytecode, stmt_or_expr->as);
  else if(stmt_or_expr->type == NODE_BIN) Iris_BytecodeGenerateBinExpr(bytecode, stmt_or_expr);
  else if(stmt_or_expr->type == NODE_VAR && !Iris_BytecodeIsArg(Iris_Cast(Iris_VarStmt*, stmt_or_expr->as)))
  {
    Iris_BytecodeGenerateVarStmt(bytecode, stmt_or_expr); 
  }
  else if(Iris_BytecodeIsPrimitiveNode(stmt_or_expr))
  {
    Iris_BytecodeWriteOpcode(bytecode, OPC_LOADLT);
    Iris_BytecodeWriteType(bytecode, TP_I64);
    Iris_BytecodeGeneratePrimitiveLitExpr(bytecode, stmt_or_expr->as, TP_I64);
  }
  else if(stmt_or_expr->type == NODE_IDENT)
  {
    val_var = Iris_Cast(Iris_VarStmt*, Iris_Cast(Iris_IdentExpr*, stmt_or_expr->as)->def);
    Iris_BytecodeWriteOpcode(bytecode, val_var->is_global ? OPC_LOADGC : OPC_LOADLC);
    Iris_BytecodeWriteType(bytecode, val_var->type);
    Iris_BytecodeWriteInt64(bytecode, val_var->offset);
  }
}

static void_t Iris_BytecodeGenerateFnPrologue(Iris_Bytecode* bytecode, Iris_Node* fn)
{
  Iris_FnDefStmt* stmt = fn->as;
  Iris_BodyStmt* body = stmt->body->as;
  Iris_VarStmt* arg = null;
  Iris_JMPEntry entry = {0};
 
  entry.fn = stmt;
  entry.start_offset = bytecode->len;
  Iris_JMPAddEntry(&bytecode->jmps, &entry);
  Iris_BytecodeWriteOpcode(bytecode, OPC_FN);
  Iris_BytecodeWriteInt64(bytecode, body->memsize);

  if(Iris_StrnCmp(stmt->atom.buff, "main", Iris_Max(stmt->atom.len, 4))) bytecode->main_offset = entry.start_offset;

  for(i64_t i = 0; i < stmt->args_count; i++)
  {
    arg = body->nodes.buff[i]->as;
    Iris_BytecodeWriteOpcode(bytecode, OPC_MOVLC);
    Iris_BytecodeWriteType(bytecode, arg->type);
    Iris_BytecodeWriteInt64(bytecode, arg->offset);
  }
}

static void_t Iris_BytecodeGenerateFnEpilogue(Iris_Bytecode* bytecode, Iris_Node* node)
{
  Iris_FnDefStmt* stmt = node->as;
  Iris_BodyStmt* body = stmt->body->as;
  if(!Iris_BytecodeHasRetStmt(body)) Iris_BytecodeWriteOpcode(bytecode, OPC_FN_RETVOID);
}
 
void_t Iris_BytecodeNew(Iris_Bytecode* bytecode, Iris_Parser* parser)
{
  bytecode->root = parser->root;
  Iris_ArrayNew(*bytecode, IRIS_BYTECODE_DEF_CODE_CAP);
  if(!bytecode->buff) Iris_ReportInternError("could not allocate buffer for code");
  Iris_ArrayNew(bytecode->jmps, IRIS_BYTECODE_DEF_JMP_ENTRIES_CAP);
  if(!bytecode->jmps.buff) Iris_ReportInternError("could not allocate buffer for jump table");
}

void_t Iris_BytecodeGenerate(Iris_Bytecode* bytecode)
{
  Iris_BodyStmt* global = bytecode->root->as;

  for(i64_t i = 0; i < global->nodes.len; i++)
  {
    if(global->nodes.buff[i]->type == NODE_FN_DEF)
    {
      Iris_BytecodeGenerateFnPrologue(bytecode, global->nodes.buff[i]);
      Iris_BytecodeGenerateBodyStmt(bytecode, Iris_Cast(Iris_FnDefStmt*, global->nodes.buff[i]->as)->body->as);
      Iris_BytecodeGenerateFnEpilogue(bytecode, global->nodes.buff[i]);
    }
  }
}

void_t Iris_BytecodeDump(const Iris_Bytecode* bytecode)
{
  ptr_t ip = bytecode->buff;
  i64_t a = 0;
  Iris_TypeKind type = 0;
  Iris_Opcode code = 0;

  printf("Size of bytecode: %li\n", bytecode->len);

  while(ip - (ptr_t)bytecode->buff < bytecode->len)
  {
    code = *(Iris_Opcode*)ip;
    ip += sizeof(code);

    printf("\e[2m%s\e[0m ", Iris_BytecodeOpcToString(code));

    switch(code) 
    {
      case OPC_FN:
        a = *(i64_t*)ip;
        ip += sizeof(a);
        printf("%li\n", a);
        break;
      case OPC_FN_CALL:
        a = *(i64_t*)ip;
        ip += sizeof(a);
        printf("%li\n", a);
        break;
      case OPC_LOADLT:
      case OPC_LOADLC:
      case OPC_LOADGC:
        type = *(Iris_TypeKind*)ip;
        ip += sizeof(Iris_TypeKind);
        a = *(i64_t*)ip;
        ip += sizeof(i64_t);
        printf("%s %li\n", Iris_BytecodeTypeToString(type), a);
        break;
      case OPC_MOVGC:
      case OPC_MOVLC:
        type = *(Iris_TypeKind*)ip;
        ip += sizeof(Iris_TypeKind);
        a = *(i64_t*)ip;
        ip += sizeof(i64_t);
        printf("%s %li\n", Iris_BytecodeTypeToString(type), a);
        break;
      case OPC_JMP:
      case OPC_JMPIF:
      case OPC_JMPNIF:
        a = *(i64_t*)ip;
        ip += sizeof(i64_t);
        printf("%li\n", a);
        break;
      case OPC_FN_RETVAL:
      case OPC_FN_RETVOID:
      case OPC_ADD:
      case OPC_MUL:
      case OPC_SUB:
      case OPC_DIV:
      case OPC_MOD:
      case OPC_OR:
      case OPC_AND:
      case OPC_NOT:
      case OPC_BIT_AND:
      case OPC_BIT_NOT:
      case OPC_BIT_OR:
      case OPC_BIT_XOR:
      case OPC_BIT_LSHIFT:
      case OPC_BIT_RSHIFT:
      case OPC_CMPEGT:
      case OPC_CMPELT:
      case OPC_CMPEQ:
      case OPC_CMPGT:
      case OPC_CMPLT:
      case OPC_CMPNEQ:
      default: printf("\n");
    }
  }
}
