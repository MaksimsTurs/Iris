#ifndef H_IRIS_LEX
#define H_IRIS_LEX

#include <stdio.h>

#include "mem.h"
#include "report.h"
#include "typedef.h"

typedef enum {
  TK_UNKNOWN = 0,
  TK_EOF,
  TK_NUMLIT,
  TK_IDENT,
  TK_KEYWORD,
  TK_BPAR_L,
  TK_BPAR_R,
  TK_BBRACES_L,
  TK_BBRACES_R,
  TK_SEMI,
  TK_COMMA,
  TK_NOT,
  TK_ASSIGN,
  TK_PLUS,
  TK_MINUS,
  TK_MUL,
  TK_DIV,
  TK_EQ,
  TK_NOT_EQ,
  TK_EQ_LT,
  TK_EQ_GT,
  TK_GT,
  TK_LT,
} Iris_TokenType;

typedef struct {
  Iris_TokenType  type;
  i8_t*           start;
  i64_t           len;
  i64_t           row;
  i64_t           col;
} Iris_Token;

typedef struct {
  i64_t         row;
  i64_t         col;
  i8_t          ch;

  ptr_t         src_base;
  Iris_ArrBuff* tokens;
  Iris_ArrBuff* src;

  i64_t         i;
} Iris_Lexer;

void_t      Iris_LexerInit(Iris_Lexer* lex);
void_t      Iris_LexerTokenize(Iris_Lexer* lex);
void_t      Iris_LexerDump(Iris_Lexer* lex);
void_t      Iris_LexerPrintToken(const Iris_Token* token);
Iris_Token* Iris_LexerConsumeToken(Iris_Lexer* lex);
Iris_Token* Iris_LexerPeekToken(Iris_Lexer* lex, i64_t off);
const i8_t* Iris_LexerTokenTypeToString(Iris_TokenType type);

#endif
