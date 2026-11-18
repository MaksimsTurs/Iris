#include "iris/lexer.h"

#define Iris_LexerIsEof($lex)           (($lex)->ch == EOF)
#define Iris_LexerIsAlpha($lex)         ((($lex)->ch >= 'a' && ($lex)->ch <= 'z') || (($lex)->ch >= 'A' && ($lex)->ch <= 'Z'))
#define Iris_LexerIsNum($lex)           ((($lex)->ch >= '0' && ($lex)->ch <= '9') || Iris_LexerIsAlpha($lex) || ($lex)->ch == '.')
#define Iris_LexerIsIdent($lex)         (Iris_LexerIsAlpha($lex) || ($lex)->ch == '_')
#define Iris_LexerIsSpace($lex)         (($lex)->ch == ' ' || ($lex)->ch == '\t' || ($lex)->ch == '\n')
#define Iris_LexerIsComment($lex)       (($lex)->ch == '-' && Iris_LexerPickChar($lex, 0) == '-')
#define Iris_LexerIsNanLit($token)      Iris_StrnCmp(($token)->start, "nan", 3)
#define Iris_LexerIsInfinityLit($token) Iris_StrnCmp(($token)->start, "infinity", 8)
#define Iris_LexerIsNullLit($token)     Iris_StrnCmp(($token)->start, "nil", 3)
#define Iris_LexerIsTrueLit($token)     Iris_StrnCmp(($token)->start, "true", 4)
#define Iris_LexerIsFalseLit($token)    Iris_StrnCmp(($token)->start, "false", 5)
#define Iris_LexerIsNumLit($lex)        ((($lex)->ch == '0' && Iris_LexerPickChar($lex, 0) == 'x') || Iris_LexerIsNum($lex))
#define Iris_LexerIsStringLit($lex)     (($lex)->ch == '"')
#define Iris_LexerCalcIndex($lex)       ((char_t*)($lex)->src.buff - (char_t*)($lex)->src_base)

static const char_t* keywords[] = {"if", "else", "elif", "for", "ret", "var", "const", "imprt", "exprt", "strct"};
static const i64_t keywords_len = Iris_ArrayLen(keywords);
static const char_t* types[] = {"void", "nil", "nan", "inf", "ptr", "char", "bool", "i9", "i16", "i32", "i64", "u8", "u16", "u32", "u64", "f32" , "f64"};
static const i64_t types_len = Iris_ArrayLen(types);

static Iris_Token* Iris_LexerNewToken(Iris_Lexer* lex, Iris_TokenKind kind)
{
  Iris_Token token = {0};

  token.type = kind;
  token.start = lex->src.buff - 1;
  token.col = lex->col;
  token.row = lex->row + 1;
  token.len = 1;

  if(!Iris_ArrayHasCap(lex->tokens))
  {
    Iris_ArrayExpand(lex->tokens, lex->tokens.cap * 2);
    if(!lex->tokens.buff)
    {
      Iris_ReportInternError("could not expand tokens array!");
    }
  }

  Iris_ArrayPush(lex->tokens, token);
  return Iris_ArrayAtBack(lex->tokens);
}

static bool_t Iris_LexerIsKeyword(const Iris_Token* token)
{
  bool_t is = false;
  i64_t keyword_len = 0;

  for(i64_t i = 0; i < keywords_len; i++)
  {
    keyword_len = (i64_t)strlen(keywords[i]);
    if(keyword_len != token->len) is = false;
    else if(Iris_StrnCmp(keywords[i], token->start, Iris_Max(token->len, keyword_len))) return true;
  }

  return is;
}

static bool_t Iris_LexerIsType(const Iris_Token* token)
{
  bool_t is = false;
  i64_t type_len = 0;

  for(i64_t i = 0; i < types_len; i++)
  {
    type_len = (i64_t)strlen(types[i]);
    if(type_len != token->len) is = false;
    else if(Iris_StrnCmp(types[i], token->start, Iris_Max(token->len, type_len))) return true;
  }

  return is;
}

static Iris_TokenKind Iris_LexerGetKeywordKind(const Iris_Token* token)
{
  for(i64_t i = 0; i < keywords_len; i++)
  {
    if(Iris_StrnCmp(keywords[i], token->start, token->len)) return Iris_Cast(Iris_TokenKind, i);
  } 
  return TK_UNKNOWN;
}

static char_t Iris_LexerPickChar(const Iris_Lexer* lex, i64_t offset)
{
  const i64_t index = Iris_LexerCalcIndex(lex) + offset;
  if(index > lex->src.len) return '\0';
  return lex->src_base[index];
}

static char_t Iris_LexerConsumeChar(Iris_Lexer* lex)
{
  if(Iris_LexerCalcIndex(lex) > lex->src.len)
  {
    lex->ch = EOF;
  }
  else
  {
    lex->ch = *(char_t*)lex->src.buff;
    lex->src.buff++;

    if(lex->ch == '\n')
    {
      lex->col = 0;
      lex->row++;
    }
    else
    {
      lex->col++;
    }
  }

  return lex->ch;
}

