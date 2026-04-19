#ifndef UTILS_H
#define UTILS_H

#include <ctype.h>

inline int isSymbolChar(char c) {
    return isalnum(c) || c == '_';
}

inline int isSymbolStartChar(char c) {
    return isalpha(c) || c == '_';
}

inline int isEndOfLine(char c) {
    // . 是註解的開始，\0 是行尾，其他的空白字符也算是行尾
    return c == '\0' || c == '\f' || c == '\n' || c == '\r' || c == '\v' || c == '.';
}

inline int isPrefixChar(char c) {
    return c == '#' || c == '@' || c == '=';
}

inline int isEmptyString(const char* str) {
    return str[0] == '\0';
}

#endif  // UTILS_H
