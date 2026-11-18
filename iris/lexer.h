#ifndef H_IRIS_LEX
#define H_IRIS_LEX

#include "iris/mem.h"
#include "iris/utils.h"
#include "iris/typedef.h"

#include <stdio.h>
#include <stdint.h>

#define Iris_LexerIsSomeKeyword($token, $keyword) Iris_StrnCmp(($token)->start, $keyword, Iris_Max(($token)->len, (i16_t)(sizeof($keyword) - 1)))

#define IRIS_LEXER_DEF_TOKENS_CAP 32

typedef enum 
{
  TK_UNKNOWN = -1,
  TK_IF,
  TK_ELSE,
  TK_ELIF,
  TK_FOR,
  TK_RET,
  TK_VAR,
  TK_CONST,
  TK_IMPRT,
  TK_EXPRT,
  TK_STRCT,
  // Literals
  TK_NANLIT,
  TK_INFINITYLIT,
  TK_NILLIT,
  TK_TRUELIT,
  TK_FALSELIT,
  TK_NUMLIT,
  TK_STRINGLIT,
  // Words
  TK_TYPE,
  TK_IDENT,
  TK_KEYWORD,
  TK_MODULE,
  // Punctuation
  TK_BPAR_L,
  TK_BPAR_R,
  TK_BBRACES_L,
  TK_BBRACES_R,
  TK_SBRACKET_L,
  TK_SBRACKET_R,
  TK_SEMI,
  TK_COLON,
  TK_COMMA,
  // Operators
  TK_ASSIGN,
  TK_NOT,
  TK_PLUS,
  TK_MINUS,
  TK_MUL,
  TK_DIV,
  TK_MOD,
  TK_EQ,
  TK_NOT_EQ,
  TK_EQ_LT,
  TK_EQ_GT,
  TK_GT,
  TK_LT,
  TK_AND,
  TK_OR,
  TK_BIT_OR,
  TK_BIT_AND,
  TK_BIT_NOT,
  TK_BIT_XOR,
  TK_BIT_LSHIFT,
  TK_BIT_RSHIFT,
  TK_EOF
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
