# SIC/XE 組譯器 (SIC/XE Assembler)

## 📝 專案摘要

本專案是一個以 C 語言 (C11 標準) 實作的 **SIC/XE 組合語言組譯器 (Assembler)**。專案採用標準的 Two-Pass (兩次掃描) 演算法設計，負責將 SIC/XE 組合語言原始碼解析並轉換為機器碼，最終輸出包含 Header、Text、Modification 與 End Record 的目的檔 (Object file)。

## 🌟 核心功能 (Features)

- **Two-Pass 組譯流程**：
    - **Pass 1**：解析原始碼並建立符號表 (Symbol Table)，同時計算各指令的記憶體位址 (Location Counter)。
    - **Pass 2**：根據 Pass 1 生成的位址與符號表，將組合語言轉換為機器碼 (Object Code)。
- **FSM 語法解析器 (Finite State Machine)**：
    - 自定義狀態機 (`parse.c`) 來逐行精準解析原始碼。
    - 核心狀態包含：`START` -> `LABEL` -> `MPre` (精準攔截 `+` 號以判定 Format 4 指令) -> `MNEM` -> `OP1` / `OP2`。
    - 能有效過濾多餘空白字元，並具備嚴謹的語法錯誤捕捉機制 (`ERROR` 狀態)。
- **支援完整的 SIC/XE 指令格式與定址模式**：
    - 支援 Format 1, Format 2, Format 3 以及 Format 4 (`+` 前綴) 的指令長度與解析。
    - 支援立即定址 (`#`)、間接定址 (`@`)、索引定址 (`,X`) 的標籤位元處理。
    - 支援 **PC-Relative** 與 **Base-Relative** 相對定址模式的自動位移 (Displacement) 計算。
- **支援多種假指令 (Assembler Directives)**：
    - 完整支援 `START`, `END`, `BYTE`, `WORD`, `RESB`, `RESW`, `BASE` 以及 `NOBASE` 等組譯器指令。

## 📂 檔案結構

- **`main.c`**：程式進入點，負責接收命令列參數 (輸入/輸出檔名)，並啟動組譯程序。
- **`assemble.c`**：組譯器核心邏輯，實作 Pass 1 與 Pass 2 的執行流程，並處理 Object file (H, T, M, E 紀錄) 的格式化輸出。
- **`parse.c`**：詞法分析與語法解析模組，基於 FSM 狀態機處理組合語言原始碼的字串拆解與狀態轉換。
- **`opcode.c`**：包含 SIC/XE 完整的指令表 (`OPTAB`) 定義，並實作二分搜尋法 (Binary Search) 來快速查找對應的 Opcode 與指令格式。
- **`utils.c` / `utils.h`**：提供字串處理與其他輔助工具函式。
- **`CMakeLists.txt`**：CMake 專案設定檔，配置編譯選項 (支援 Debug/Release 模式)。

## 🚀 編譯與執行方式

### 1. 編譯專案

本專案使用 CMake 進行自動化建置。請在專案根目錄下開啟終端機並執行以下指令：

```bash
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```

上述指令將會在 `build/` 目錄下生成可執行檔 `main`。

### 2. 執行組譯器

編譯完成後，您可以透過命令列參數指定執行模式與輸入/輸出檔案：

```bash
# 基本語法
./main [options] <inputfile> [outputfile]
```

**命令列選項 (Options)：**

- **`-h`, `--help`**：顯示幫助訊息並離開程式。
- **`-v`, `--version`**：顯示版本資訊並離開程式 _(偷偷說：這裡藏有開發者的專屬彩蛋！)_。
- **`-x`, `--xe`**：以 SIC/XE 模式進行組譯 **(預設)**。
- **`-s`, `--sic`**：以標準 SIC 模式進行組譯。
- **`<inputfile>`**：欲組譯的組合語言原始碼檔案 (例如 `test.asm`)。
- **`[outputfile]`**：輸出的目的檔名稱 (若未指定，預設將輸出為 `output.obj`)。

**執行範例：**

```bash
# 以預設的 XE 模式組譯，並指定輸出檔名
./main -x ../examples/textbooksicxe.asm my_output.obj

# 以 SIC 模式組譯，使用預設輸出檔名
./main -s ../examples/textbooksic.asm

# 查詢版本與觸發彩蛋
./main --version
```
