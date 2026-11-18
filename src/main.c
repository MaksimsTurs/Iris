#include "iris/iris.h"
#include "iris/lexer.h"
#include "iris/parser.h"
#include "iris/bytecode.h"
#include "iris/vm.h"

int main(i32_t argc, i8_t** argv)
{
  Iris_Options options = {0};
  Iris_Lexer lex = {0};
  Iris_Parser parser = {0};
  Iris_Bytecode bytecode = {0};
  Iris_Vm vm = {0};

  if(Iris_ProcessCmdArgs(&options, argv, argc))
  {
    Iris_LexerNew(&lex, options.modulepath);
    Iris_LexerTokenize(&lex);
    if(options.p_tokens)  Iris_LexerDump(&lex);

    Iris_ParserNew(&parser, &lex);
    Iris_ParserParse(&parser);
    if(options.p_ast)     Iris_ParserDump(parser.root);

    Iris_BytecodeNew(&bytecode, &parser);
    Iris_BytecodeGenerate(&bytecode);
    if(options.p_code)    Iris_BytecodeDump(&bytecode);
  
    Iris_VmNew(&vm, &bytecode);
    Iris_VmStart(&vm);
  }

  return -1;
}
