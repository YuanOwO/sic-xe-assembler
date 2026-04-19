#include "assemble.h"

#include <stdarg.h>
#include <stdio.h>

#include "constants.h"
#include "opcode.h"
#include "parse.h"
#include "stb_ds.h"
#include "symbol.h"
#include "utils.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

char buffer[READ_BUFFER_SIZE];       // 用於讀取文件的緩衝區
char msgBuffer[MAX_OPERAND_LENGTH];  // 用於格式化錯誤訊息的緩衝區
Instruction* instructions = NULL;    // 用於存儲解析後的指令陣列
Symbol* symbolTable = NULL;          // 符號表，存儲標籤和地址的映射

void errormsg(Instruction* inst, int lineNumber, const char* msg, ...) {
    va_list args;
    va_start(args, msg);
    vsnprintf(msgBuffer, MAX_OPERAND_LENGTH, msg, args);
    va_end(args);
    printf("Error on line %d: %s\n", lineNumber, msgBuffer);
    inst->opcodeIndex = 0;
    strncpy(inst->operand1, msgBuffer, MAX_OPERAND_LENGTH - 1);
}

FILE* readFile(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (file == NULL) {
        printf("Failed to open file: %s\n", filename);
        return NULL;
    }
    return file;
}

int isRegisterName(const char* name) {
    if (*(name + 1) != '\0') return 0;  // 必須是單字元
    char c = name[0];
    return c == 'A' || c == 'X' || c == 'L' || c == 'B' || c == 'S' || c == 'T' || c == 'F';
}

/**
 *  不包含 PC 和 SW 的暫存器編號
 */
int getRegisterNumber(const char* regName) {
    switch (regName[0]) {
    case '\0':
        return 0;  // 空字串表示沒有暫存器
    case 'A':
        return 0;
    case 'X':
        return 1;
    case 'L':
        return 2;
    case 'B':
        return 3;
    case 'S':
        return 4;
    case 'T':
        return 5;
    case 'F':
        return 6;
    }
    return 0xF;  // 無效的暫存器名稱
}

////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * 驗證指令的語義是否合法
 */
