#ifndef H_IRIS_REPORT
#define H_IRIS_REPORT

#include <stdio.h>
#include <stdlib.h>

#include "typedef.h"

#define IRIS_ANSI_RES "\e[0m"
#define IRIS_ANSI_ERR "\e[91;1m"
#define IRIS_ANSI_DIM "\e[2m"

#define IRIS_ERR        "Error"
#define IRIS_ERR_SYNTAX "Syntax Error"
#define IRIS_ERR_REDEF  "Redefenition Error"
#define IRIS_ERR_NOTDEF "Not Found Error"

#define Iris_ReportSimpleError(msg, ...) do { \
  printf(IRIS_ANSI_ERR "Error: " IRIS_ANSI_RES msg "\n", __VA_ARGS__); \
  exit(-1); \
} while(false)

#define Iris_ReportSyntaxError(prev, curr, expected) do { \
  Iris_Token* _curr = curr != null ? curr : prev; \
  printf( \
    IRIS_ANSI_ERR IRIS_ERR_SYNTAX \
    IRIS_ANSI_RES IRIS_ANSI_DIM "[%zu:%zu]:" IRIS_ANSI_RES \
    " expected \"%s\" before \"%.*s\"", \
    _curr->row, _curr->col, \
    expected, \
    prev->len, prev->start \
  ); \
  exit(-1); \
} while(true)

#define Iris_ReportRedefenitionEror(first_def) do { \
  printf( \
    IRIS_ANSI_ERR IRIS_ERR_REDEF \
    IRIS_ANSI_RES IRIS_ANSI_DIM "[%zu:%zu]:" IRIS_ANSI_RES \
    " \"%.*s\" already exist", \
    first_def->row, first_def->col, \
    first_def->len, first_def->start \
  ); \
  exit(-1); \
} while(true)

#define Iris_ReportNotFound(what) do { \
  printf( \
    IRIS_ANSI_ERR IRIS_ERR_NOTDEF \
    IRIS_ANSI_RES IRIS_ANSI_DIM "[%zu:%zu]:" IRIS_ANSI_RES \
    " could not find \"%.*s\"", \
    what->row, what->col, \
    what->len, what->start \
  ); \
  exit(-1); \
} while(true)

#endif
