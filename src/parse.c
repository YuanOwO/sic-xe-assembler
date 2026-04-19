#include "parse.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "opcode.h"
#include "utils.h"

int isInQuote(size_t flags) {
    return flags & FLAG_INST_PARSING_INQUOTE;
}

int isNumericState(size_t flags, int operandNumber) {
    if (operandNumber == 1) {
        return flags & FLAG_INST_NUMERIC_OPERAND1;
    } else if (operandNumber == 2) {
        return flags & FLAG_INST_NUMERIC_OPERAND2;
    }
    return 0;
}

char* get_state_name(void* state) {
    if (state == state_start) return "state_start";
    if (state == state_label_or_mnem) return "state_label_or_mnem";
    if (state == state_wait_mnem) return "state_wait_mnem";
    if (state == state_mnem_prefix) return "state_mnem_prefix";
    if (state == state_mnem) return "state_mnem";
    if (state == state_wait_op1) return "state_wait_op1";
    if (state == state_op1) return "state_op1";
    if (state == state_wait_comma) return "state_wait_comma";
    if (state == state_wait_op2) return "state_wait_op2";
    if (state == state_op2) return "state_op2";
    if (state == state_error) return "state_error";
    if (state == state_done) return "state_done";
    return "unknown_state";
}

////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * 起始狀態，等待 label 或 mnemonic 的開始
 *
 * a-zA-Z_ => 可能是 label 或 mnemonic，轉到 state_label_or_mnem
 * + => Format 4 mnemonic 的開始，轉到 state_mnem
 * . 或 \0 => 註解行或空行，直接轉到 state_done
 */
void* state_start(char c, char** l, char** r, Instruction* inst) {
    if (isblank(c)) {  // 忽略空白
        (*l)++;
        return state_start;
    }

    if (c == '+') {
        inst->flags |= FLAG_INST_FORMAT_4;
        return state_mnem_prefix;
    }

    if (isSymbolStartChar(c)) {  // a-zA-Z_
        return state_label_or_mnem;
    }

    if (isEndOfLine(c)) {  // 該行是註解或空行，直接結束
        return state_done;
    }

    snprintf(inst->operand1, MAX_OPERAND_LENGTH, "Invalid start of line: '%c'", c);
    return state_error;
}

/**
 * label 或 mnemonic 狀態
 *
 * a-zA-Z0-9_ => label 或 mnemonic，繼續讀取
 * 空白或 . 或 \0 => 結束，檢查是 label 還是 mnemonic
 */
void* state_label_or_mnem(char c, char** l, char** r, Instruction* inst) {
    if (isSymbolChar(c)) {  // a-zA-Z0-9_
        return state_label_or_mnem;
    }

    if (isblank(c) || isEndOfLine(c)) {
        **r = '\0';
        size_t op = searchOpcode(*l);

        if (isEndOfLine(c)) {  // 該行已經結束了，必須是 mnemonic
            if (op) {
                inst->opcodeIndex = op;
                return state_done;
            }
            // 無效的 mnemonic
            snprintf(inst->operand1, MAX_OPERAND_LENGTH, "Unrecognized mnemonic: %s", *l);
            return state_error;
        }
        if (op) {  // mnemonic 保留字 => 沒有 label，這部分是 mnemonic
            inst->opcodeIndex = op;
            **r = c;
            return state_wait_op1;
        }
        // 既不是 mnemonic 保留字，也符合 label 的格式 => 這部分是 label
        if (*r - *l >= MAX_LABEL_LENGTH) {  // label 太長了
            snprintf(inst->operand1, MAX_OPERAND_LENGTH, "Label too long: %s", *l);
            **r = c;
            return state_error;
        }
        strncpy(inst->label, *l, MAX_LABEL_LENGTH - 1);
        **r = c;
        return state_wait_mnem;  // 下一部分是 mnemonic
    }

    snprintf(inst->operand1, MAX_OPERAND_LENGTH, "Invalid character in label/mnemonic: '%c'", c);
    return state_error;
}

/**
 * 等待 mnemonic 的狀態
 *
 * 空白 => 跳過
 * a-zA-Z => mnemonic 開始，轉到 state_mnem
 * + => Format 4 mnemonic 的開始，轉到 state_mnem
 * . => 註解 => 沒有 mnemonic => 錯誤
 */
