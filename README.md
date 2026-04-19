# SIC/XE 組譯器 (SIC/XE Assembler)

## 📝 專案摘要

本專案是一個以 C 語言 (C11 標準) 實作的 **SIC/XE 組合語言組譯器 (Assembler)**。專案採用標準的 Two-Pass (兩次掃描) 演算法設計，負責將 SIC/XE 組合語言原始碼解析並轉換為機器碼，最終輸出包含 Header、Text、Modification 與 End Record 的目的檔 (Object file)。

## 🌟 核心功能 (Features)

- **Two-Pass 組譯流程**：
    - **Pass 1**：解析原始碼並建立符號表 (Symbol Table)，同時計算各指令的記憶體位址 (Location Counter)。
    - **Pass 2**：根據 Pass 1 生成的位址與符號表，將組合語言轉換為機器碼 (Object Code)。
- **FSM 語法解析器**：自定義「有限狀態機 (Finite State Machine)」來逐行解析標籤 (Label)、助記符 (Mnemonic) 與運算元 (Operands)，並能有效捕捉語法錯誤。
- **支援完整的 SIC/XE 指令格式與定址**：
    - 支援 Format 1, Format 2, Format 3 以及 Format 4 (`+` 前綴) 的指令長度。
    - 支援立即定址 (`#`)、間接定址 (`@`)、索引定址 (`,X`)。
    - 支援 PC-Relative 與 Base-Relative 相對定址模式的自動位移 (Displacement) 計算。
- **支援多種假指令 (Assembler Directives)**：支援 `START`, `END`, `BYTE`, `WORD`, `RESB`, `RESW`, `BASE` 以及 `NOBASE` 等組譯器指令。

## 📂 檔案結構

- **`main.c`**：程式進入點，負責接收命令列參數 (輸入的組合語言檔名與輸出的目的檔名)，並啟動組譯程序。
- **`assemble.c`**：組譯器核心邏輯，實作 Pass 1 與 Pass 2 的執行流程，並處理 Object file (H, T, M, E 紀錄) 的格式化輸出。
- **`parse.c`**：詞法分析與語法解析模組，負責處理組合語言原始碼的字串解析與狀態轉換。
- **`opcode.c`**：包含 SIC/XE 完整的指令表 (`OPTAB`) 定義，並提供二分搜尋法來快速查找對應的 Opcode 與指令格式。
- **`utils.c` / `utils.h`**：提供字串處理與其他輔助工具函式。
- **`CMakeLists.txt`**：CMake 專案設定檔，配置編譯選項 (包含 Debug/Release 模式設定)。

## 🚀 編譯與執行方式

### 1. 編譯專案

本專案使用 CMake 進行建置。請在專案根目錄下執行以下指令：

```bash
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```

### 2. 執行組譯器

編譯完成後，會產生名為 `main` 的執行檔。可以透過命令列參數指定要組譯的檔案與輸出的檔案名稱：

```bash
# 語法： ./main [輸入的 .asm 檔案] [輸出的 .obj 檔案]
./main ../examples/textbooksicxe.asm output.obj
```

_(若不指定參數，程式將會使用預設的檔案路徑與名稱進行組譯)_

## 🛠 技術依賴 (Dependencies)

- **`stb_ds.h`**：專案內部採用了這個單頭檔 (Single-header) C 函式庫來實作動態陣列 (Dynamic Array) 與雜湊表 (Hash Table)，大幅提升了符號表存取的效能與方便性。
