#include "assemble.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>

#include "constants.h"
#include "opcode.h"
#include "parse.h"
#include "stb_ds.h"
#include "symbol.h"
#include "utils.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

char buffer[READ_BUFFER_SIZE];       // 用於暫存文字的緩衝區
char msgBuffer[MAX_OPERAND_LENGTH];  // 第二個緩衝區，用於格式化錯誤訊息
Instruction* instructions = NULL;    // 用於存儲解析後的指令陣列，使用 stb_ds 的動態陣列
Symbol* symbolTable = NULL;          // 符號表，使用 stb_ds 的 hash table

////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * 顯示錯誤訊息
 */
void errormsg(Instruction* inst, int lineNumber, const char* msg, ...) {
    va_list args;
    va_start(args, msg);
    vsnprintf(msgBuffer, MAX_OPERAND_LENGTH, msg, args);
    va_end(args);
    printf("Error on line %d: %s\n", lineNumber, msgBuffer);
    inst->opcodeIndex = 0;
    strncpy(inst->operand1, msgBuffer, MAX_OPERAND_LENGTH - 1);
}

/**
 * 讀取輸入文件
 *
 * @param filename 要讀取的文件名
 * @return FILE* 打開的文件指針，如果打開失敗則返回 NULL
 */
FILE* readFile(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (file == NULL) {
        printf("Failed to open file: %s\n", filename);
        return NULL;
    }
    return file;
}

/**
 * 創建輸出文件
 *
 * @param filename 要創建的文件名
 * @return FILE* 打開的文件指針，如果創建失敗則返回 NULL
 */