void* state_wait_mnem(char c, char** l, char** r, Instruction* inst) {
    if (isblank(c)) {  // 忽略空白
        (*l)++;
        return state_wait_mnem;
    }

    if (isalpha(c)) {  // a-zA-Z => mnemonic 開始
        return state_mnem;
    }

    if (c == '+') {
        inst->flags |= FLAG_INST_FORMAT_4;
        return state_mnem_prefix;
    }

    if (c == '.') {  // 註解 => 沒有 mnemonic => 錯誤
        snprintf(inst->operand1, MAX_OPERAND_LENGTH, "Missing mnemonic before comment");
        return state_error;
    }

    snprintf(inst->operand1, MAX_OPERAND_LENGTH, "Invalid character before mnemonic: '%c'", c);
    return state_error;
}

void* state_mnem_prefix(char c, char** l, char** r, Instruction* inst) {
    if (isalpha(c)) {  // a-zA-Z => mnemonic 開始
        return state_mnem;
    }

    snprintf(inst->operand1, MAX_OPERAND_LENGTH, "Invalid character before mnemonic: '%c'", c);
    return state_error;
}

/**
 * mnemonic 狀態
 *
 * a-zA-Z => 繼續讀取 mnemonic
 * 空白或 . => 結束，檢查 mnemonic 是否有效
 */
void* state_mnem(char c, char** l, char** r, Instruction* inst) {
    if (isalpha(c)) {  // a-zA-Z (助記符只包含字母)
        return state_mnem;
    }

    if (isblank(c) || isEndOfLine(c)) {  // 結束了，檢查 mnemonic 是否有效
        **r = '\0';
        size_t op = searchOpcode(*l);
        if (op) {
            inst->opcodeIndex = op;
            if (isEndOfLine(c)) {  // 後面是註解
                return state_done;
            } else {
                return state_wait_op1;
            }
        }
        snprintf(inst->operand1, MAX_OPERAND_LENGTH, "Unrecognized mnemonic: %s", *l);
        return state_error;
    }

    snprintf(inst->operand1, MAX_OPERAND_LENGTH, "Invalid character in mnemonic: '%c'", c);
    return state_error;
}

/**
 * 等待 operand1 的狀態
 *
 * 空白 => 跳過
 * a-zA-Z0-9_ => operand1 開始，轉到 state_op1
 * #@= => prefix，轉到 state_op1
 * . => 註解 => 沒有 operand => 直接結束
 */
void* state_wait_op1(char c, char** l, char** r, Instruction* inst) {
    if (isblank(c)) {
        (*l)++;
        return state_wait_op1;
    }

    if (isSymbolStartChar(c)) {  // a-zA-Z_ => operand1 開始
        return state_op1;
    }

    if (isdigit(c)) {
        inst->flags |= FLAG_INST_NUMERIC_OPERAND1;  // operand1 是數字
        return state_op1;
    }

    if (isPrefixChar(c)) {
        if (c == '#') {
            inst->flags |= FLAG_INST_IMMEDIATE;  // '#' => 立即值
        } else if (c == '@') {
            inst->flags |= FLAG_INST_INDIRECT;  // '@' => 間接尋址
        } else {
            // FIXME: 尚未實作字面值
            // inst->flags |= FLAG_INST_LITERAL;  // '=' => 字面值
        }
        return state_op1_prefix;
    }

    if (isEndOfLine(c)) {  // 註解 => 沒有 operand => 直接結束
        return state_done;
    }

    snprintf(inst->operand1, MAX_OPERAND_LENGTH, "Invalid character at start of operand: '%c'", c);
    return state_error;
}

void* state_op1_prefix(char c, char** l, char** r, Instruction* inst) {
    if (isSymbolStartChar(c)) {  // a-zA-Z_ => operand1 開始
        return state_op1;
    } else if (isdigit(c)) {
        inst->flags |= FLAG_INST_NUMERIC_OPERAND1;  // operand1 是數字
        return state_op1;
    } else {
        snprintf(inst->operand1, MAX_OPERAND_LENGTH, "Invalid character at start of operand: '%c'", c);
        return state_error;
    }
}

/**
 * operand1 狀態
 * 合法的 operand1：
 * - LABEL
 * - REGISTER
 * - NUMERIC
 * - C'{any characters}+' (不支援轉義字元)
 * - X'({hexdigit}{hexdigit})+'
 *
 * a-zA-Z0-9_ => 繼續讀取 operand1
 * 空白或 . 或 , => 結束
 */
