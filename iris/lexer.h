#ifndef H_IRIS_LEX
#define H_IRIS_LEX

#include "iris/mem.h"
#include "iris/utils.h"
#include "iris/typedef.h"
#include "iris/ansicodes.h"

#include <stdio.h>
#include <stdint.h>

#define IRIS_LEXER_DEF_TOKENS_CAP 12
#define IRIS_LEXER_MAX_TOKEN_LEN  UINT16_MAX - 2

typedef enum 
{
  TK_UNKNOWN = 0,
  TK_NANLIT,
  TK_INFINITYLIT,
  TK_NULLLIT,
  TK_TRUELIT,
  TK_FALSELIT,
  TK_NUMLIT,
  TK_STRINGLIT,
  TK_TYPE,
  TK_IDENT,
  TK_KEYWORD,
  TK_BPAR_L,
  TK_BPAR_R,
  TK_BBRACES_L,
  TK_BBRACES_R,
  TK_SEMI,
  TK_COLON,
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
  TK_EOF,
  TK_AND,
  TK_OR
} Iris_TokenKind;

typedef struct 
{
  i16_t          len;
  Iris_TokenKind type;
  char_t*        start;
  i32_t          row;
  i32_t          col;
} Iris_Token;

typedef struct
{
  Iris_Token* buff;
  i64_t       cap;
  i64_t       len;
  i64_t       iter;
} Iris_Tokens;

typedef struct 
{
  char_t      ch;
  i32_t       row;
  i32_t       col;

  char_t*     src_base;
  Iris_Tokens tokens;
  Iris_Source src;
} Iris_Lexer;

void_t        Iris_LexerNew(Iris_Lexer* lex, const i8_t* filepath);
void_t        Iris_LexerTokenize(Iris_Lexer* lex);
void_t        Iris_LexerDestroy(Iris_Lexer* lex);
Iris_Token*   Iris_LexerConsumeToken(Iris_Lexer* lex);
Iris_Token*   Iris_LexerPickToken(Iris_Lexer* lex, i64_t offset);
void_t        Iris_LexerDump(const Iris_Lexer* lex);
void_t        Iris_LexerPrint(const Iris_Token* token);
const char_t* Iris_LexerTokenTypeToString(Iris_TokenKind kind);

#endif