FILE* writeFile(const char* filename) {
    FILE* file = fopen(filename, "w");
    if (file == NULL) {
        printf("Failed to create output file: %s\n", filename);
        return NULL;
    }
    return file;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * 判斷一個字串是否是暫存器名稱
 *
 * @param name 要檢查的字串
 * @return int 如果是暫存器名稱則返回 1，否則返回 0
 */
int isRegisterName(const char* name) {
    if (*(name + 1) != '\0') return 0;  // 必須是單字元
    char c = name[0];
    return c == 'A' || c == 'X' || c == 'L' || c == 'B' || c == 'S' || c == 'T' || c == 'F';
}

/**
 * 根據暫存器名稱獲取對應的暫存器編號
 *
 * 不包含 PC 和 SW，因為它們不能作為指令的操作數出現
 *
 * @param regName 暫存器名稱
 * @return int 對應的暫存器編號，如果名稱無效則返回 0xF
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
 *
 * 主要檢查以下幾點：
 * - 標籤名稱是否合法（不能是暫存器名稱）
 * - 假指令的 operand 格式是否合法
 * - 機器指令的 operand 格式是否合法（例如 Format 2 的指令必須是兩個暫存器，RSUB 不能有 operand，等等）
 * - 索引定址的格式是否合法（例如 operand2 必須是 ",X"）
 * - 在 SIC 模式下不允許使用 XE 專用的指令
 *
 * @param inst 要驗證的指令
 * @param op 指令對應的操作碼信息
 * @param lineNumber 當前行號，用於錯誤訊息
 * @param xeMode 是否是 XE 模式
 * @return int 如果語義合法則返回 1，否則返回 0
 */
int validateSemantics(Instruction* inst, OpcodeEntry op, int lineNumber, int xeMode) {
    // 檢查標籤名稱是否合法 (不能是暫存器)
    if (!isEmptyString(inst->label)) {
        if (isRegisterName(inst->label)) {
            errormsg(inst, lineNumber, "Label cannot be a register name: '%s'", inst->label);
            return false;
        }
    }

    int op1IsEmpty = isEmptyString(inst->operand1);
    int op1IsReg = isRegisterName(inst->operand1);
    int op1IsN = inst->flags & FLAG_INST_NUMERIC_OPERAND1;
    int op2IsEmpty = isEmptyString(inst->operand2);
    int op2IsReg = isRegisterName(inst->operand2);
    int op2IsN = inst->flags & FLAG_INST_NUMERIC_OPERAND2;

    // operand1 是數字的話，必須在 0~15 之間才算合法的 Format 2 的 n
    if (op1IsN && (inst->numOperand1 < 0 || inst->numOperand1 > 15)) {
        op1IsN = false;
    }
    // operand2 是數字的話，必須在 0~15 之間才算合法的 Format 2 的 n
    if (op2IsN && (inst->numOperand2 < 0 || inst->numOperand2 > 15)) {
        op2IsN = false;
    }

    // 檢查假指令
    if (op.type == OP_DIRECTIVE) {
        // 通則：
        // - 假指令的 operand1 不能是暫存器名稱，
        // - 假指令的 operand2 必須是空的（因為假指令最多只有一個 operand）
        // - 不能使用 prefix（#, @）
        if (op1IsReg) {
            errormsg(inst, lineNumber, "Directive '%s' operand cannot be a register name: '%s'", op.mnemonic,
                     inst->operand1);
            return false;
        }
        if (!op2IsEmpty) {
            errormsg(inst, lineNumber, "Directive '%s' takes at most one operand", op.mnemonic);
            return false;
        }
        if (inst->flags & (FLAG_INST_IMMEDIATE | FLAG_INST_INDIRECT | FLAG_INST_FORMAT_4)) {
            errormsg(inst, lineNumber, "Directive '%s' operand cannot use prefix (#, @)", op.mnemonic);
            return false;
        }

        switch (op.opcode) {
        case CODE_DIR_BASE:
            // BASE <symbol> （必須有 operand）
            if (op1IsEmpty) {
                errormsg(inst, lineNumber,
                         "Directive 'BASE' requires an operand for the base register label");
                return false;
            }
            break;

        case CODE_DIR_START:
            // START <address> （必須有 operand，且必須是數字，表示起始地址）
            if ((inst->flags & FLAG_INST_NUMERIC_OPERAND1) == 0) {
                errormsg(inst, lineNumber,
                         "Directive 'START' requires a numeric operand for the starting address");
                return false;
            }
            if (xeMode) {
                if (inst->numOperand1 < 0 || inst->numOperand1 > 0xFFFFF) {
                    errormsg(inst, lineNumber,
                             "Directive 'START' operand must be a valid address (0 to 0xFFFFF)");
                    return false;
                }
            } else {
                if (inst->numOperand1 < 0 || inst->numOperand1 > 0x7FFF) {
                    errormsg(inst, lineNumber,
                             "Directive 'START' operand must be a valid address (0 to 0x7FFF) in SIC mode");
                    return false;
                }
            }
            break;

        case CODE_DIR_END:
            // END <symbol> （必須有 operand）
            if (op1IsEmpty) {
                errormsg(inst, lineNumber,
                         "Directive 'END' requires an operand for the first executable instruction's label");
                return false;
            }
            break;

        case CODE_DIR_BYTE:
            if (op1IsEmpty) {
                errormsg(inst, lineNumber, "Directive 'BYTE' requires an operand");
                return false;
            }
            // BYTE 的 operand 格式必須是 C'...' 或 X'...'
            if (inst->operand1[0] != 'C' && inst->operand1[0] != 'X') {
                errormsg(inst, lineNumber, "Directive 'BYTE' operand must start with C or X: '%s'",
                         inst->operand1);
                return false;
            }
            if (inst->operand1[1] != '\'' && inst->operand1[strlen(inst->operand1) - 1] != '\'') {
                errormsg(inst, lineNumber,
                         "Directive 'BYTE' operand must be in the format C'...' or X'...': '%s'",
                         inst->operand1);
                return false;
            }
            break;

        case CODE_DIR_WORD:
            // WORD 的 operand 必須是數字，且必須在 -8388608 ~ 8388607 之間（3 字節有符號整數的範圍）
            if ((inst->flags & FLAG_INST_NUMERIC_OPERAND1) == 0) {
                errormsg(inst, lineNumber,
                         "Directive 'WORD' requires a numeric operand for the constant value");
                return false;
            }
            if (xeMode) {
                if (inst->numOperand1 < -0x800000 || inst->numOperand1 > 0x7FFFFF) {
                    errormsg(inst, lineNumber,
                             "Directive 'WORD' operand must be a valid 3-byte signed integer (-8388608 to "
                             "8388607)");
                    return false;
                }
            } else {
                if (inst->numOperand1 < -0x8000 || inst->numOperand1 > 0x7FFF) {
                    errormsg(inst, lineNumber,
                             "Directive 'WORD' operand must be a valid 3-byte signed integer (-32768 to "
                             "32767) in SIC mode");
                    return false;
                }
            }
            break;

        case CODE_DIR_RESB:
        case CODE_DIR_RESW:
            // RESB 和 RESW 的 operand 必須是數字，且必須在 0 ~ 1048575 之間（最多保留 0xFFFFF 字節）
            if ((inst->flags & FLAG_INST_NUMERIC_OPERAND1) == 0) {
                errormsg(inst, lineNumber,
                         "Directive '%s' requires a numeric operand for the number of bytes/words to reserve",
                         op.mnemonic);
                return false;
            }
            if (xeMode) {
                if (inst->numOperand1 < 0 || inst->numOperand1 > 0xFFFFF) {
                    errormsg(inst, lineNumber,
                             "Directive '%s' operand must be a valid non-negative integer (0 to 0xFFFFF)",
                             op.mnemonic);
                    return false;
                }
            } else {
                if (inst->numOperand1 < 0 || inst->numOperand1 > 0x7FFF) {
                    errormsg(inst, lineNumber,
                             "Directive '%s' operand must be a valid non-negative integer (0 to 32767) in "
                             "SIC mode",
                             op.mnemonic);
                    return false;
                }
            }
            break;

        default:  // 奇怪的假指令
            return false;
        }

        return true;
    }

    // 檢查 Format 1 (不能有 operand)
    if (op.flags & FLAG_OP_FORMAT_1) {
        if (!op1IsEmpty || !op2IsEmpty) {
            errormsg(inst, lineNumber, "Format 1 instruction '%s' takes no operands", op.mnemonic);
            return false;
        } else {
            return true;
        }
    }

    // 檢查 Format 2 (暫存器名稱是否正確)
    if (op.flags & FLAG_OP_FORMAT_2) {
        if (inst->flags & (FLAG_INST_IMMEDIATE | FLAG_INST_INDIRECT | FLAG_INST_FORMAT_4)) {
            errormsg(inst, lineNumber, "Format 2 instruction cannot use prefix ( #, @)");
            return false;
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
                return false;
            } else {
                return true;
            }
        }

        if (op.opcode == CODE_SHIFTL || op.opcode == CODE_SHIFTR) {
            if (!op1IsReg || !op2IsN) {
                errormsg(inst, lineNumber, "Instruction '%s' format is: %s r1, n", op.mnemonic, op.mnemonic);
                return false;
            } else {
                return true;
            }
        }

        if (op.opcode == CODE_SVC) {
            if (!op1IsN || !op2IsEmpty) {
                errormsg(inst, lineNumber, "Instruction '%s' format is: %s n", op.mnemonic, op.mnemonic);
                return false;
            } else {
                return true;
            }
        }

        // 一般的 Format 2 指令必須是兩個暫存器
        if (!op1IsReg || !op2IsReg) {
            errormsg(inst, lineNumber, "Format 2 instruction '%s' requires two register operands",
                     op.mnemonic);
            return false;
        }

        return true;
    }

    // 檢查 Format 3/4 的合法性

    // 檢查 RSUB 特例
    if (op.opcode == CODE_RSUB) {
        if ((!op1IsEmpty || !op2IsEmpty)) {
            errormsg(inst, lineNumber, "Instruction 'RSUB' takes no operands");
            return false;
        }

        return true;
    }

    if (op1IsEmpty) {
        errormsg(inst, lineNumber, "Instruction '%s' requires at least one operand", op.mnemonic);
        return false;
    }

    // 檢查索引定址 (,X) 的合法性
    if (!op2IsEmpty) {
        if ((!op2IsReg || inst->operand2[0] != 'X')) {
            errormsg(inst, lineNumber, "Invalid indexed addressing format: '%s'", inst->operand2);
            return false;
        }
        inst->flags |= FLAG_INST_INDEXED;
    }

    return true;  // 恭喜，全部合法！
}