void* state_op1(char c, char** l, char** r, Instruction* inst) {
    if (isInQuote(inst->flags)) {
        if (c == '\'') {  // 結束引號（尚未支持轉義字元，所以直接遇到下一個單引號就結束）
            inst->flags &= ~FLAG_INST_PARSING_INQUOTE;  // 結束引號
            if (**l == 'X' && (inst->flags & FLAG_INST_PARSING_ODDCHAR)) {
                snprintf(inst->operand1, MAX_OPERAND_LENGTH,
                         "Hexadecimal literal must have even number of digits");
                return state_error;
            }
            return state_op1;
        }
        if (**l == 'X') {
            // X'...' 的情況，允許任何十六進位字元（需要偶數個）
            if (isxdigit(c)) {
                inst->flags ^= FLAG_INST_PARSING_ODDCHAR;
                return state_op1;
            }
            snprintf(inst->operand1, MAX_OPERAND_LENGTH, "Invalid character in hexadecimal literal: '%c'", c);
            return state_error;
        }
        // C'...' 的情況，允許任何字元（尚未支持轉義字元），除了單引號和行尾
        if (isEndOfLine(c) && c != '.') {
            snprintf(inst->operand1, MAX_OPERAND_LENGTH, "Unterminated character literal");
            return state_error;
        }
        return state_op1;
    }

    if (isalnum(c) || c == '_') {  // a-zA-Z0-9_ => 繼續讀取 operand1
        if (isNumericState(inst->flags, 1) && !isxdigit(c)) {
            snprintf(inst->operand1, MAX_OPERAND_LENGTH, "Invalid character in numeric operand: '%c'", c);
            return state_error;
        }

        return state_op1;
    }

    if (c == '\'') {  // 引號，開始讀取 literal
        inst->flags |= FLAG_INST_PARSING_INQUOTE;
        return state_op1;
    }

    if (isblank(c) || isEndOfLine(c) || c == ',') {  // 結束了
        **r = '\0';
        if (isNumericState(inst->flags, 1)) {
            inst->numOperand1 = (int)strtol(*l, NULL, 16);  // 嘗試將 operand1 解析為數字
        } else {
            size_t op = searchOpcode(*l);
            if (op) {  // Operand1 是保留字，這種情況通常是指令格式錯誤了，因為 operand1 不應該是保留字
                snprintf(inst->operand1, MAX_OPERAND_LENGTH, "Invalid operand: %s", *l);
                return state_error;
            }
        }
        strncpy(inst->operand1, *l, MAX_OPERAND_LENGTH - 1);
        **r = c;
        if (isEndOfLine(c)) {
            return state_done;
        } else if (c == ',') {
            return state_wait_op2;
        } else {
            return state_wait_comma;  // operand 後面可能有註解，也可能有第二個 operand
        }
    }

    snprintf(inst->operand1, MAX_OPERAND_LENGTH, "Invalid character in operand: '%c'", c);
    return state_error;
}

/**
 * 等待逗號的狀態
 *
 * 空白 => 跳過
 * , => 逗號，轉到 state_wait_op2
 * . => 註解 => 沒有第二個 operand => 直接結束
 */
void* state_wait_comma(char c, char** l, char** r, Instruction* inst) {
    if (isblank(c)) {
        (*l)++;
        return state_wait_comma;
    }

    if (c == ',') {
        (*l)++;
        return state_wait_op2;
    }

    if (isEndOfLine(c)) {  // 註解 => 沒有第二個 operand => 直接結束
        return state_done;
    }

    snprintf(inst->operand1, MAX_OPERAND_LENGTH, "Invalid character after operand: '%c'", c);
    return state_error;
}

/**
 * 等待 operand2 的狀態
 *
 * 空白 => 跳過
 * a-zA-Z0-9_ => operand2 開始，轉到 state_op2
 * . => 註解 => 沒有 operand2 => 錯誤
 */
void* state_wait_op2(char c, char** l, char** r, Instruction* inst) {
    if (isblank(c)) {
        (*l)++;
        return state_wait_op2;
    }

    if (isSymbolStartChar(c)) {  // a-zA-Z_ => operand2 開始
        return state_op2;
    }

    if (isdigit(c)) {
        inst->flags |= FLAG_INST_NUMERIC_OPERAND2;  // operand2 是數字
        return state_op2;
    }

    if (isEndOfLine(c)) {
        snprintf(inst->operand1, MAX_OPERAND_LENGTH, "Missing second operand before comment");
        return state_error;
    }

    snprintf(inst->operand1, MAX_OPERAND_LENGTH, "Invalid character at start of second operand: '%c'", c);
    return state_error;
}

