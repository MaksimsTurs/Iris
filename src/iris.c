#include "iris/iris.h"

#define IRIS_VERSION "1.0.0"

#define Iris_PrintVersion()           printf(IRIS_ANSI_DIM "Iris version: " IRIS_ANSI_RES IRIS_VERSION"\n")
#define Iris_PrintUsage() do {                                              \
  Iris_PrintSection("Usage");                                               \
  Iris_PrintText("irisc");                                                  \
  Iris_PrintText("irisc <help | version>");                                 \
  Iris_PrintText("irisc <filepath> [...options]");                          \
  Iris_PrintSection("Base Options");                                        \
  Iris_PrintOption("usage",     "Prints this usage text.");                 \
  Iris_PrintOption("version",   "Prints version of a interpreter.");        \
  Iris_PrintSection("Printing Options");                                    \
  Iris_PrintOption("ptokens",   "Prints generated tokens.");                \
  Iris_PrintOption("past",      "Prints generated abstract syntax tree.");  \
  Iris_PrintOption("pcode",     "Prints generated code.");                  \
} while(0)
#define Iris_PrintText(text)          printf("  " text "\n");
#define Iris_PrintSection(head)       printf(IRIS_ANSI_DIM head IRIS_ANSI_RES ":\n")
#define Iris_PrintOption(name, desc)  do { \
  printf("  %-10s-- ", name);              \
  printf("%s\n", desc);                    \
} while(0)

bool_t Iris_ProcessCmdArgs(Iris_Options* options, i8_t** args, i32_t len)
{
  if(len == 1)
  {
    Iris_PrintUsage();
    return false;
  }

  for(i32_t i = 1; i < len; i++)
  {
    if(Iris_StrnCmp(args[i], "usage", 4))
    {
      Iris_PrintUsage();
      return false;
    }
    else if(Iris_StrnCmp(args[i], "version", 7))
    {
      Iris_PrintVersion();
      return false;
    }
    else if(Iris_IsSource(args[i]))
    {
      options->filepath = args[i];
    } 
    else if(Iris_StrnCmp(args[i], "ptokens", 6))
    {
      options->p_tokens = true;
    }
    else if(Iris_StrnCmp(args[i], "past", 4))
    {
      options->p_ast = true;
    }
    else if(Iris_StrnCmp(args[i], "pcode", 4))
    {
      options->p_code = true;
    }
  }
  
  return true;
}
