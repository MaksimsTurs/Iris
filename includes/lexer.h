#ifndef H_IRIS_LEX
#define H_IRIS_LEX

#include <stdio.h>

#include "mem.h"
#include "utils.h"
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
  Iris_TokenType type;
  i8_t*     start;
  i32_t     len;
  i32_t     row;
  i32_t     col;
} Iris_Token;

typedef struct {
  i32_t row;
  i32_t col;
  i8_t  ch;

  Iris_MemBuffer*   tokens;
  u64_t             i;
  Iris_MemBuffer*   src;
  i8_t*             base;
} Iris_Lexer;

void_t      Iris_LexerTokenize(Iris_Lexer* lex);
void_t      Iris_LexerDump(Iris_Lexer* lex);
void_t      Iris_LexerPrintToken(const Iris_Token* token);
Iris_Token* Iris_LexerConsumeToken(Iris_Lexer* lex);
Iris_Token* Iris_LexerPeekToken(Iris_Lexer* lex, u64_t off);
i8_t*       Iris_LexerTokenTypeToString(Iris_TokenType type);

#endif
