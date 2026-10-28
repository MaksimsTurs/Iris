#ifndef H_IRIS_OPCODES
#define H_IRIS_OPCODES

#define Iris_OpcodeCreate(name, id, shift) OPC_##name = (id | (1 << (shift + 8)))

typedef enum
{
  OPCID_ADD = 1,
  OPCID_SUB,
  OPCID_MUL,
  OPCID_DIV,
  OPCID_PUSH,
  OPCID_MOV,
  OPCID_PROLOG,
  OPCID_RET
} Iris_OpcodeId;

typedef enum 
{
  Iris_OpcodeCreate(ADD,      OPCID_ADD,    0),
  Iris_OpcodeCreate(SUB,      OPCID_SUB,    0),
  Iris_OpcodeCreate(MUL,      OPCID_MUL,    0),
  Iris_OpcodeCreate(DIV,      OPCID_DIV,    0),
  Iris_OpcodeCreate(PUSHNLOS, OPCID_PUSH,   0),
  Iris_OpcodeCreate(PUSHNLSF, OPCID_PUSH,   1),
  Iris_OpcodeCreate(PUSHSFOS, OPCID_PUSH,   2),
  Iris_OpcodeCreate(PUSHSFSF, OPCID_PUSH,   3),
  Iris_OpcodeCreate(MOVOSSF,  OPCID_MOV,    0),
  Iris_OpcodeCreate(PROLOG,   OPCID_PROLOG, 0),
  Iris_OpcodeCreate(RETVOID,  OPCID_RET,    0),
  Iris_OpcodeCreate(RETVAL,   OPCID_RET,    1),
} Iris_Opcode;

#endif
