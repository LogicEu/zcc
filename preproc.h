#ifndef EULANG_PREPROC_H
#define EULANG_PREPROC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <utopia/utopia.h>

#define chrspace(c) (((c) == ' ') || ((c) == '\n') || ((c) == '\t'))
#define chrstr(c) (((c) == '\'') || ((c) == '"'))
#define chrparen(c) (((c) == '(') || ((c) == '[') || ((c) == '{')) 
#define chrbetween(c, a, b) (((c) >= (a)) && ((c) <= (b)))
#define chrdigit(c) chrbetween(c, '0', '9')
#define chralpha(c) (chrbetween(c, 'A', 'Z') || chrbetween(c, 'a', 'z') || ((c) == '_'))

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

#define atol(a) zatol(a)
#define memcpy(a, b, n) zmemcpy(a, b, n)
#define memcmp(a, b, n) zmemcmp(a, b, n)
#define strcpy(a, b) zstrcpy(a, b)
#define strcat(a, b) zstrcat(a, b)
#define strcmp(a, b) zstrcmp(a, b)
#define strstr(a, b) zstrstr(a, b)
#define strlen(a) zstrlen(a)

void* zmemcpy(void* dst, const void* src, size_t n);
int zmemcmp(const void* p1, const void* p2, size_t n);
char* zstrcpy(char* dst, const char* src);
char* zstrcat(char* dst, const char* src);
int zstrcmp(const char* s1, const char* s2);
size_t zstrlen(const char* str);
char* zstrstr(const char* big, const char* small);
long zatol(const char* str);

char* strrange(const char* str, const range_t range);
range_t tokenrange(const range_t* toks, const size_t count, range_t range);
range_t parenrange(const char* str, const size_t index);

array_t strlines(string_t* str);
array_t strtoks(const string_t* str);
void strtext(string_t* text, array_t* lines);

ptu_t ptu_read(const char* filename);
void ptu_free(ptu_t* ptu);
void ptu_preprocess(ptu_t* ptu, const array_t* includes);

bnode_t* tree_parse(const char* str, range_t* args, const size_t argcount);
long tree_eval(const bnode_t* root, const char* str);

string_t ppc_read(const char* filename);
void ppc_log(const char* fmt, ...);
void ppc_log_range(const char* str, const range_t range);
void ppc_log_tokrange(const char* str, const range_t* tokens, const range_t range);

#ifdef __cplusplus
}
#endif
#endif