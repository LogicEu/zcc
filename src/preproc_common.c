#include <preproc.h>

char* strrange(const char* str, const range_t range)
{
    static char buffer[0xfff];
    const size_t len = range.end - range.start;
    memcpy(buffer, str + range.start, len);
    buffer[len] = 0;
    return buffer;
}

range_t tokenrange(const range_t* toks, const size_t count, range_t range)
{
    size_t i;
    for (i = 0; i < count && toks[i].start < range.start; ++i);
    for (range.start = i; i < count && toks[i].end <= range.end; ++i);
    range.end = i;
    return range;
}

range_t parenrange(const char* str, const size_t index)
{
    char ch = str[index];
    switch (ch) {
        case '{': 
        case '[': ++ch;
        case '(': ++ch;
    }
    
    size_t i, scope;
    for (i = index + 1, scope = 1; scope && str[i]; ++i) {
        scope += (str[i] == str[index]) - (str[i] == ch);
    }
    return (range_t){index, i};
}