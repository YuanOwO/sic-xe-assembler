#include "opcode.h"

#include <string.h>

/**
 * 指令表
 */
OpcodeEntry OPTAB[] = {
    {"",       OP_NULL,      0,               0                            }, // 沒有東西，index 從 1 開始
    {"ADD",    OP_MACHINE,   CODE_ADD,        FLAG_OP_FORMAT_3             },
    {"ADDF",   OP_MACHINE,   CODE_ADDF,       FLAG_OP_FORMAT_3 | FLAG_OP_XE},
    {"ADDR",   OP_MACHINE,   CODE_ADDR,       FLAG_OP_FORMAT_2             },
    {"AND",    OP_MACHINE,   CODE_AND,        FLAG_OP_FORMAT_3             },
    {"BASE",   OP_DIRECTIVE, CODE_DIR_BASE,   FLAG_OP_XE                   },
    {"BYTE",   OP_DIRECTIVE, CODE_DIR_BYTE,   0                            },
    {"CLEAR",  OP_MACHINE,   CODE_CLEAR,      FLAG_OP_FORMAT_2 | FLAG_OP_XE},
    {"COMP",   OP_MACHINE,   CODE_COMP,       FLAG_OP_FORMAT_3             },
    {"COMPF",  OP_MACHINE,   CODE_COMPF,      FLAG_OP_FORMAT_3 | FLAG_OP_XE},
    {"COMPR",  OP_MACHINE,   CODE_COMPR,      FLAG_OP_FORMAT_2 | FLAG_OP_XE},
    {"DIV",    OP_MACHINE,   CODE_DIV,        FLAG_OP_FORMAT_3             },
    {"DIVF",   OP_MACHINE,   CODE_DIVF,       FLAG_OP_FORMAT_3 | FLAG_OP_XE},
    {"DIVR",   OP_MACHINE,   CODE_DIVR,       FLAG_OP_FORMAT_2 | FLAG_OP_XE},
    {"END",    OP_DIRECTIVE, CODE_DIR_END,    0                            },
    {"FIX",    OP_MACHINE,   CODE_FIX,        FLAG_OP_FORMAT_1 | FLAG_OP_XE},
    {"FLOAT",  OP_MACHINE,   CODE_FLOAT,      FLAG_OP_FORMAT_1 | FLAG_OP_XE},
    {"HIO",    OP_MACHINE,   CODE_HIO,        FLAG_OP_FORMAT_1 | FLAG_OP_XE},
    {"J",      OP_MACHINE,   CODE_J,          FLAG_OP_FORMAT_3             },
    {"JEQ",    OP_MACHINE,   CODE_JEQ,        FLAG_OP_FORMAT_3             },
    {"JGT",    OP_MACHINE,   CODE_JGT,        FLAG_OP_FORMAT_3             },
    {"JLT",    OP_MACHINE,   CODE_JLT,        FLAG_OP_FORMAT_3             },
    {"JSUB",   OP_MACHINE,   CODE_JSUB,       FLAG_OP_FORMAT_3             },
    {"LDA",    OP_MACHINE,   CODE_LDA,        FLAG_OP_FORMAT_3             },
    {"LDB",    OP_MACHINE,   CODE_LDB,        FLAG_OP_FORMAT_3 | FLAG_OP_XE},
    {"LDCH",   OP_MACHINE,   CODE_LDCH,       FLAG_OP_FORMAT_3             },
    {"LDF",    OP_MACHINE,   CODE_LDF,        FLAG_OP_FORMAT_3 | FLAG_OP_XE},
    {"LDL",    OP_MACHINE,   CODE_LDL,        FLAG_OP_FORMAT_3             },
    {"LDS",    OP_MACHINE,   CODE_LDS,        FLAG_OP_FORMAT_3 | FLAG_OP_XE},
    {"LDT",    OP_MACHINE,   CODE_LDT,        FLAG_OP_FORMAT_3 | FLAG_OP_XE},
    {"LDX",    OP_MACHINE,   CODE_LDX,        FLAG_OP_FORMAT_3             },
    {"LPS",    OP_MACHINE,   CODE_LPS,        FLAG_OP_FORMAT_3 | FLAG_OP_XE},
    {"MUL",    OP_MACHINE,   CODE_MUL,        FLAG_OP_FORMAT_3             },
    {"MULF",   OP_MACHINE,   CODE_MULF,       FLAG_OP_FORMAT_3 | FLAG_OP_XE},
    {"MULR",   OP_MACHINE,   CODE_MULR,       FLAG_OP_FORMAT_2 | FLAG_OP_XE},
    {"NOBASE", OP_DIRECTIVE, CODE_DIR_NOBASE, FLAG_OP_XE                   },
    {"NORM",   OP_MACHINE,   CODE_NORM,       FLAG_OP_FORMAT_1 | FLAG_OP_XE},
    {"OR",     OP_MACHINE,   CODE_OR,         FLAG_OP_FORMAT_3             },
    {"RD",     OP_MACHINE,   CODE_RD,         FLAG_OP_FORMAT_3             },
    {"RESB",   OP_DIRECTIVE, CODE_DIR_RESB,   0                            },
    {"RESW",   OP_DIRECTIVE, CODE_DIR_RESW,   0                            },
    {"RMO",    OP_MACHINE,   CODE_RMO,        FLAG_OP_FORMAT_2 | FLAG_OP_XE},
    {"RSUB",   OP_MACHINE,   CODE_RSUB,       FLAG_OP_FORMAT_3             },
    {"SHIFTL", OP_MACHINE,   CODE_SHIFTL,     FLAG_OP_FORMAT_2 | FLAG_OP_XE},
    {"SHIFTR", OP_MACHINE,   CODE_SHIFTR,     FLAG_OP_FORMAT_2 | FLAG_OP_XE},
    {"SIO",    OP_MACHINE,   CODE_SIO,        FLAG_OP_FORMAT_1 | FLAG_OP_XE},
    {"SSK",    OP_MACHINE,   CODE_SSK,        FLAG_OP_FORMAT_3 | FLAG_OP_XE},
    {"STA",    OP_MACHINE,   CODE_STA,        FLAG_OP_FORMAT_3             },
    {"START",  OP_DIRECTIVE, CODE_DIR_START,  0                            },
    {"STB",    OP_MACHINE,   CODE_STB,        FLAG_OP_FORMAT_3 | FLAG_OP_XE},
    {"STCH",   OP_MACHINE,   CODE_STCH,       FLAG_OP_FORMAT_3             },
    {"STF",    OP_MACHINE,   CODE_STF,        FLAG_OP_FORMAT_3 | FLAG_OP_XE},
    {"STI",    OP_MACHINE,   CODE_STI,        FLAG_OP_FORMAT_3 | FLAG_OP_XE},
    {"STL",    OP_MACHINE,   CODE_STL,        FLAG_OP_FORMAT_3             },
    {"STS",    OP_MACHINE,   CODE_STS,        FLAG_OP_FORMAT_3 | FLAG_OP_XE},
    {"STSW",   OP_MACHINE,   CODE_STSW,       FLAG_OP_FORMAT_3             },
    {"STT",    OP_MACHINE,   CODE_STT,        FLAG_OP_FORMAT_3 | FLAG_OP_XE},
    {"STX",    OP_MACHINE,   CODE_STX,        FLAG_OP_FORMAT_3             },
    {"SUB",    OP_MACHINE,   CODE_SUB,        FLAG_OP_FORMAT_3             },
    {"SUBF",   OP_MACHINE,   CODE_SUBF,       FLAG_OP_FORMAT_3 | FLAG_OP_XE},
    {"SUBR",   OP_MACHINE,   CODE_SUBR,       FLAG_OP_FORMAT_2 | FLAG_OP_XE},
    {"SVC",    OP_MACHINE,   CODE_SVC,        FLAG_OP_FORMAT_2             },
    {"TD",     OP_MACHINE,   CODE_TD,         FLAG_OP_FORMAT_3             },
    {"TIO",    OP_MACHINE,   CODE_TIO,        FLAG_OP_FORMAT_1 | FLAG_OP_XE},
    {"TIX",    OP_MACHINE,   CODE_TIX,        FLAG_OP_FORMAT_3             },
    {"TIXR",   OP_MACHINE,   CODE_TIXR,       FLAG_OP_FORMAT_2 | FLAG_OP_XE},
    {"WD",     OP_MACHINE,   CODE_WD,         FLAG_OP_FORMAT_3             },
    {"WORD",   OP_DIRECTIVE, CODE_DIR_WORD,   0                            },
};

