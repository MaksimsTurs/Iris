#include "iris/lexer.h"

#define Iris_LexerIsEof($lex)           (($lex)->ch == EOF)
#define Iris_LexerIsAlpha($lex)         ((($lex)->ch >= 'a' && ($lex)->ch <= 'z') || (($lex)->ch >= 'A' && ($lex)->ch <= 'Z'))
#define Iris_LexerIsNum($lex)           ((($lex)->ch >= '0' && ($lex)->ch <= '9') || Iris_LexerIsAlpha($lex) || ($lex)->ch == '.')
#define Iris_LexerIsIdent($lex)         (Iris_LexerIsAlpha($lex) || ($lex)->ch == '_')
#define Iris_LexerIsSpace($lex)         (($lex)->ch == ' ' || ($lex)->ch == '\t' || ($lex)->ch == '\n')
#define Iris_LexerIsComment($lex)       (($lex)->ch == '-' && Iris_LexerPickChar($lex, 0) == '-')
#define Iris_LexerIsNanLit($token)      Iris_StrnCmp(($token)->start, "nan", 3)
#define Iris_LexerIsInfinityLit($token) Iris_StrnCmp(($token)->start, "infinity", 8)
#define Iris_LexerIsNullLit($token)     Iris_StrnCmp(($token)->start, "null", 4)
#define Iris_LexerIsTrueLit($token)     Iris_StrnCmp(($token)->start, "true", 4)
#define Iris_LexerIsFalseLit($token)    Iris_StrnCmp(($token)->start, "false", 5)
#define Iris_LexerIsNumLit($lex)        ((($lex)->ch == '0' && Iris_LexerPickChar($lex, 0) == 'x') || Iris_LexerIsNum($lex))
#define Iris_LexerIsStringLit($lex)     (($lex)->ch == '"')
#define Iris_LexerCalcIndex($lex)       ((char_t*)($lex)->src.buff - (char_t*)($lex)->src_base)

static Iris_Token* Iris_LexerNewToken(Iris_Lexer* lex, Iris_TokenKind kind)
{
  Iris_Token token = {0};

  token.type = kind;
  token.start = lex->src.buff - 1;
  token.col = lex->col;
  token.row = lex->row + 1;
  token.len = 1;

  // TODO: Add error handling if expading fails.
  if(!Iris_ArrayHasCap(lex->tokens))
  {
    Iris_ArrayExpand(lex->tokens, lex->tokens.cap * 2);
  }

  Iris_ArrayPush(lex->tokens, token);

  return Iris_ArrayAtBack(lex->tokens);
}

// TODO: Replace it with Map or Set.
static bool_t Iris_LexerIsKeyword(const Iris_Token* token)
{
  static const char_t* keywords[] = { 
    "ret", 
    "strct", 
    "var", "const", 
    "if", "else", "elif", 
    "for"
  };
  static const i64_t len = Iris_ArrayLen(keywords);

  bool_t is = false;

  for(i64_t i = 0; i < len; i++) {
    if((i64_t)strlen(keywords[i]) != token->len)
    {
      is = false;
    }
    else if(Iris_StrnCmp(keywords[i], token->start, token->len))
    {
      is = true;
      break;
    }
  }

  return is;
}

static bool_t Iris_LexerIsType(const Iris_Token* token)
{
  static const char_t* types[] = {
    "void",
    "ptr",
    "i8", "i16", "i32", "i64",
    "u8", "u16", "u32", "u64",
    "f32", "f64",
  };
  static const i64_t len = Iris_ArrayLen(types);

  bool_t is = false;

  for(i64_t i = 0; i < len; i++) {
    if((i64_t)strlen(types[i]) != token->len)
    {
      is = false;
    }
    else if(Iris_StrnCmp(types[i], token->start, token->len))
    {
      is = true;
      break;
    }
  }

  return is;
}

