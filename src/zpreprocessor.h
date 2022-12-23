#ifndef ZCC_PREPROCESSOR_H
#define ZCC_PREPROCESSOR_H

#include <utopia/utopia.h>

struct map zcc_defines_std(void);
int zcc_defines_push(struct map* defines, const char* keystr, const char* valstr);
int zcc_defines_undef(struct map* defines, const char* key);
void zcc_defines_free(struct map* defines, const size_t from);

char* zcc_preprocess_text(char* str, size_t* size);
char* zcc_preprocess_macros(char* src, size_t* size, const struct map* defines, const char** includes);

#endif