/**
 * operand2 狀態
 * 合法的 operand2：
 * - REGISTER
 * - NUMERIC
 *
 * a-zA-Z0-9_ => 繼續讀取 operand2
 * 空白或 . 或 , => 結束
 */
void* state_op2(char c, char** l, char** r, Instruction* inst) {
    if (isalnum(c) || c == '_') {  // a-zA-Z0-9_ => 繼續讀取 operand2
        if (isNumericState(inst->flags, 2) && !isxdigit(c)) {
            snprintf(inst->operand1, MAX_OPERAND_LENGTH, "Invalid character in numeric operand: '%c'", c);
            return state_error;
        }

        return state_op2;
    }

    if (isblank(c) || isEndOfLine(c) || c == ',') {  // 結束了
        **r = '\0';
        if (isNumericState(inst->flags, 2)) {
            inst->numOperand2 = (int)strtol(*l, NULL, 16);  // 嘗試將 operand2 解析為數字
        } else {
            size_t op = searchOpcode(*l);
            if (op) {  // Operand2 是保留字，這種情況通常是指令格式錯誤了，因為 operand2 不應該是保留字
                snprintf(inst->operand1, MAX_OPERAND_LENGTH, "Invalid operand: %s", *l);
                return state_error;
            }
        }
        strncpy(inst->operand2, *l, MAX_LABEL_LENGTH - 1);
        **r = c;
        if (isEndOfLine(c)) {
            return state_done;
        } else {
            return state_wait_done;  // operand 後面可能有註解，也可能直接結束
        }
    }

    snprintf(inst->operand1, MAX_OPERAND_LENGTH, "Invalid character in operand: '%c'", c);
    return state_error;
}

void* state_wait_done(char c, char** l, char** r, Instruction* inst) {
    if (isblank(c)) {
        (*l)++;
        return state_wait_done;
    }

    if (isEndOfLine(c)) {  // 註解 => 直接結束
        return state_done;
    }

    snprintf(inst->operand1, MAX_OPERAND_LENGTH, "Invalid character after second operand: '%c'", c);
    return state_error;
}

/**
 * 解析錯誤狀態，直接返回 NULL 表示解析失敗
 */
void* state_error(char c, char** l, char** r, Instruction* inst) {
    // 直接返回 NULL，表示解析失敗
    return NULL;
}

/**
 * 解析完成狀態，直接返回 NULL 表示解析成功
 */
void* state_done(char c, char** l, char** r, Instruction* inst) {
    // 直接返回 NULL，表示解析完成
    return NULL;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

Instruction parseLine(char* line) {
    Instruction inst;
    inst.address = 0;         // 指令的地址
    inst.flags = 0;           // 初始化 flags
    inst.opcodeIndex = 0;     // 初始化 opcodeIndex
    inst.objcode[0] = '\0';   // 初始化 objcode
    inst.label[0] = '\0';     // 初始化 label
    inst.operand1[0] = '\0';  // 初始化 operand1
    inst.operand2[0] = '\0';  // 初始化 operand2

    char *l = line, *r = line;              // token 的左右邊界 [l, r]
    StateFunc current_state = state_start;  // 初始狀態
    StateFunc last_state = current_state;   // 上一個狀態

    while (*r != '\0' && current_state != state_error && current_state != state_done) {
        current_state = (StateFunc)current_state(*r, &l, &r, &inst);
        if (current_state != last_state) {
            // printf("Transitioned to new state: %p\n", current_state);
            last_state = current_state;
            l = r;  // 狀態轉換了，更新左邊界到目前的右邊界
        }

        // 推進右邊界
        r++;
    }

    if (*r == '\0' && current_state != state_done && current_state != state_error) {
        current_state = (StateFunc)current_state('\0', &l, &r, &inst);  // 傳遞 \0 讓最後一個狀態收尾
    }

    // 狀態沒有 accepted
    if (current_state != state_done && current_state != state_error) {
        printf("Unexpected end of line: %s\n", line);
        // printf("Current state: %s\n", get_state_name(current_state));
    }

    if (current_state == state_error) {
        inst.opcodeIndex = 0;  // 解析失敗，將 opcodeIndex 設為 0，並在 operand1 中存放錯誤訊息
        printf("Error parsing line: %s\n", inst.operand1);  // inst.operand1 用於存放錯誤訊息
    }

    return inst;
}