int validateSemantics(Instruction* inst, OpcodeEntry op, int lineNumber) {
    // 檢查標籤名稱是否合法 (不能是暫存器)
    if (!isEmptyString(inst->label)) {
        if (isRegisterName(inst->label)) {
            errormsg(inst, lineNumber, "Label cannot be a register name: '%s'", inst->label);
            return 0;
        }
    }

    int op1IsEmpty = isEmptyString(inst->operand1);
    int op1IsReg = isRegisterName(inst->operand1);
    int op1IsN = inst->flags & FLAG_INST_NUMERIC_OPERAND1;
    int op2IsEmpty = isEmptyString(inst->operand2);
    int op2IsReg = isRegisterName(inst->operand2);
    int op2IsN = inst->flags & FLAG_INST_NUMERIC_OPERAND2;

    if (op1IsN && (inst->numOperand1 < 0 || inst->numOperand1 > 15)) {
        op1IsN = 0;  // operand1 是數字，但不在 0-15 的範圍內，當作非法的暫存器名稱處理
    }
    if (op2IsN && (inst->numOperand2 < 0 || inst->numOperand2 > 15)) {
        op2IsN = 0;  // operand2 是數字，但不在 0-15 的範圍內，當作非法的暫存器名稱處理
    }

    // 檢查假指令
    if (op.type == OP_DIRECTIVE) {
        // 通則：假指令的 operand1 不能是暫存器名稱，且不能使用 prefix（#, @, +）
        if (op1IsReg) {
            errormsg(inst, lineNumber, "Directive '%s' operand cannot be a register name: '%s'", op.mnemonic,
                     inst->operand1);
            return 0;
        }
        if (!op2IsEmpty) {
            errormsg(inst, lineNumber, "Directive '%s' takes at most one operand", op.mnemonic);
            return 0;
        }
        if (inst->flags & (FLAG_INST_IMMEDIATE | FLAG_INST_INDIRECT | FLAG_INST_FORMAT_4)) {
            errormsg(inst, lineNumber, "Directive '%s' operand cannot use prefix (+, #, @)", op.mnemonic);
            return 0;
        }

        switch (op.opcode) {
        case CODE_DIR_BASE:
            if (op1IsEmpty) {
                errormsg(inst, lineNumber,
                         "Directive 'BASE' requires an operand for the base register label");
                return 0;
            }
            break;

        case CODE_DIR_START:
            if ((inst->flags & FLAG_INST_NUMERIC_OPERAND1) == 0) {
                errormsg(inst, lineNumber,
                         "Directive 'START' requires a numeric operand for the starting address");
                return 0;
            }
            if (inst->numOperand1 < 0 || inst->numOperand1 > 0xFFFFF) {
                errormsg(inst, lineNumber,
                         "Directive 'START' operand must be a valid address (0 to 0xFFFFF)");
                return 0;
            }
            break;

        case CODE_DIR_END:
            if (op1IsEmpty) {
                errormsg(inst, lineNumber,
                         "Directive 'END' requires an operand for the first executable instruction's label");
                return 0;
            }
            break;

        case CODE_DIR_BYTE:
            if (op1IsEmpty) {
                errormsg(inst, lineNumber, "Directive 'BYTE' requires an operand");
                return 0;
            }
            break;

        case CODE_DIR_WORD:
            if ((inst->flags & FLAG_INST_NUMERIC_OPERAND1) == 0) {
                errormsg(inst, lineNumber,
                         "Directive 'WORD' requires a numeric operand for the constant value");
                return 0;
            }
            if (inst->numOperand1 < -0x800000 || inst->numOperand1 > 0x7FFFFF) {
                errormsg(
                    inst, lineNumber,
                    "Directive 'WORD' operand must be a valid 3-byte signed integer (-8388608 to 8388607)");
                return 0;
            }
            break;

        case CODE_DIR_RESB:
        case CODE_DIR_RESW:
            if ((inst->flags & FLAG_INST_NUMERIC_OPERAND1) == 0) {
                errormsg(inst, lineNumber,
                         "Directive '%s' requires a numeric operand for the number of bytes/words to reserve",
                         op.mnemonic);
                return 0;
            }
            if (inst->numOperand1 < 0 || inst->numOperand1 > 0xFFFFF) {
                errormsg(inst, lineNumber,
                         "Directive '%s' operand must be a valid non-negative integer (0 to 0xFFFFF)",
                         op.mnemonic);
                return 0;
            }
            break;

        default:  // 奇怪的假指令
            return 0;
        }
        return 1;
    }

    // 檢查 Format 1 (不能有 operand)
    if (op.flags & FLAG_OP_FORMAT_1) {
        if (!op1IsEmpty || !op2IsEmpty) {
            errormsg(inst, lineNumber, "Format 1 instruction '%s' takes no operands", op.mnemonic);
            return 0;
        } else {
            return 1;
        }
    }

    // 檢查 Format 2 (暫存器名稱是否正確)
    if (op.flags & FLAG_OP_FORMAT_2) {
        if (inst->flags & (FLAG_INST_IMMEDIATE | FLAG_INST_INDIRECT | FLAG_INST_FORMAT_4)) {
            errormsg(inst, lineNumber, "Format 2 instruction cannot use prefix (+, #, @)");
            return 0;
        }

        // Format 2 的 operand 格式是 R1, R2，必須是有效的暫存器名稱
        // 特例：
        // - CLEAR r1
        // - TIXR r1
        // - SHIFTL r1, n
        // - SHIFTR r1, n
        // - SVC n

        if (op.opcode == CODE_CLEAR || op.opcode == CODE_TIXR) {
            if (!op1IsReg || !op2IsEmpty) {
                errormsg(inst, lineNumber, "Instruction '%s' format is: %s r1", op.mnemonic, op.mnemonic);
                return 0;
            } else {
                return 1;
            }
        }

        if (op.opcode == CODE_SHIFTL || op.opcode == CODE_SHIFTR) {
            if (!op1IsReg || !op2IsN) {
                errormsg(inst, lineNumber, "Instruction '%s' format is: %s r1, n", op.mnemonic, op.mnemonic);
                return 0;
            } else {
                return 1;
            }
        }

        if (op.opcode == CODE_SVC) {
            if (!op1IsN || !op2IsEmpty) {
                errormsg(inst, lineNumber, "Instruction '%s' format is: %s n", op.mnemonic, op.mnemonic);
                return 0;
            } else {
                return 1;
            }
        }

        // 一般的 Format 2 指令必須是兩個暫存器
        if (op1IsReg && op2IsReg) {
            return 1;
        }

        errormsg(inst, lineNumber, "Format 2 instruction '%s' requires two register operands", op.mnemonic);
        return 0;
    }

    // 檢查 RSUB 特例
    if (op.opcode == CODE_RSUB) {
        if ((!op1IsEmpty || !op2IsEmpty)) {
            errormsg(inst, lineNumber, "RSUB instruction takes no operands");
            return 0;
        }

        return 1;
    }

    if (op1IsEmpty) {
        errormsg(inst, lineNumber, "Instruction '%s' requires at least one operand", op.mnemonic);
        return 0;
    }

    // 檢查索引定址 (,X) 的合法性
    if (!op2IsEmpty) {
        if ((!op2IsReg || inst->operand2[0] != 'X')) {
            errormsg(inst, lineNumber, "Invalid indexed addressing format: '%s'", inst->operand2);
            return 0;
        }
        inst->flags |= FLAG_INST_INDEXED;
    }

    return 1;  // 恭喜，全部合法！
}