const size_t OPTAB_SIZE = sizeof(OPTAB) / sizeof(OpcodeEntry);

/**
 * 搜尋指令的 opcode
 *
 * 由於 OPTAB 是按照助記符字母順序排序的，可以使用二分搜尋來提高搜尋效率。
 *
 * @param mnemonic 指令助記符
 * @return int 指令在 OPTAB 中的索引，找不到則返回 0 (FALSE)
 */
size_t searchOpcode(char* mnemonic) {
    size_t l = 0, r = OPTAB_SIZE - 1;  // 二分搜尋的左右邊界 [l, r]

    while (l <= r) {
        size_t mid = l + (r - l) / 2;
        int cmp = strcmp(mnemonic, OPTAB[mid].mnemonic);
        if (cmp == 0) {
            return mid;  // 找到指令，返回其在 OPTAB 中的索引
        } else if (cmp < 0) {
            r = mid - 1;
        } else {
            l = mid + 1;
        }
    }

    return 0;  // Not found
}

/**
 * 根據索引獲取指令的詳細信息
 *
 * @param index 指令在 OPTAB 中的索引
 * @return OpcodeEntry 指令的詳細信息，如果索引無效則返回空的 OpcodeEntry
 */
OpcodeEntry getOpcode(size_t index) {
    if (index <= 0 || index >= OPTAB_SIZE) {
        return OPTAB[0];  // 返回空的 OpcodeEntry
    }
    return OPTAB[index];
}
