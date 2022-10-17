#ifndef ZCC_STD_H
#define ZCC_STD_H

#include <zstddef.h>

/* stdlib */
void* zmalloc(size_t size);
void zfree(void* ptr);
long zatol(const char* str);

/*  string */
void* zmemcpy(void* dst, const void* src, size_t n);
int zmemcmp(const void* p1, const void* p2, size_t n);
char* zstrcpy(char* dst, const char* src);
char* zstrcat(char* dst, const char* src);
int zstrcmp(const char* s1, const char* s2);
size_t zstrlen(const char* str);
char* zstrchr(const char* str, const int c);
char* zstrstr(const char* big, const char* small);

#endif