int getInstructionSize(Instruction* inst, OpcodeEntry op) {
    if (op.type == OP_DIRECTIVE) {
        switch (op.opcode) {
        case CODE_DIR_START:
        case CODE_DIR_END:
            return 0;
        case CODE_DIR_BYTE:
            if (inst->operand1[0] == 'C') {
                return strlen(inst->operand1) - 3;  // C'...' 中的字元數量
            } else if (inst->operand1[0] == 'X') {
                return (strlen(inst->operand1) - 3) / 2;  // X'...' 中的十六進位字元數量的一半
            }
            break;
        case CODE_DIR_WORD:
            return 3;
        case CODE_DIR_RESB:
            return atoi(inst->operand1);
        case CODE_DIR_RESW:
            return 3 * atoi(inst->operand1);
        default:
            return 0;
        }
    } else if (op.type == OP_MACHINE) {
        if (inst->flags & FLAG_INST_FORMAT_4) {
            return 4;
        } else if (op.flags & FLAG_OP_FORMAT_1) {
            return 1;
        } else if (op.flags & FLAG_OP_FORMAT_2) {
            return 2;
        } else if (op.flags & FLAG_OP_FORMAT_3) {
            return 3;
        }
    }
    return 0;  // 不佔用空間（例如解析失敗的指令）
}

////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * PASS 1: 讀取文件並解析每一行，建立指令陣列和符號表
 */
