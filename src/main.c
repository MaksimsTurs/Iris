#include "iris/iris.h"
#include "iris/lexer.h"
#include "iris/parser.h"
#include "iris/bytecode.h"

int main(i32_t argc, i8_t** argv)
{
  Iris_Options options = {0};
  Iris_Lexer lex = {0};
  Iris_Parser parser = {0};
  Iris_Bytecode bytecode = {0};

  if(Iris_ProcessCmdArgs(&options, argv, argc))
  {
    Iris_LexerNew(&lex, options.modulepath);
    Iris_LexerTokenize(&lex);

    Iris_ParserNew(&parser, &lex);
    Iris_ParserParse(&parser);

    Iris_BytecodeNew(&bytecode, &parser);
    Iris_BytecodeGenerate(&bytecode);

    if(options.p_tokens)  Iris_LexerDump(&lex);
    if(options.p_ast)     Iris_ParserDump(parser.root);
    if(options.p_code)    Iris_BytecodeDump(&bytecode);
  }

  return -1;
}
