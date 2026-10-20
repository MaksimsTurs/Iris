#include "lexer.h"

static Iris_Token* Iris_LexerCreateToken(Iris_Lexer* lex, Iris_TokenType type)
{
  Iris_Token* token = (Iris_Token*)(lex->tokens->buff + (lex->tokens->len * sizeof(Iris_Token)));
  
  token->type = type;
  token->start = lex->src->buff - 1;
  token->col = lex->col;
  token->row = lex->row + 1;
  token->len = 1;

  lex->tokens->len++;
  
  return token;
}

static void_t Iris_LexerInit(Iris_Lexer* lex)
{
  lex->base = lex->src->buff;
  lex->tokens = Iris_MemBufferAlloc(256, sizeof(Iris_Token));
  lex->row = 0;
  lex->ch = 0;
  lex->col = 0;
  lex->i = 0;
}

static inline bool_t Iris_LexerIsAlpha(const Iris_Lexer* lex)
{
  return (lex->ch >= 'a' && lex->ch <= 'z') || (lex->ch >= 'A' && lex->ch <= 'Z');
}

static inline bool_t Iris_LexerIsNum(const Iris_Lexer* lex)
{
  return lex->ch >= '0' && lex->ch <= '9';
}

static inline bool_t Iris_LexerIsIdent(const Iris_Lexer* lex)
{
  return Iris_LexerIsAlpha(lex) || lex->ch == '_';
}

static inline bool_t Iris_LexerIsEOF(const Iris_Lexer* lex)
{
  return lex->ch == EOF;
}

static inline bool_t Iris_LexerIsSpace(const Iris_Lexer* lex)
{
  return lex->ch == ' ' || lex->ch == '\t' || lex->ch == '\n';
}

static const i8_t* keywords[] = { 
  "fn", "return"
};

static bool_t Iris_LexerIsKeyword(const Iris_Token* token)
{
  i32_t length = sizeof(keywords) / sizeof(keywords[0]);

  for(i32_t i = 0; i < length; i++) {
    if(Iris_StrCmprN(keywords[i], token->start, token->len))
    {
      return true;
    }
  }

  return false;
}

static i8_t Iris_LexerReadChar(Iris_Lexer* lex)
{
  if(lex->src->buff - lex->base > (i64_t)lex->src->len)
  {
    lex->ch = EOF;
  }
  else
  {
    lex->ch = *lex->src->buff;
    lex->src->buff++;

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

void_t Iris_LexerTokenize(Iris_Lexer* lex)
{
  Iris_Token* token = NULL;

  Iris_LexerInit(lex);
  Iris_LexerReadChar(lex);

  while(!Iris_LexerIsEOF(lex))
  {
    if(Iris_LexerIsIdent(lex))
    {
      token = Iris_LexerCreateToken(lex, TK_IDENT);
      Iris_LexerReadChar(lex);
      
      while(Iris_LexerIsIdent(lex))
      {
        token->len++;
        Iris_LexerReadChar(lex);
      }

      if(Iris_LexerIsKeyword(token))
      {
        token->type = TK_KEYWORD;
      }
    }
    else if(Iris_LexerIsNum(lex))
    {
      token = Iris_LexerCreateToken(lex, TK_NUMLIT);
      Iris_LexerReadChar(lex);
       
      while(Iris_LexerIsNum(lex))
      {
        token->len++;
        Iris_LexerReadChar(lex);
       }
    }
    else if(!Iris_LexerIsSpace(lex))
    {
      token = Iris_LexerCreateToken(lex, TK_UNKNOWN);
      Iris_LexerReadChar(lex);

      switch(*token->start)
      {
        case '{':
          token->type = TK_BBRACES_L;
          break;
        case '}':
          token->type = TK_BBRACES_R;
          break;
        case '(':
          token->type = TK_BPAR_L;
          break;
        case ')':
          token->type = TK_BPAR_R;
          break;
        case ',':
          token->type = TK_COMMA;
          break;
        case '+':
          token->type = TK_PLUS;
          break;
        case '-':
          token->type = TK_MINUS;
          break;
        case '*':
          token->type = TK_MUL;
          break;
        case '/':
          token->type = TK_DIV;
          break;
        case ';':
          token->type = TK_SEMI;
          break;
        case '<':
          token->type = TK_LT;
          
          if(lex->ch == '=')
          {
            token->type = TK_EQ_LT;
            token->len++;
            Iris_LexerReadChar(lex);
          }
          break;
        case '!':
          token->type = TK_NOT;

          if(lex->ch == '=')
          {
            token->type = TK_NOT_EQ;
            token->len++;
            Iris_LexerReadChar(lex);
          }
          break;
        case '>':
          token->type = TK_GT;
          
          if(lex->ch == '=')
          {
            token->type = TK_EQ_GT;
            token->len++;
            Iris_LexerReadChar(lex);
          }
          break;
        case '=':
          token->type = TK_ASSIGN;
          
          if(lex->ch == '=')
          {
            token->type = TK_EQ;
            token->len++;
            Iris_LexerReadChar(lex);
          }
          break;
        default:
          token->type = TK_UNKNOWN;
      }
    }
    else
    {
      Iris_LexerReadChar(lex);
    }
  }

  Iris_LexerCreateToken(lex, TK_EOF);
}

void_t Iris_LexerDump(Iris_Lexer* lex)
{
  Iris_Token* token = NULL;

  for(u64_t i = 0; i < lex->tokens->len; i++)
  {
    token = (Iris_Token*)(lex->tokens->buff + (i * sizeof(Iris_Token)));
    Iris_LexerPrintToken(token);
  }
}

void_t Iris_LexerPrintToken(const Iris_Token *token)
{
  printf(
    "[%04u:%04u] \e[2m%s \e[m%.*s\n",
    token->row, token->col,
    Iris_LexerTokenTypeToString(token->type),
    token->len, token->start
  );
}

Iris_Token* Iris_LexerConsumeToken(Iris_Lexer* lex)
{
  Iris_Token* token = (Iris_Token*)(lex->tokens->buff + (lex->i * sizeof(Iris_Token)));

  if(token->type == TK_EOF)
  {
    return NULL;
  }

  token = (Iris_Token*)(lex->tokens->buff + (lex->i * sizeof(Iris_Token)));
  lex->i++;

  return token;
}

Iris_Token* Iris_LexerPeekToken(Iris_Lexer* lex, u64_t off)
{
  Iris_Token* token = NULL;

  if(lex->i + off >= lex->tokens->len)
  {
    return NULL;
  }
  else
  {
    token = (Iris_Token*)(lex->tokens->buff + ((lex->i + off) * sizeof(Iris_Token)));

    if(token->type == TK_EOF)
    {
      return NULL;
    }
  }

  return token;
}

i8_t* Iris_LexerTokenTypeToString(Iris_TokenType type)
{
  switch(type) {
    case TK_IDENT:
      return "Ident";
    case TK_KEYWORD:
      return "Keyword";
    case TK_SEMI:
      return "Semi";
    case TK_BPAR_L:
    case TK_BPAR_R:
      return "Parenthesis";
    case TK_BBRACES_R:
    case TK_BBRACES_L:
      return "Braces";
    case TK_ASSIGN:
      return "Assign";
    case TK_COMMA:
      return "Comma";
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
    case TK_NUMLIT:
      return "NumLit";
    case TK_EOF:
      return "EOF";
    case TK_UNKNOWN:
    default:
      return "Unknown";
  }
}
