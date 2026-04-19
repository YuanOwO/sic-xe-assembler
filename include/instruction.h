#ifndef INSTRUCTION_H
#define INSTRUCTION_H

#include <stddef.h>

#include "constants.h"

#define FLAG_INST_FORMAT_4 0x1
#define FLAG_INST_PC_RELATIVE 0x2
#define FLAG_INST_BASE_RELATIVE 0x4
#define FLAG_INST_INDEXED 0x8
#define FLAG_INST_IMMEDIATE 0x10
#define FLAG_INST_INDIRECT 0x20

#define FLAG_INST_HAS_SECOND_OPERAND 0x100
#define FLAG_INST_NUMERIC_OPERAND1 0x200
#define FLAG_INST_NUMERIC_OPERAND2 0x400

#define FLAG_INST_PARSING_INQUOTE 0x1000  // 解析 operand 時遇到引號了，後面要等到另一個引號才結束 operand
#define FLAG_INST_PARSING_ODDCHAR 0x2000  // X'...' 遇到奇數個字元了，後面還要再等一個字元才結束 operand

typedef struct {
    size_t address;
    size_t flags;
    size_t opcodeIndex;
    size_t operandState;
    char objcode[MAX_OBJCODE_LENGTH];
    char label[MAX_LABEL_LENGTH];
    char operand1[MAX_OPERAND_LENGTH];  // 若 opcode 是 0，則存放錯誤訊息
    char operand2[MAX_LABEL_LENGTH];
    int numOperand1;  // operand1 的數值（如果 operand1 是數字的話）
    int numOperand2;  // operand2 的數值（如果 operand2 是數字的話）
} Instruction;

#endif  // INSTRUCTION_H