static char_t Iris_LexerPickChar(const Iris_Lexer* lex, i64_t offset)
{
  const i64_t index = Iris_LexerCalcIndex(lex) + offset;

  if(index > lex->src.len)
  {
    return '\0';
  }

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
     
      // TODO: Check "len" overflow.
      while(Iris_LexerIsIdent(lex) || Iris_LexerIsNum(lex))
      {
        token->len++;
        Iris_LexerConsumeChar(lex);
      }

      if(Iris_LexerIsKeyword(token))
      {
        token->type = TK_KEYWORD;
      }
      else if(Iris_LexerIsType(token))
      {
        token->type = TK_TYPE;
      }
      else if(Iris_LexerIsTrueLit(token))
      {
        token->type = TK_TRUELIT;
      }
      else if(Iris_LexerIsFalseLit(token))
      {
        token->type = TK_FALSELIT;
      }
      else if(Iris_LexerIsInfinityLit(token))
      {
        token->type = TK_INFINITYLIT;
      }
      else if(Iris_LexerIsNanLit(token))
      {
        token->type = TK_NANLIT;
      }
      else if(Iris_LexerIsNullLit(token))
      {
        token->type = TK_NULLLIT;
      }
    }
    else if(Iris_LexerIsStringLit(lex))
    {
      token = Iris_LexerNewToken(lex, TK_STRINGLIT);
      token->start++;
      Iris_LexerConsumeChar(lex);

      // TODO: Check "len" overflow.
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
      
      // TODO: Check "len" overflow.
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
        case '{': token->type = TK_BBRACES_L; break; 
        case '}': token->type = TK_BBRACES_R; break;
        case '(': token->type = TK_BPAR_L;    break;
        case ')': token->type = TK_BPAR_R;    break;
        case '+': token->type = TK_PLUS;      break;
        case '-': token->type = TK_MINUS;     break;
        case '*': token->type = TK_MUL;       break;
        case '/': token->type = TK_DIV;       break;
        case ';': token->type = TK_SEMI;      break;
        case ':': token->type = TK_COLON;     break;
        case '&':
          token->type = TK_AND;

          if(lex->ch == '&')
          {
            token->type = TK_AND;
            token->len++;
            Iris_LexerConsumeChar(lex);
          }
          break;
        case '|':
          token->type = TK_OR;

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
        case '>':
          token->type = TK_GT;
          
          if(lex->ch == '=')
          {
            token->type = TK_EQ_GT;
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
        default:
          token->type = TK_UNKNOWN;
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
  for(i64_t i = 0; i < lex->tokens.len; i++)
  {
    Iris_LexerPrint(Iris_ArrayAt(lex->tokens, i));
  }
}

void_t Iris_LexerPrint(const Iris_Token *token)
{
  printf(
    "[%04i:%04i]"IRIS_ANSI_DIM" %s " IRIS_ANSI_RES "%.*s\n",
    token->row, token->col,
    Iris_LexerTokenTypeToString(token->type),
    token->len, token->start
  );
}

Iris_Token* Iris_LexerConsumeToken(Iris_Lexer* lex)
{
  Iris_Token* token = Iris_ArrayAt(lex->tokens, lex->tokens.iter);

  if(token->type == TK_EOF)
  {
    return null;
  }

  lex->tokens.iter++;
  return token;
}

Iris_Token* Iris_LexerPickToken(Iris_Lexer* lex, i64_t offset)
{
  const i64_t index = lex->tokens.iter + offset;
  Iris_Token* token = null;

  if(index >= lex->tokens.len)
  {
    return null;
  }

  token = Iris_ArrayAt(lex->tokens, index);

  if(!token || token->type == TK_EOF)
  {
    token = null;
  }

  return token;
}

const char_t* Iris_LexerTokenTypeToString(Iris_TokenKind type)
{
  switch(type) {
    case TK_IDENT:
      return "Ident";
    case TK_KEYWORD:
      return "Keyword";
    case TK_TYPE:
      return "Type";
    case TK_BPAR_L:
    case TK_BPAR_R:
      return "Parenthesis";
    case TK_BBRACES_R:
    case TK_BBRACES_L:
      return "Braces";
    case TK_ASSIGN:
      return "Assign";
    case TK_SEMI:
      return "Semi";
    case TK_COLON:
      return "Colon";
    case TK_NOT:
      return "Not";
    case TK_PLUS:
      return "Plus";
    case TK_MINUS:
      return "Minus";
    case TK_MUL:
      return "Mul";
    case TK_DIV:
      return "Div";
    case TK_EQ:
      return "Eq";
    case TK_NOT_EQ:
      return "NotEq";
    case TK_EQ_LT:
      return "EqLess";
    case TK_EQ_GT:
      return "EqGreater";
    case TK_GT:
      return "Greater";
    case TK_LT:
      return "Less";
    case TK_NULLLIT:
      return "NullLit";
    case TK_TRUELIT:
      return "TrueLit";
    case TK_FALSELIT:
      return "FalseLit";
    case TK_NANLIT:
      return "NanLit";
    case TK_INFINITYLIT:
      return "InfinityLit";
    case TK_NUMLIT:
      return "NumLit";
    case TK_STRINGLIT:
      return "StringLit";
    case TK_EOF:
      return "EOF";
    case TK_UNKNOWN:
    default:
      return "Unknown";
  }
}
