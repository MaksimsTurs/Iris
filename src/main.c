#include "iris/iris.h"
#include "iris/lexer.h"
#include "iris/parser.h"
// #include "iris/codegen.h"
// #include "iris/vm.h"

int main(i32_t argc, i8_t** argv)
{
  Iris_Options options = {0};
  Iris_Lexer lex = {0};
  Iris_Parser parser = {0};
  // Iris_Codegen codegen = {0};
  // Iris_Vm vm = {0};

  if(Iris_ProcessCmdArgs(&options, argv, argc))
  {
    Iris_LexerNew(&lex, options.filepath);
    Iris_LexerTokenize(&lex);

    if(options.p_tokens)
    {
      Iris_LexerDump(&lex);
    }

    Iris_ParserNew(&parser, &lex);
    Iris_ParserParse(&parser);

    if(options.p_ast)
    {
      Iris_ParserDump(parser.root, -1);
    }
    //
    // Iris_CodegenInit(&codegen, &parser);
    // Iris_CodegenGenerate(&codegen);
    //
    // if(options.p_code)
    // {
    //   Iris_CodegenDump(&codegen);
    // }

    // Iris_VmNew(&vm, &codegen);
    // Iris_VmStart(&vm);
  }

  return -1;
}
