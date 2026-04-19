#ifndef ASSEMBLE_H
#define ASSEMBLE_H

#define STB_DS_IMPLEMENTATION

void errormsg(Instruction* inst, int lineNumber, const char* msg, ...);
FILE* readFile(const char* filename);
int isRegisterName(const char* name);
int getRegisterNumber(const char* regName);
int validateSemantics(Instruction* inst, OpcodeEntry op, int lineNumber);
int getInstructionSize(Instruction* inst, OpcodeEntry op);
int runPass1(FILE* file, int xeMode);
void generateDataCode(Instruction* inst);
void generateByteDataCode(Instruction* inst);
void generateSIC(Instruction* inst, OpcodeEntry op);
void generateFormat1(Instruction* inst, OpcodeEntry op);
void generateFormat2(Instruction* inst, OpcodeEntry op);
int generateFormat3or4(Instruction* inst, OpcodeEntry op, size_t pc, int hasBase, size_t base);
int runPass2(int xeMode);
void writeObjectFile(const char* filename);
void assemble(const char* filename, const char* output, int xeMode);

#endif  // ASSEMBLE_H
