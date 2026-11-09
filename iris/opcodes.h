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
  OPCID_FN,
  OPCID_FN_RET,
  OPCID_FN_CALL
} Iris_OpcodeId;

typedef enum 
{
  Iris_OpcodeCreate(ADD,        OPCID_ADD,      0),
  Iris_OpcodeCreate(SUB,        OPCID_SUB,      0),
  Iris_OpcodeCreate(MUL,        OPCID_MUL,      0),
  Iris_OpcodeCreate(DIV,        OPCID_DIV,      0),
  Iris_OpcodeCreate(COPYSFOS,   OPCID_COPY,     0),
  Iris_OpcodeCreate(COPYSFSF,   OPCID_COPY,     1),
  Iris_OpcodeCreate(MOVNLSF,    OPCID_MOV,      0),
  Iris_OpcodeCreate(MOVNLOS,    OPCID_MOV,      1),
  Iris_OpcodeCreate(MOVOSSF,    OPCID_MOV,      2),
  Iris_OpcodeCreate(FN,         OPCID_FN,       0),
  Iris_OpcodeCreate(FN_CALL,    OPCID_FN_CALL,  0),
  Iris_OpcodeCreate(FN_RETVOID, OPCID_FN_RET,   0),
  Iris_OpcodeCreate(FN_RETVAL,  OPCID_FN_RET,   1),
} Iris_Opcode;

#endif