/**
 * 根據指令和操作碼信息計算指令佔用的位元組數
 *
 * @param inst 指令
 * @param op 指令對應的操作碼信息
 * @return int 指令佔用的位元組數
 */
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
 *
 * - 解析每一行的指令，並將解析結果存儲在 instructions 動態陣列中
 * - 同時建立符號表，將每個標籤和對應的地址存儲在 symbolTable hash table 中
 * - 驗證指令的語義是否合法，並在遇到錯誤時記錄錯誤訊息
 * - 追蹤 START 和 END 指令，確保它們的存在和位置合法
 * - 在 SIC 模式下不允許使用 XE 專用的指令
 * - 檢查 location counter 是否超出最大地址範圍
 *
 * @param file 已經打開的輸入文件指針
 * @param xeMode 是否是 XE 模式
 * @return int 如果遇到錯誤則返回 1，否則返回 0
 */
int runPass1(FILE* file, int xeMode) {
    int hasStarted = false;      // 是否已經遇到 START 指令了
    int hasEnded = false;        // 是否已經遇到 END 指令了
    int hasError = false;        // 是否遇到過錯誤了
    int lineNumber = 0;          // 行號計數器
    size_t locationCounter = 0;  // location counter，初始為 0

    while (fgets(buffer, sizeof(buffer), file) != NULL) {
        lineNumber++;
        Instruction inst = parseLine(buffer);
        OpcodeEntry op = getOpcode(inst.opcodeIndex);

        if (inst.opcodeIndex == 0) {
            if (!isEmptyString(inst.operand1)) {  // 解析失敗，inst.operand1 中存放了錯誤訊息
                printf("Error on line %d: %s\n", lineNumber, inst.operand1);
                hasError = true;
            }
            continue;  // 註解行或空行，跳過
        }

        inst.address = locationCounter;  // 設置指令的地址
        locationCounter += getInstructionSize(&inst, op);

        // 檢查 location counter 是否超出最大地址範圍
        if (xeMode) {
            if (locationCounter > 0xFFFFF) {
                printf("Error on line %d: Location counter exceeded maximum address (0xFFFFF)\n", lineNumber);
                hasError = true;
                continue;
            }
        } else {
            if (locationCounter > 0x7FFF) {
                printf("Error on line %d: Location counter exceeded maximum address (0x7FFF) in SIC mode\n",
                       lineNumber);
                hasError = true;
                continue;
            }
        }

        // 語義驗證
        if (!validateSemantics(&inst, op, lineNumber, xeMode)) {
            hasError = true;
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
            hasError = true;
            continue;
        }

        // 如果這行有 label，則將 label 和地址添加到符號表中
        if (!isEmptyString(inst.label)) {
            if (shgeti(symbolTable, inst.label) != -1) {
                errormsg(&inst, lineNumber, "Duplicate label: '%s'", inst.label);
                hasError = true;
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
        hasError = true;
    }

    if (!hasEnded) {
        printf("Error: Missing END directive\n");
        hasError = true;
    }

    return hasError;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * 根據 WORD 指令的 operand 生成對應的 Object Code
 */
int generateWordDataCode(Instruction* inst) {
    if (inst->flags & FLAG_INST_NUMERIC_OPERAND1) {
        // 直接將數字轉換為 3 字節的十六進位表示
        int value = inst->numOperand1;
        if (value < 0) {
            value += 0x1000000;  // 對於負數，轉換為對應的二補數表示
        }
        snprintf(inst->objcode, MAX_OBJCODE_LENGTH, "%06X", value & 0xFFFFFF);
    } else {
        // operand1 是一個標籤，需要從符號表中查找地址並轉換為 3 字節的十六進位表示
        snprintf(inst->objcode, MAX_OBJCODE_LENGTH, "%06zX", shget(symbolTable, inst->operand1));
    }
    return true;
}

/**
 * 根據 BYTE 指令的 operand 生成對應的 Object Code
 */
int generateByteDataCode(Instruction* inst) {
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
    return true;
}

////////////////////

/**
 * 根據機器指令生成對應的 Object Code（SIC 模式）
 *
 * @param inst 要生成 Object Code 的指令
 * @param op 指令對應的操作碼信息
 */
int generateSIC(Instruction* inst, OpcodeEntry op) {
    uint32_t TA = 0;

    if (op.opcode == CODE_RSUB) {
        TA = 0;  // RSUB 的 TA 固定為 0
    } else {
        int i = shgeti(symbolTable, inst->operand1);
        if (i != -1) {
            TA = symbolTable[i].value;
        } else {
            printf("Pass 2 Error: Undefined symbol '%s'\n", inst->operand1);
            return false;
        }
    }

    TA &= 0x7FFF;  // SIC 模式下地址限制為 15 bits
    uint8_t byte1 = op.opcode;
    uint8_t byte2 = (TA >> 8) & 0xFF;
    uint8_t byte3 = TA & 0xFF;

    byte2 |= (inst->flags & FLAG_INST_INDEXED) << 4;

    snprintf(inst->objcode, MAX_OBJCODE_LENGTH, "%02X%02X%02X", byte1, byte2, byte3);
    return true;
}

////////////////////

/**
 * 根據機器指令生成對應的 Object Code（Format 1）
 */
int generateFormat1(Instruction* inst, OpcodeEntry op) {
    snprintf(inst->objcode, MAX_OBJCODE_LENGTH, "%02zX", op.opcode);
    return true;
}

/**
 * 根據機器指令生成對應的 Object Code（Format 2）
 */
int generateFormat2(Instruction* inst, OpcodeEntry op) {
    int r1 = getRegisterNumber(inst->operand1);
    int r2 = getRegisterNumber(inst->operand2);

    uint8_t byte1 = op.opcode;
    uint8_t byte2 = (r1 << 4) | r2;
    snprintf(inst->objcode, MAX_OBJCODE_LENGTH, "%02X%02X", byte1, byte2);
    return true;
}

/**
 * 根據機器指令生成對應的 Object Code（Format 3 或 Format 4）
 */
int generateFormat3or4(Instruction* inst, OpcodeEntry op, size_t pc, int hasBase, size_t base) {
    // 1. 如果沒有使用 # 或 @ => Simple Mode，ni = 11
    if (!(inst->flags & FLAG_INST_IMMEDIATE) && !(inst->flags & FLAG_INST_INDIRECT)) {
        inst->flags |= (FLAG_INST_IMMEDIATE | FLAG_INST_INDIRECT);
    }

    uint32_t disp = 0, TA = 0;
    int isAbsolute = 0;

    // 2. 決定 Target Address
    if (op.opcode == CODE_RSUB) {
        TA = 0;  // RSUB 的 TA 固定為 0
        isAbsolute = 1;
    } else if (inst->flags & FLAG_INST_NUMERIC_OPERAND1) {
        // 如果 operand1 是純數字，直接使用這個數字作為 TA
        TA = inst->numOperand1;
        isAbsolute = 1;
    } else {
        int i = shgeti(symbolTable, inst->operand1);
        if (i != -1) {
            TA = symbolTable[i].value;
        } else {
            printf("Pass 2 Error: Undefined symbol '%s'\n", inst->operand1);
            return 1;
        }
    }

    // 3. 計算 Displacement 與設定 b, p 旗標
    if (inst->flags & FLAG_INST_FORMAT_4 || isAbsolute) {
        disp = TA;  // Format 4 或絕對地址直接使用 TA 作為位移，b 和 p 旗標都不設置
    } else {
        int pcDisp = TA - pc;
        if (pcDisp >= -2048 && pcDisp <= 2047) {
            inst->flags |= FLAG_INST_PC_RELATIVE;
            disp = pcDisp;
            if (disp < 0) {
                disp += 0x1000;  // 對於負的 PC-Relative 位移，轉換為對應的二補數表示
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

    // 4. 根據 opcode, nixbpe 旗標和 displacement 組合成機器碼
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
 *
 * - 遍歷指令陣列，根據每條指令的類型和格式生成對應的 Object Code
 * - 在生成 Format 3/4 的指令時，根據 PC 和 BASE 的值計算位移，並設定相應的 b 和 p 旗標
 * - 在遇到未定義的符號時報錯，並在遇到其他生成機器碼的錯誤時報錯
 * - 在 SIC 模式下直接生成機器碼，跳過 Format 1/2/3 的判斷，因為 SIC 模式下所有指令都使用相同的格式
 * - 生成的 Object Code 存儲在指令結構體的 objcode 欄位中，供後續寫入物件文件使用
 *
 * @param xeMode 是否是 XE 模式
 * @return int 如果遇到錯誤則返回 1，否則返回 0
 */
int runPass2(int xeMode) {
    int len = arrlen(instructions);
    int hasBase = false;  // 是否有 BASE 指令了
    size_t pc = 0, base = 0;

    for (int i = 0; i < len; i++) {
        Instruction* inst = &instructions[i];
        OpcodeEntry op = getOpcode(inst->opcodeIndex);
        if (i + 1 < len) {
            pc = instructions[i + 1].address;
        }

        if (op.type == OP_DIRECTIVE) {
            if (op.opcode == CODE_DIR_WORD) {
                generateWordDataCode(inst);
                // printf("Generated WORD object code: '%s' for instruction at line %d\n", inst->objcode,
                // i + 1);
            } else if (op.opcode == CODE_DIR_BYTE) {
                generateByteDataCode(inst);
                // printf("Generated BYTE object code: '%s' for instruction at line %d\n", inst->objcode,
                // i + 1);
            } else if (op.opcode == CODE_DIR_BASE) {
                int idx = shgeti(symbolTable, inst->operand1);
                if (idx == -1) {
                    printf("Error: Undefined symbol '%s' for BASE directive\n", inst->operand1);
                    return 1;
                }
                hasBase = true;
                base = symbolTable[idx].value;
            } else if (op.opcode == CODE_DIR_NOBASE) {
                hasBase = false;
            }
        } else if (op.type == OP_MACHINE) {
            if (!xeMode) {
                generateSIC(inst, op);
                continue;  // SIC 模式下直接生成機器碼，跳過後續的格式判斷
            }

            if (op.flags & FLAG_OP_FORMAT_1) {
                generateFormat1(inst, op);
                // printf("Generated Format 1 object code: '%s' for instruction at line %d\n", inst->objcode,
                //        i + 1);
            } else if (op.flags & FLAG_OP_FORMAT_2) {
                generateFormat2(inst, op);
                // printf("Generated Format 2 object code: '%s' for instruction at line %d\n", inst->objcode,
                //        i + 1);
            } else if (op.flags & FLAG_OP_FORMAT_3) {
                int ret = generateFormat3or4(inst, op, pc, hasBase, base);
                if (ret != 0) {
                    printf("Error on line %d: Failed to generate object code for instruction '%s'\n", i + 1,
                           op.mnemonic);
                    return 1;
                }
                // printf("Generated Format 3 object code: '%s' for instruction at line %d\n", inst->objcode,
                //        i + 1);
            } else {
                printf("Error on line %d: Unsupported instruction format for instruction '%s'\n", i + 1,
                       op.mnemonic);
                return 1;
            }
        } else {
            printf("Error on line %d: Invalid opcode type for instruction '%s'\n", i + 1, op.mnemonic);
            return 1;
        }
    }

    return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * 根據指令陣列中的 Object Code 生成物件文件
 *
 * - 寫入 H Record，包含程式名稱、起始地址和程式長度
 * - 根據指令陣列中的 Object Code 生成 T Record，並在遇到 RESB/RESW 指令時斷行
 * - 處理 M Record，記錄所有需要修改的位址（Format 4 且 operand1 是標籤的指令）
 * - 寫入 E Record，包含程式的起始地址
 * - 在寫入過程中處理緩衝區，確保每行 T Record 的 Object Code 不超過 30 Bytes（60 個十六進位字元）
 *
 * @param filename 輸出的物件文件名稱
 * @return int 如果成功生成物件文件則返回 0，否則返回 1
 */
int writeObjectFile(const char* filename) {
    FILE* file = writeFile(filename);
    if (file == NULL) {
        return 1;
    }

    int instCount = arrlen(instructions);
    if (instCount == 0) {
        fclose(file);
        printf("Error: No instructions to write to object file\n");
        return 1;
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
    return 0;
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
