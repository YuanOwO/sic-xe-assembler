#ifndef PARSE_H
#define PARSE_H

#include "instruction.h"

typedef enum {
    ST_LN_START,
    ST_LN_LABEL,
    ST_LN_WAIT_MNEM,
    ST_LN_MNEM,
    ST_LN_WAIT_OP1,
    ST_LN_OP1,
    ST_LN_WAIT_OP2,
    ST_LN_OP2,
    ST_LN_DONE,
    ST_LN_ERROR
} LineParseState;

typedef enum {
    ST_OPR_EMPTY,
    ST_OPR_FIRST_OPERAND,
    ST_OPR_COMMA,
    ST_OPR_WAIT_COMMA,
    ST_OPR_INDEXED,
    ST_OPR_WAIT_INDIRECT,
    ST_OPR_INDIRECT,
    ST_OPR_WAIT_IMMEDIATE,
    ST_OPR_IMMEDIATE,
    ST_OPR_IMMEDIATE_NUMBER,
    ST_OPR_NUMERIC,
    ST_OPR_ERROR
} OperandParseState;

typedef void* (*StateFunc)(char c, char** l, char** r, Instruction* inst);

int isInQuote(size_t flags);
int isNumericState(size_t flags, int operandNumber);
char* get_state_name(void* state);

void* state_start(char c, char** l, char** r, Instruction* inst);
void* state_label_or_mnem(char c, char** l, char** r, Instruction* inst);
void* state_wait_mnem(char c, char** l, char** r, Instruction* inst);
void* state_mnem_prefix(char c, char** l, char** r, Instruction* inst);
void* state_mnem(char c, char** l, char** r, Instruction* inst);
void* state_wait_op1(char c, char** l, char** r, Instruction* inst);
void* state_op1_prefix(char c, char** l, char** r, Instruction* inst);
void* state_op1(char c, char** l, char** r, Instruction* inst);
void* state_wait_comma(char c, char** l, char** r, Instruction* inst);
void* state_wait_op2(char c, char** l, char** r, Instruction* inst);
void* state_op2(char c, char** l, char** r, Instruction* inst);
void* state_wait_done(char c, char** l, char** r, Instruction* inst);
void* state_error(char c, char** l, char** r, Instruction* inst);
void* state_done(char c, char** l, char** r, Instruction* inst);

Instruction parseLine(char* line);

#endif  // PARSE_H
