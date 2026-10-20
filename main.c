#include "lexer.h"
#include "parser.h"
#include "utils.h"

int main(void)
{
  Iris_Lexer lex = {0};
  Iris_Parser parser = {0};

  lex.src = Iris_LoadFile("./main.iri");
  
  Iris_LexerTokenize(&lex);
  Iris_LexerDump(&lex);

  Iris_ParserParse(&parser, &lex);
  Iris_ParserDump(parser.root);

  return 0;
}