int runPass1(FILE* file, int xeMode) {
    int hasStarted = 0;          // 是否已經遇到 START 指令了
    int hasEnded = 0;            // 是否已經遇到 END 指令了
    int hasError = 0;            // 是否遇到過錯誤了
    int lineNumber = 0;          // 行號計數器
    size_t locationCounter = 0;  // location counter，初始為 0

    while (fgets(buffer, sizeof(buffer), file) != NULL) {
        lineNumber++;
        Instruction inst = parseLine(buffer);
        OpcodeEntry op = getOpcode(inst.opcodeIndex);

        if (inst.opcodeIndex == 0) {
            if (!isEmptyString(inst.operand1)) {  // 解析失敗，inst.operand1 中存放了錯誤訊息
                printf("Error on line %d: %s\n", lineNumber, inst.operand1);
                hasError = 1;
            }
            continue;  // 註解行或空行，跳過
        }

        inst.address = locationCounter;  // 設置指令的地址
        locationCounter += getInstructionSize(&inst, op);

        if (!validateSemantics(&inst, op, lineNumber)) {
            printf("Semantic validation failed for line %d, skipping.\n", lineNumber);
            hasError = 1;
            continue;  // 語義不合法，錯誤訊息已經在 validateSemantics 中設置好了，直接跳過
        }

        if (op.type == OP_DIRECTIVE) {
            if (op.opcode == CODE_DIR_START) {
                locationCounter = inst.numOperand1;  // START 指令會重置 location counter
                inst.address = locationCounter;      // START 指令的地址就是 location counter 的初始值
                hasStarted = lineNumber;             // 記錄 START 指令所在的行號
            } else if (op.opcode == CODE_DIR_END) {
                hasEnded = lineNumber;  // 記錄 END 指令所在的行號
            }
        }

        if (!xeMode && (op.flags & FLAG_OP_XE)) {
            errormsg(&inst, lineNumber, "Instruction '%s' is not supported in SIC mode", op.mnemonic);
            hasError = 1;
            continue;
        }

        // 如果這行有 label，則將 label 和地址添加到符號表中
        if (!isEmptyString(inst.label)) {
            if (shgeti(symbolTable, inst.label) != -1) {
                errormsg(&inst, lineNumber, "Duplicate label: '%s'", inst.label);
                hasError = 1;
                continue;
            } else {
                shput(symbolTable, strdup(inst.label), inst.address);
            }
        }

        arrput(instructions, inst);  // 將解析結果添加到指令陣列中
    }

    fclose(file);

    if (!hasStarted) {
        printf("Error: Missing START directive\n");
        hasError = 1;
    }

    if (!hasEnded) {
        printf("Error: Missing END directive\n");
        hasError = 1;
    }

    return hasError;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * 根據指令和操作碼資訊生成機器碼，並存儲在 inst->objcode 中
 *
 * WORD: 3 bytes
 */
void generateDataCode(Instruction* inst) {
    if (inst->flags & FLAG_INST_NUMERIC_OPERAND1) {
        // 直接將數字轉換為 3 字節的十六進位表示
        int value = inst->numOperand1;
        if (value < 0) {
            value += 0x1000000;  // 對於負數，轉換為對應的正數表示
        }
        snprintf(inst->objcode, MAX_OBJCODE_LENGTH, "%06X", value & 0xFFFFFF);
    } else {
        // operand1 是一個標籤，需要從符號表中查找地址並轉換為 3 字節的十六進位表示
        snprintf(inst->objcode, MAX_OBJCODE_LENGTH, "%06zX", shget(symbolTable, inst->operand1));
    }
}

void generateByteDataCode(Instruction* inst) {
    char *now = msgBuffer, *ptr = inst->operand1 + 2;
    if (inst->operand1[0] == 'C') {
        while (*ptr && *ptr != '\'') {
            snprintf(now, 3, "%02X", (uint8_t)*ptr);  // 將字元轉換為兩位十六進位表示
            ptr++;
            now += 2;
        }
    } else if (inst->operand1[0] == 'X') {
        while (*ptr && *ptr != '\'') {
            snprintf(now, 3, "%c", *ptr);  // 直接複製十六進位字元
            ptr++;
            now += 1;
        }
    }
    strncpy(inst->objcode, msgBuffer, MAX_OBJCODE_LENGTH - 1);
}

////////////////////

void generateSIC(Instruction* inst, OpcodeEntry op) {
    int TA;
    if (op.opcode == CODE_RSUB) {
        TA = 0;  // RSUB 的 TA 固定為 0
    } else {
        TA = shget(symbolTable, inst->operand1);  // SIC 指令的 TA 直接從符號表中查找
    }

    uint8_t byte1 = op.opcode;
    uint8_t byte2 = (TA >> 8) & 0xFF;
    uint8_t byte3 = TA & 0xFF;

    byte2 |= (inst->flags & FLAG_INST_INDEXED) << 4;

    snprintf(inst->objcode, MAX_OBJCODE_LENGTH, "%02X%02X%02X", byte1, byte2, byte3);
}

////////////////////

void generateFormat1(Instruction* inst, OpcodeEntry op) {
    snprintf(inst->objcode, MAX_OBJCODE_LENGTH, "%02zX", op.opcode);
}

void generateFormat2(Instruction* inst, OpcodeEntry op) {
    int r1 = getRegisterNumber(inst->operand1);
    int r2 = getRegisterNumber(inst->operand2);

    uint8_t byte1 = op.opcode;
    uint8_t byte2 = (r1 << 4) | r2;
    snprintf(inst->objcode, MAX_OBJCODE_LENGTH, "%02X%02X", byte1, byte2);
}

int generateFormat3or4(Instruction* inst, OpcodeEntry op, size_t pc, int hasBase, size_t base) {
    int disp = 0, TA = 0;
    int isAbsolute = 0;

    // 1. 確保 n 和 i 至少有一個是 1 (處理 Simple Addressing)
    if (!(inst->flags & FLAG_INST_IMMEDIATE) && !(inst->flags & FLAG_INST_INDIRECT)) {
        inst->flags |= (FLAG_INST_IMMEDIATE | FLAG_INST_INDIRECT);
    }

    // 2. 決定 Target Address (TA)
    if (op.opcode == CODE_RSUB) {
        TA = 0;  // RSUB 的 TA 固定為 0
        isAbsolute = 1;
    } else if (inst->flags & FLAG_INST_NUMERIC_OPERAND1) {
        TA = inst->numOperand1;  // 純數字 Operand
        isAbsolute = 1;
    } else {
        int symIndex = shgeti(symbolTable, inst->operand1);
        if (symIndex != -1) {
            TA = symbolTable[symIndex].value;
        } else {
            printf("Pass 2 Error: Undefined symbol '%s'\n", inst->operand1);
            return 1;
        }
    }

    // 3. 計算 Displacement 與設定 b, p 旗標
    if (inst->flags & FLAG_INST_FORMAT_4) {
        disp = TA;  // Format 4 不需要 Relative
    } else {
        if (isAbsolute) {
            disp = TA;  // 絕對數值不需要 Relative
        } else {
            int pcDisp = TA - pc;
            if (pcDisp >= -2048 && pcDisp <= 2047) {
                inst->flags |= FLAG_INST_PC_RELATIVE;
                disp = pcDisp;
                if (disp < 0) {
                    disp += 0x1000;  // 對於負的 PC-Relative 位移，轉換為對應的正數表示
                }
            } else if (hasBase) {
                int baseDisp = TA - base;
                if (baseDisp >= 0 && baseDisp <= 4095) {
                    inst->flags |= FLAG_INST_BASE_RELATIVE;
                    disp = baseDisp;
                } else {
                    printf("Error: Base-Relative Displacement out of bounds\n");
                    return 1;
                }
            } else {
                printf("Error: PC-Relative out of bounds and BASE not set\n");
                return 1;
            }
        }
    }

    // 4. 根據 opcode、n、i、x、b、p、e 旗標和 displacement 組合成機器碼
    uint8_t byte1 = op.opcode | (inst->flags >> 4 & 0x3);
    uint8_t byte2 = ((inst->flags & 0xF) << 4);
    uint8_t byte3 = 0, byte4 = 0;

    if (inst->flags & FLAG_INST_FORMAT_4) {
        disp &= 0xFFFFF;  // 限制為 20 bits
        byte2 |= ((disp >> 16) & 0x0F);
        byte3 = (disp >> 8) & 0xFF;
        byte4 = disp & 0xFF;
        sprintf(inst->objcode, "%02X%02X%02X%02X", byte1, byte2, byte3, byte4);
    } else {
        disp &= 0xFFF;  // 限制為 12 bits
        byte2 |= ((disp >> 8) & 0x0F);
        byte3 = disp & 0xFF;
        sprintf(inst->objcode, "%02X%02X%02X", byte1, byte2, byte3);
    }

    return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * PASS 2: 根據解析結果生成機器碼，並處理符號表中的標籤地址替換
 */
int runPass2(int xeMode) {
    int len = arrlen(instructions);
    int hasBase = 0;  // 是否有 BASE 指令了
    size_t pc = 0, base = 0;

    // printf("\n\n\nParsed %d instructions:\n", len);

    for (int i = 0; i < len; i++) {
        Instruction* inst = &instructions[i];
        OpcodeEntry op = getOpcode(inst->opcodeIndex);
        if (i + 1 < len) {
            pc = instructions[i + 1].address;
        }

        // printf(
        //     "\nInstruction %d: address=%zx, flags=%zx, label='%s', mnemonic='%s', "
        //     "operand1='%s', operand2='%s'\n",
        //     i + 1, inst->address, inst->flags, inst->label, op.mnemonic, inst->operand1, inst->operand2);

        if (op.type == OP_DIRECTIVE) {
            if (op.opcode == CODE_DIR_WORD) {
                generateDataCode(inst);
                // printf("Generated WORD object code: '%s' for instruction at line %d\n", inst->objcode, i +
                // 1);
            } else if (op.opcode == CODE_DIR_BYTE) {
                generateByteDataCode(inst);
                // printf("Generated BYTE object code: '%s' for instruction at line %d\n", inst->objcode, i +
                // 1);
            } else if (op.opcode == CODE_DIR_BASE) {
                int idx = shgeti(symbolTable, inst->operand1);
                if (idx == -1) {
                    printf("Error: Undefined symbol '%s' for BASE directive\n", inst->operand1);
                    return 1;
                }
                hasBase = 1;
                base = symbolTable[idx].value;
            } else if (op.opcode == CODE_DIR_NOBASE) {
                hasBase = 0;
            }
        } else if (op.type == OP_MACHINE) {
            if (!xeMode) {
                generateSIC(inst, op);
                continue;  // SIC 模式下直接生成機器碼，跳過後續的格式判斷
            }

            if (op.flags & FLAG_OP_FORMAT_1) {
                generateFormat1(inst, op);
                // printf("Generated Format 1 object code: '%s' for instruction at line %d\n", inst->objcode,
                //    i + 1);
            } else if (op.flags & FLAG_OP_FORMAT_2) {
                generateFormat2(inst, op);
                // printf("Generated Format 2 object code: '%s' for instruction at line %d\n", inst->objcode,
                //        i + 1);
            } else if (op.flags & FLAG_OP_FORMAT_3) {
                int ret = generateFormat3or4(inst, op, pc, hasBase, base);
                if (ret != 0) {
                    printf("Failed to generate object code for instruction at line %d\n", i + 1);
                    return 1;
                }
                // printf("Generated Format 3 object code: '%s' for instruction at line %d\n", inst->objcode,
                //        i + 1);
            } else {
                printf("Unknown instruction format for instruction at line %d\n", i + 1);
                return 1;
            }
        } else {
            printf("Unknown opcode type for instruction at line %d\n", i + 1);
            return 1;
        }
    }

    return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void writeObjectFile(const char* filename) {
    FILE* file = fopen(filename, "w");
    if (!file) {
        printf("Failed to create object file.\n");
        return;
    }

    int instCount = arrlen(instructions);
    if (instCount == 0) {
        fclose(file);
        return;
    }

    // --- 1. 寫入 H Record ---
    size_t startAddress = instructions[0].address;
    size_t programLength = instructions[instCount - 1].address - startAddress;
    char progName[7] = "      ";
    if (!isEmptyString(instructions[0].label)) {
        strncpy(progName, instructions[0].label, 6);
    }
    fprintf(file, "H%-6s%06zX%06zX\n", progName, startAddress, programLength);

    size_t* m_records = NULL;

    // --- 2. 處理 T Record ---
    char* now = buffer;
    size_t startAddr = -1;

    for (int i = 0; i < instCount; i++) {
        Instruction* inst = &instructions[i];
        OpcodeEntry op = getOpcode(inst->opcodeIndex);

        // 遇到保留記憶體 (RESB / RESW)，強迫輸出並斷行
        if (op.opcode == CODE_DIR_RESB || op.opcode == CODE_DIR_RESW) {
            if (now > buffer) {
                *now = '\0';  // 確保字串結尾
                fprintf(file, "T%06zX%02zX%s\n", startAddr, (now - buffer) / 2, buffer);
                now = buffer;    // 重置緩衝區指標
                startAddr = -1;  // 標記為需要新的起始位址
            }
            continue;
        }

        if (isEmptyString(inst->objcode)) continue;

        // 設定這行 T Record 的起始位址
        if (startAddr == (size_t)-1) {
            startAddr = inst->address;
        }

        // 將 inst->objcode 分塊寫入緩衝區，每塊最多 60 個字元 (30 Bytes)，然後輸出 T Record
        int code_left = strlen(inst->objcode);
        char* ptr = inst->objcode;

        while (code_left > 0) {
            int current_len = now - buffer;
            int space_left = 60 - current_len;

            // 決定這次要切多大塊 (取剩餘機碼和剩餘空間的「最小值」)
            int chunk_size = (code_left < space_left) ? code_left : space_left;

            // 使用 memcpy 進行高速記憶體拷貝
            memcpy(now, ptr, chunk_size);

            now += chunk_size;
            ptr += chunk_size;
            code_left -= chunk_size;

            // 如果緩衝區剛好滿了 60 個字元 (30 Bytes)，直接印出
            if (now - buffer == 60) {
                *now = '\0';
                fprintf(file, "T%06zX1E%s\n", startAddr, buffer);
                now = buffer;     // 清空緩衝區
                startAddr += 30;  // 記憶體是連續的，下一個起始位址直接 +30
            }
        }

        // 紀錄 M Record 的位址 (Format 4 且 operand1 是標籤的指令)
        if ((inst->flags & FLAG_INST_FORMAT_4) && !(inst->flags & FLAG_INST_NUMERIC_OPERAND1)) {
            // M Record 的位址是指向 TA 的位置，也就是 opcode 後面第一個位元組的位址
            arrput(m_records, inst->address + 1);
        }
    }

    // 迴圈結束，把剩下的尾數印出來 (如果 buffer 裡還有東西的話)
    if (now > buffer) {
        *now = '\0';
        fprintf(file, "T%06zX%02zX%s\n", startAddr, (now - buffer) / 2, buffer);
    }

    // --- 3. 處理 M Record ---
    for (size_t i = 0; i < arrlen(m_records); i++) {
        fprintf(file, "M%06zX05\n", m_records[i]);
    }
    arrfree(m_records);

    // --- 4. 寫入 E Record ---
    fprintf(file, "E%06zX\n", startAddress);

    fclose(file);
    printf("Object file '%s' generated successfully!\n", filename);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void assemble(const char* filename, const char* output, int xeMode) {
    int ret;

    FILE* file = readFile(filename);

    if (file == NULL) {
        return;
    }

    ret = runPass1(file, xeMode);  // 執行 PASS 1，建立指令陣列和符號表
    fclose(file);

    if (ret != 0) {
        printf("Assembly failed during Pass 1.\n");
    } else {
        ret = runPass2(xeMode);  // 執行 PASS 2，生成機器碼

        if (ret != 0) {
            printf("Assembly failed during Pass 2.\n");
        } else {
            printf("Assembly completed successfully.\n");
            writeObjectFile(output);  // 寫入物件檔
        }
    }

    ////////////////////////////////////////////////////////////

    // 清理資源
    for (size_t i = 0; i < shlen(symbolTable); i++) {
        free(symbolTable[i].key);  // 釋放 strdup 分配的字串
    }
    arrfree(instructions);
    shfree(symbolTable);
}
