#ifndef ASSEMBLE_H
#define ASSEMBLE_H

#define STB_DS_IMPLEMENTATION

#include <stdio.h>

#include "instruction.h"
#include "opcode.h"

void errormsg(Instruction* inst, int lineNumber, const char* msg, ...);
FILE* readFile(const char* filename);
int isRegisterName(const char* name);
int getRegisterNumber(const char* regName);
int validateSemantics(Instruction* inst, OpcodeEntry op, int lineNumber, int xeMode);
int getInstructionSize(Instruction* inst, OpcodeEntry op);
int runPass1(FILE* file, int xeMode);
int generateWordDataCode(Instruction* inst);
int generateByteDataCode(Instruction* inst);
int generateSIC(Instruction* inst, OpcodeEntry op);
int generateFormat1(Instruction* inst, OpcodeEntry op);
int generateFormat2(Instruction* inst, OpcodeEntry op);
int generateFormat3or4(Instruction* inst, OpcodeEntry op, size_t pc, int hasBase, size_t base);
int runPass2(int xeMode);
int writeObjectFile(const char* filename);
void assemble(const char* filename, const char* output, int xeMode);

#endif  // ASSEMBLE_H
