#include "lexer.h"
#include "parser.h"
#include "bytecode.h"
#include "vm.h"
#include "utils.h"

int main(void)
{
  Iris_Lexer lex = {0};
  Iris_Parser parser = {0};
  Iris_ByteCode bytecode = {0};
  Iris_Vm vm = {0};

  lex.src = Iris_LoadFile("./main.iri");

  Iris_LexerTokenize(&lex);
  Iris_LexerDump(&lex);

  Iris_ParserParse(&parser, &lex);
  Iris_ParserDump(parser.root);

  Iris_ByteCodeGenerate(&bytecode, &parser);
  Iris_ByteCodeDump(&bytecode);

  Iris_VmStart(&vm, &bytecode);

  return 0;
}
