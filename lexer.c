#include "lexer.h"
#include "typedef.h"
#include <stdio.h>
#include <string.h>

static Iris_Token* Iris_LexerCreateToken(Iris_Lexer* lex, Iris_TokenType type)
{
  Iris_Token token = {0};
  
  token.type = type;
  token.start = lex->src->buff - 1;
  token.col = lex->col;
  token.row = lex->row + 1;
  token.len = 1;

  if(lex->tokens->len >= lex->tokens->cap)
  {
    lex->tokens = Iris_ArrBuffReAlloc(lex->tokens, lex->tokens->cap * 2);
    
    if(lex->tokens == null)
    {
      Iris_ReportSimpleError("could not allocate memory for tokens!", null);
    }
  }

  return Iris_ArrBuffPush(lex->tokens, &token);
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
  "fn", "ret",
  "var", "const",
  "if"
};

static bool_t Iris_LexerIsKeyword(const Iris_Token* token)
{
  i64_t keyword_len = 0;
  i64_t length = sizeof(keywords) / sizeof(keywords[0]);
  bool_t res = false;

  for(i64_t i = 0; i < length; i++) {
    keyword_len = (i64_t)strlen(keywords[i]);

    if(keyword_len != token->len)
    {
      res = false;
    }

    if(!strncmp(keywords[i], token->start, keyword_len))
    {
      return true;
    }
  }

  return res;
}

static i8_t Iris_LexerReadChar(Iris_Lexer* lex)
{
  if(lex->src->buff - lex->src_base > (i64_t)lex->src->len)
  {
    lex->ch = EOF;
  }
  else
  {
    lex->ch = *(i8_t*)lex->src->buff;
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

void_t Iris_LexerInit(Iris_Lexer* lex)
{
  lex->src_base = lex->src->buff;
  lex->tokens = Iris_ArrBuffAlloc(256, sizeof(Iris_Token));
  lex->row = 0;
  lex->ch = 0;
  lex->col = 0;
}

void_t Iris_LexerTokenize(Iris_Lexer* lex)
{
  Iris_Token* token = null;

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
  for(u64_t i = 0; i < lex->tokens->len; i++)
  {
    Iris_LexerPrintToken((Iris_Token*)Iris_ArrBuffAt(lex->tokens, i));
  }
}

void_t Iris_LexerPrintToken(const Iris_Token *token)
{
  // TODO: Find %.*s formatting but for u64_t integers.
  printf(
    "[%04zu:%04zu] \e[2m%s \e[m%.*s\n",
    token->row, token->col,
    Iris_LexerTokenTypeToString(token->type),
    (i32_t)token->len, token->start
  );
}

Iris_Token* Iris_LexerConsumeToken(Iris_Lexer* lex)
{
  Iris_Token* token = Iris_ArrBuffAt(lex->tokens, lex->i);

  if(token->type == TK_EOF)
  {
    return null;
  }

  lex->i++;
  return token;
}

Iris_Token* Iris_LexerPeekToken(Iris_Lexer* lex, i64_t offset)
{
  Iris_Token* token = null;
  i64_t index = lex->i + offset;

  if(index < 0 || index > lex->tokens->len)
  {
    return null;
  }
  else
  {
    token = Iris_ArrBuffAt(lex->tokens, index);

    if(token->type == TK_EOF)
    {
      return null;
    }
  }

  return token;
}

const i8_t* Iris_LexerTokenTypeToString(Iris_TokenType type)
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
