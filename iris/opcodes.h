#ifndef H_IRIS_OPC
#define H_IRIS_OPC

#define Iris_OpcodeCreate($name, $id, $type) OPC_##$name = ($id | (1 << ($type + 8)))

typedef enum
{
  OPCID_ADD,
  OPCID_SUB,
  OPCID_MUL,
  OPCID_DIV,
  OPCID_MOD,
  OPCID_OR,
  OPCID_AND,
  OPCID_NOT,
  OPCID_BIT_OR,
  OPCID_BIT_AND,
  OPCID_BIT_NOT,
  OPCID_BIT_XOR,
  OPCID_BIT_LSHIFT,
  OPCID_BIT_RSHIFT,
  OPCID_MOV,
  OPCID_LOAD,
  OPCID_FN,
  OPCID_FN_RET,
  OPCID_FN_CALL,
  OPCID_JMP,
  OPCID_CMP,
} Iris_OpcodeId;

typedef enum 
{
  Iris_OpcodeCreate(ADD,        OPCID_ADD,        0),
  Iris_OpcodeCreate(SUB,        OPCID_SUB,        0),
  Iris_OpcodeCreate(MUL,        OPCID_MUL,        0),
  Iris_OpcodeCreate(DIV,        OPCID_DIV,        0),
  Iris_OpcodeCreate(MOD,        OPCID_MOD,        0),
  Iris_OpcodeCreate(OR,         OPCID_OR,         0),
  Iris_OpcodeCreate(AND,        OPCID_AND,        0),
  Iris_OpcodeCreate(NOT,        OPCID_NOT,        0),
  Iris_OpcodeCreate(BIT_OR,     OPCID_BIT_OR,     0),
  Iris_OpcodeCreate(BIT_AND,    OPCID_BIT_AND,    0),
  Iris_OpcodeCreate(BIT_NOT,    OPCID_BIT_NOT,    0),
  Iris_OpcodeCreate(BIT_XOR,    OPCID_BIT_XOR,    0),
  Iris_OpcodeCreate(BIT_LSHIFT, OPCID_BIT_LSHIFT, 0),
  Iris_OpcodeCreate(BIT_RSHIFT, OPCID_BIT_RSHIFT, 0),
  Iris_OpcodeCreate(LOADLT,     OPCID_LOAD,       0),
  Iris_OpcodeCreate(LOADLC,     OPCID_LOAD,       1),
  Iris_OpcodeCreate(LOADGC,     OPCID_LOAD,       2),
  Iris_OpcodeCreate(MOVLC,      OPCID_MOV,        0),
  Iris_OpcodeCreate(MOVGC,      OPCID_MOV,        1),
  Iris_OpcodeCreate(FN,         OPCID_FN,         0),
  Iris_OpcodeCreate(FN_CALL,    OPCID_FN_CALL,    0),
  Iris_OpcodeCreate(FN_RETVOID, OPCID_FN_RET,     0),
  Iris_OpcodeCreate(FN_RETVAL,  OPCID_FN_RET,     1),
  Iris_OpcodeCreate(JMP,        OPCID_JMP,        0),
  Iris_OpcodeCreate(JMPIF,      OPCID_JMP,        1),
  Iris_OpcodeCreate(JMPNIF,     OPCID_JMP,        2),
  Iris_OpcodeCreate(CMPEQ,      OPCID_CMP,        0),
  Iris_OpcodeCreate(CMPNEQ,     OPCID_CMP,        1),
  Iris_OpcodeCreate(CMPGT,      OPCID_CMP,        2),
  Iris_OpcodeCreate(CMPLT,      OPCID_CMP,        3),
  Iris_OpcodeCreate(CMPEGT,     OPCID_CMP,        4),
  Iris_OpcodeCreate(CMPELT,     OPCID_CMP,        5),
} Iris_Opcode;

#endif
