#include <preproc.h>

void* zmemcpy(void* dst, const void* src, size_t n)
{
    unsigned char* a = dst;
    const unsigned char* b = src;
    while (n) {
        *a = *b;
        ++a, ++b, --n;
    }
    return dst;
}

int zmemcmp(const void* p1, const void* p2, size_t n)
{
    const unsigned char* a = p1, *b = p2;
    while (n && *a == *b) {
        ++a, ++b, --n;
    }
    return n ? (*a - *b) : 0;
}

char* zstrcpy(char* dst, const char* src)
{
    unsigned char* a = (unsigned char*)dst;
    const unsigned char* b = (unsigned char*)src;
    while (*b) {
        *a = *b;
        ++a, ++b;
    }
    *a = 0;
    return dst;
}

char* zstrcat(char* dst, const char* src)
{
    unsigned char* a = (unsigned char*)dst;
    const unsigned char* b = (unsigned char*)src;
    while (*a) {
        ++a;
    }

    while (*b) {
        *a = *b;
        ++a, ++b;
    }
    
    *a = 0;
    return dst;
}

int zstrcmp(const char* s1, const char* s2)
{
    const unsigned char* a = (unsigned char*)s1;
    const unsigned char* b = (unsigned char*)s2;
    while (*a && *a == *b) {
        ++a, ++b;
    }
    return *a - *b;
}

long zatol(const char* str)
{
    long i, j, n;
    for (i = 0, n = 0; chrdigit(str[i]); ++i);
    for (j = 10, --i; i >= 0; --i) {
        n += (str[i] - '0') * j;
        j *= 10;
    }
    return n;
}

size_t zstrlen(const char* str)
{
    size_t i;
    for (i = 0; str[i]; ++i);
    return i;
}

char* zstrstr(const char* big, const char* small)
{
    size_t i, j, n;
    for (i = 0; big[i]; ++i) {
        for (j = n = 0; small[j]; ++j) {
            if (big[i + j] != small[j]) {
                ++n;
                break;
            }
        }
        if (!n) {
            return (char*)(size_t)big + i;
        }
    }
    return NULL;
}