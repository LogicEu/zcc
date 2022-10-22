#ifndef ZCC_STD_H
#define ZCC_STD_H

#include <zstddef.h>

#ifndef ZCC_EXIT_FAILURE
    #define ZCC_EXIT_FAILURE 1
#endif

#ifndef ZCC_EXIT_SUCCESS
    #define ZCC_EXIT_SUCCESS 0
#endif

/* stdlib */
void zexit(int status);
void* zmalloc(size_t size);
void zfree(void* ptr);
long zatol(const char* str);
int zatoi(const char* str);
int zitoa(int num, char* str, const int base);

/*  string */
void* zmemcpy(void* dst, const void* src, size_t n);
int zmemcmp(const void* p1, const void* p2, size_t n);
void* zmemset(void* dst, int val, size_t n);
char* zstrcpy(char* dst, const char* src);
char* zstrcat(char* dst, const char* src);
int zstrcmp(const char* s1, const char* s2);
size_t zstrlen(const char* str);
char* zstrchr(const char* str, const int c);
char* zstrstr(const char* big, const char* small);
char* zstrrev(char* str);

#endif