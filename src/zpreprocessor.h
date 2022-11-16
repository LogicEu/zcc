#ifndef ZCC_PREPROCESSOR_H
#define ZCC_PREPROCESSOR_H

#include <utopia/utopia.h>

map_t zcc_defines_std(void);
int zcc_defines_push(map_t* defines, const char* keystr, const char* valstr);
int zcc_defines_undef(map_t* defines, const char* key);
void zcc_defines_free(map_t* defines, const size_t from);

char* zcc_preprocess_text(char* str, size_t* size);
char* zcc_preprocess_macros(char* src, size_t* size, const map_t* defines, const char** includes);

#endif