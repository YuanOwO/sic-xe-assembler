#include "utils.h"

void setFlag(size_t* flags, size_t flag) {
    *flags |= flag;
}

void resetFlag(size_t* flags, size_t flag) {
    *flags &= ~flag;
}

void clearFlags(size_t* flags) {
    *flags = 0;
}
