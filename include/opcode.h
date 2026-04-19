#ifndef OPCODE_H
#define OPCODE_H

#include <stddef.h>

#include "constants.h"

#define FLAG_OP_FORMAT_1 1
#define FLAG_OP_FORMAT_2 2
#define FLAG_OP_FORMAT_3 4
#define FLAG_OP_XE 8

typedef enum { OP_NULL, OP_MACHINE, OP_DIRECTIVE } OpType;

typedef enum {
    CODE_ADD = 0x18,
    CODE_ADDF = 0x58,
    CODE_ADDR = 0x90,
    CODE_AND = 0x40,
    CODE_CLEAR = 0xB4,
    CODE_COMP = 0x28,
    CODE_COMPF = 0x88,
    CODE_COMPR = 0xA0,
    CODE_DIV = 0x24,
    CODE_DIVF = 0x64,
    CODE_DIVR = 0x9C,
    CODE_FIX = 0xC4,
    CODE_FLOAT = 0xC0,
    CODE_HIO = 0xF4,
    CODE_J = 0x3C,
    CODE_JEQ = 0x30,
    CODE_JGT = 0x34,
    CODE_JLT = 0x38,
    CODE_JSUB = 0x48,
    CODE_LDA = 0x00,
    CODE_LDB = 0x68,
    CODE_LDCH = 0x50,
    CODE_LDF = 0x70,
    CODE_LDL = 0x08,
    CODE_LDS = 0x6C,
    CODE_LDT = 0x74,
    CODE_LDX = 0x04,
    CODE_LPS = 0xD0,
    CODE_MUL = 0x20,
    CODE_MULF = 0x60,
    CODE_MULR = 0x98,
    CODE_NORM = 0xC8,
    CODE_OR = 0x44,
    CODE_RD = 0xD8,
    CODE_RMO = 0xAC,
    CODE_RSUB = 0x4C,
    CODE_SHIFTL = 0xA4,
    CODE_SHIFTR = 0xA8,
    CODE_SIO = 0xF0,
    CODE_SSK = 0xEC,
    CODE_STA = 0x0C,
    CODE_STB = 0x78,
    CODE_STCH = 0x54,
    CODE_STF = 0x80,
    CODE_STI = 0xD4,
    CODE_STL = 0x14,
    CODE_STS = 0x7C,
    CODE_STSW = 0xE8,
    CODE_STT = 0x84,
    CODE_STX = 0x10,
    CODE_SUB = 0x1C,
    CODE_SUBF = 0x5C,
    CODE_SUBR = 0x94,
    CODE_SVC = 0xB0,
    CODE_TD = 0xE0,
    CODE_TIO = 0xF8,
    CODE_TIX = 0x2C,
    CODE_TIXR = 0xB8,
    CODE_WD = 0xDC
} OpCode;

typedef enum {
    CODE_DIR_BASE,
    CODE_DIR_NOBASE,
    CODE_DIR_START,
    CODE_DIR_END,
    CODE_DIR_BYTE,
    CODE_DIR_WORD,
    CODE_DIR_RESB,
    CODE_DIR_RESW
} DirCode;

typedef struct {
    char mnemonic[MAX_MNEMONIC_LENGTH];
    OpType type;
    size_t opcode;
    size_t flags;
} OpcodeEntry;

size_t searchOpcode(char*);
OpcodeEntry getOpcode(size_t);

#endif  // OPCODE_H
