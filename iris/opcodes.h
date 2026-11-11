#ifndef H_IRIS_OPC
#define H_IRIS_OPC

#define Iris_OpcodeCreate(name, id, shift) OPC_##name = (id | (1 << (shift + 8)))

typedef enum
{
  OPCID_ADD = 1,
  OPCID_SUB,
  OPCID_MUL,
  OPCID_DIV,
  OPCID_COPY,
  OPCID_MOV,
  OPCID_PUSH,
  OPCID_LOAD,
  OPCID_FN,
  OPCID_FN_RET,
  OPCID_FN_CALL,
  OPCID_JMP,
  OPCID_CMP,
  OPCID_OR,
  OPCID_AND
} Iris_OpcodeId;

typedef enum
{
  TYPE_OFFSET,
  TYPE_F64,
  TYPE_I64,
  TYPE_BOOL,
  TYPE_NIL,
  TYPE_NAN,
  TYPE_INFINITY,
} Iris_Type;

typedef enum 
{
  Iris_OpcodeCreate(ADD,        OPCID_ADD,      0),
  Iris_OpcodeCreate(SUB,        OPCID_SUB,      0),
  Iris_OpcodeCreate(MUL,        OPCID_MUL,      0),
  Iris_OpcodeCreate(DIV,        OPCID_DIV,      0),
  Iris_OpcodeCreate(COPYSFOS,   OPCID_COPY,     0),
  Iris_OpcodeCreate(COPYSFSF,   OPCID_COPY,     1),
  Iris_OpcodeCreate(PUSHCNSF,   OPCID_PUSH,     4),
  Iris_OpcodeCreate(PUSHCNOS,   OPCID_PUSH,     3),
  Iris_OpcodeCreate(PUSHOS,     OPCID_PUSH,     0),
  Iris_OpcodeCreate(PUSHSF,     OPCID_PUSH,     1),
  Iris_OpcodeCreate(PUSHLT,     OPCID_PUSH,     2),
  Iris_OpcodeCreate(LOADSF,     OPCID_LOAD,     0),
  Iris_OpcodeCreate(LOADLT,     OPCID_LOAD,     1),
  Iris_OpcodeCreate(MOVOSSF,    OPCID_MOV,      0),
  Iris_OpcodeCreate(FN,         OPCID_FN,       0),
  Iris_OpcodeCreate(FN_CALL,    OPCID_FN_CALL,  0),
  Iris_OpcodeCreate(FN_RETVOID, OPCID_FN_RET,   0),
  Iris_OpcodeCreate(FN_RETVAL,  OPCID_FN_RET,   1),
  Iris_OpcodeCreate(JMP,        OPCID_JMP,      0),
  Iris_OpcodeCreate(JMPIF,      OPCID_JMP,      1),
  Iris_OpcodeCreate(JMPNIF,     OPCID_JMP,      2),
  Iris_OpcodeCreate(CMPEQ,      OPCID_CMP,      0),
  Iris_OpcodeCreate(CMPNEQ,     OPCID_CMP,      1),
  Iris_OpcodeCreate(CMPGT,      OPCID_CMP,      2),
  Iris_OpcodeCreate(CMPLT,      OPCID_CMP,      3),
  Iris_OpcodeCreate(CMPEGT,     OPCID_CMP,      4),
  Iris_OpcodeCreate(CMPELT,     OPCID_CMP,      5),
  Iris_OpcodeCreate(OR,         OPCID_OR,       0),
  Iris_OpcodeCreate(AND,        OPCID_AND,      0),
} Iris_Opcode;

#endif