void_t Iris_LexerNew(Iris_Lexer* lex, const i8_t* filepath)
{
  lex->src = Iris_LoadSource(filepath);
  lex->src_base = lex->src.buff;
  lex->row = 0;
  lex->ch = 0;
  lex->col = 0;
  lex->tokens.iter = 0;
  Iris_ArrayNew(lex->tokens, IRIS_LEXER_DEF_TOKENS_CAP);
  if(!lex->tokens.buff)
  {
    Iris_ReportInternError("could not allocate token array!");
  }
}

void_t Iris_LexerDestroy(Iris_Lexer* lex)
{
  lex->tokens.iter = 0;
  lex->ch = 0;
  lex->col = 0;
  lex->row = 0;
  Iris_ArrayDestroy(lex->src);
  Iris_ArrayDestroy(lex->tokens);
}

void_t Iris_LexerTokenize(Iris_Lexer* lex)
{
  Iris_Token* token = null;

  Iris_LexerConsumeChar(lex);
  
  while(!Iris_LexerIsEof(lex))
  {
    if(Iris_LexerIsComment(lex))
    {
      while(Iris_LexerConsumeChar(lex) != '\n');
    }
    else if(Iris_LexerIsIdent(lex))
    {
      token = Iris_LexerNewToken(lex, TK_IDENT);
      Iris_LexerConsumeChar(lex);
      
      while(Iris_LexerIsIdent(lex) || Iris_LexerIsNum(lex))
      {
        token->len++;
        Iris_LexerConsumeChar(lex);
      }

      if(Iris_LexerIsKeyword(token))
      {
        token->type = Iris_LexerGetKeywordKind(token);

        if(Iris_LexerIsSomeKeyword(token, "imprt", 5))
        { 
          while(Iris_LexerIsSpace(lex)) Iris_LexerConsumeChar(lex);
          token = Iris_LexerNewToken(lex, TK_MODULE);
          while(Iris_LexerIsAlpha(lex) || lex->ch == '/')
          {
            token->len++;
            Iris_LexerConsumeChar(lex);
          }
          token->len--;
        }
      }
      else if(Iris_LexerIsType(token))        token->type = TK_TYPE;
      else if(Iris_LexerIsTrueLit(token))     token->type = TK_TRUELIT;
      else if(Iris_LexerIsFalseLit(token))    token->type = TK_FALSELIT;
      else if(Iris_LexerIsInfinityLit(token)) token->type = TK_INFINITYLIT;
      else if(Iris_LexerIsNanLit(token))      token->type = TK_NANLIT;
      else if(Iris_LexerIsNullLit(token))     token->type = TK_NILLIT;
    }
    else if(Iris_LexerIsStringLit(lex))
    {
      token = Iris_LexerNewToken(lex, TK_STRINGLIT);
      token->start++;
      Iris_LexerConsumeChar(lex);

      while(lex->ch != '"')
      {
        token->len++;
        Iris_LexerConsumeChar(lex);
      }
     
      token->len--;
      Iris_LexerConsumeChar(lex);
    }
    else if(Iris_LexerIsNumLit(lex))
    {
      token = Iris_LexerNewToken(lex, TK_NUMLIT);
      Iris_LexerConsumeChar(lex);
      
      while(Iris_LexerIsNum(lex))
      {
        token->len++;
        Iris_LexerConsumeChar(lex);
      }
    }
    else if(!Iris_LexerIsSpace(lex))
    {
      token = Iris_LexerNewToken(lex, TK_UNKNOWN);
      Iris_LexerConsumeChar(lex);

      switch(*token->start)
      {
        case '{': token->type = TK_BBRACES_L;   break; 
        case '}': token->type = TK_BBRACES_R;   break;
        case '(': token->type = TK_BPAR_L;      break;
        case ')': token->type = TK_BPAR_R;      break;
        case '[': token->type = TK_SBRACKET_L;  break;
        case ']': token->type = TK_SBRACKET_R;  break;
        case ';': token->type = TK_SEMI;        break;
        case ':': token->type = TK_COLON;       break;
        case ',': token->type = TK_COMMA;       break;
        case '+': token->type = TK_PLUS;        break;
        case '-': token->type = TK_MINUS;       break;
        case '*': token->type = TK_MUL;         break;
        case '/': token->type = TK_DIV;         break;
        case '%': token->type = TK_MOD;         break;
        case '^': token->type = TK_BIT_XOR;     break;
        case '~': token->type = TK_BIT_NOT;     break;
        case '&':
          token->type = TK_BIT_AND;

          if(lex->ch == '&')
          {
            token->type = TK_AND;
            token->len++;
            Iris_LexerConsumeChar(lex);
          }
          break;
        case '|':
          token->type = TK_BIT_OR;

          if(lex->ch == '|')
          {
            token->type = TK_OR;
            token->len++;
            Iris_LexerConsumeChar(lex);
          }
          break;
        case '<':
          token->type = TK_LT;
          
          if(lex->ch == '=')
          {
            token->type = TK_EQ_LT;
            token->len++;
            Iris_LexerConsumeChar(lex);
          }
          else if(lex->ch == '<')
          {
            token->type = TK_BIT_LSHIFT;
            token->len++;
            Iris_LexerConsumeChar(lex);
          }
          break;
        case '>':
          token->type = TK_GT;
          
          if(lex->ch == '=')
          {
            token->type = TK_EQ_GT;
            token->len++;
            Iris_LexerConsumeChar(lex);
          }
          else if(lex->ch == '>')
          {
            token->type = TK_BIT_RSHIFT;
            token->len++;
            Iris_LexerConsumeChar(lex);
          }
          break;
        case '!':
          token->type = TK_NOT;

          if(lex->ch == '=')
          {
            token->type = TK_NOT_EQ;
            token->len++;
            Iris_LexerConsumeChar(lex);
          }
          break;
        case '=':
          token->type = TK_ASSIGN;
          
          if(lex->ch == '=')
          {
            token->type = TK_EQ;
            token->len++;
            Iris_LexerConsumeChar(lex);
          }
          break;
        default: { /* Skip */ }
      }
    }
    else
    {
      Iris_LexerConsumeChar(lex);
    }
  }

  Iris_LexerNewToken(lex, TK_EOF);
}

