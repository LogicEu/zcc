#ifndef ZCC_PREPROCESSOR_H
#define ZCC_PREPROCESSOR_H

#include <zstddef.h>

char* zcc_preprocess_text(char* str, size_t* size);
char* zcc_preprocess_macros(char* src, size_t* size, const char** predefs, const char** includes);

#endif