#ifndef CONTEXT_H
#define CONTEXT_H

#include <stddef.h>

#include "constants.h"
#include "instruction.h"
#include "symbol.h"

typedef struct {
    // --- 執行環境與設定 ---
    int xeMode;         // 是否啟用 XE 模式 (0 或 1)
    size_t lineNumber;  // 原始碼目前的行號

    // --- 組譯器狀態 (State) ---
    size_t locationCounter;  // 目前的 LOCCTR
    size_t startAddress;     // 程式起始位址
    size_t baseAddress;      // BASE 暫存器的位址 (若無則設為特定值或用 flag 判斷)

    // --- 資料結構 (取代原本的 Global Variables) ---
    Instruction* instructions;  // 指令陣列 (stb_ds 動態陣列)
    Symbol* symbolTable;        // 符號表 (stb_ds Hash Table)

    // --- 錯誤處理 ---
    char msgBuffer[MAX_OPERAND_LENGTH];  // 錯誤訊息暫存
    int hasError;                        // 標記組譯過程是否發生錯誤
} Context;

#endif  // CONTEXT_H