void_t Iris_LexerDump(const Iris_Lexer* lex)
{
  for(i64_t i = 0; i < lex->tokens.len; i++) Iris_LexerPrint(Iris_ArrayAt(lex->tokens, i));
}

void_t Iris_LexerPrint(const Iris_Token *token)
{
  printf(
    "[%04i:%04i]" IRIS_ANSI_DIM" %s " IRIS_ANSI_RES "%.*s\n",
    token->row, token->col,
    Iris_LexerTokenTypeToString(token->type),
    token->len, token->start
  );
}

Iris_Token* Iris_LexerConsumeToken(Iris_Lexer* lex)
{
  Iris_Token* token = Iris_ArrayAt(lex->tokens, lex->tokens.iter);
  if(token->type == TK_EOF) return Iris_ArrayAt(lex->tokens, lex->tokens.iter);
  lex->tokens.iter++;
  return token;
}

Iris_Token* Iris_LexerPickToken(Iris_Lexer* lex, i64_t offset)
{
  const i64_t index = lex->tokens.iter + offset;
  Iris_Token* token = null;
  if(index >= lex->tokens.len) return Iris_ArrayAt(lex->tokens, lex->tokens.len);
  token = Iris_ArrayAt(lex->tokens, index);
  return token;
}

const char_t* Iris_LexerTokenTypeToString(Iris_TokenKind type)
{
  switch(type) {
    case TK_IDENT:        return "Ident";
    case TK_IF:
    case TK_ELSE:
    case TK_ELIF:
    case TK_FOR:
    case TK_VAR:
    case TK_CONST:
    case TK_IMPRT:
    case TK_EXPRT:
    case TK_STRCT:
    case TK_RET:
    case TK_KEYWORD:      return "Keyword";
    case TK_MODULE:       return "Module";
    case TK_TYPE:         return "Type";
    case TK_COMMA:        return "Comma";
    case TK_BPAR_L:   
    case TK_BPAR_R:       return "Parenthesis";
    case TK_BBRACES_R:
    case TK_BBRACES_L:    return "Braces";
    case TK_SBRACKET_L:
    case TK_SBRACKET_R:   return "Bracket";
    case TK_ASSIGN:       return "Assign";
    case TK_SEMI:         return "Semi";
    case TK_COLON:        return "Colon";
    case TK_NOT:          return "Not";
    case TK_PLUS:         return "Plus";
    case TK_MINUS:        return "Minus";
    case TK_MUL:          return "Mul";
    case TK_DIV:          return "Div";
    case TK_MOD:          return "Mod";
    case TK_EQ:           return "Eq";
    case TK_NOT_EQ:       return "NotEq";
    case TK_EQ_LT:        return "EqLess";
    case TK_EQ_GT:        return "EqGreater";
    case TK_GT:           return "Greater";
    case TK_LT:           return "Less";
    case TK_AND:          return "And";
    case TK_OR:           return "Or";
    case TK_BIT_AND:      return "BitAnd";
    case TK_BIT_OR:       return "BitOr";
    case TK_BIT_NOT:      return "BitNot";
    case TK_BIT_XOR:      return "BitXor";
    case TK_BIT_LSHIFT:   return "LShift";
    case TK_BIT_RSHIFT:   return "RShift";
    case TK_NILLIT:       return "NilLit";
    case TK_TRUELIT:      return "TrueLit";
    case TK_FALSELIT:     return "FalseLit";
    case TK_NANLIT:       return "NanLit";
    case TK_INFINITYLIT:  return "InfinityLit";
    case TK_NUMLIT:       return "NumLit";
    case TK_STRINGLIT:    return "StringLit";
    case TK_EOF:          return "EOF";
    case TK_UNKNOWN:
    default:
      return "Unknown";
  }
}
