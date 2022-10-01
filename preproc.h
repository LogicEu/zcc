#ifndef EULANG_PREPROC_H
#define EULANG_PREPROC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <utopia/utopia.h>

typedef struct range_t {
    size_t start;
    size_t end;
} range_t;

typedef struct ptu_t {
    char* name;
    string_t text;
    array_t lines;
    array_t tokens;
} ptu_t;

array_t strlines(string_t* str);
array_t strtoks(const string_t* str);
void strtext(string_t* text, array_t* lines);

ptu_t ptu_read(const char* filename);
void ptu_free(ptu_t* ptu);
void ptu_preprocess(ptu_t* ptu, const array_t* includes);

void ppc_log(const char* fmt, ...);

#ifdef __cplusplus
}
#endif
#endif