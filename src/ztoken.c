#include <zstdlib.h>
#include <zstring.h>
#include <zlexer.h>
#include <ztoken.h>

char* ztokbuf(const struct token* token)
{
    static char buf[0xff];
    zmemcpy(buf, token->str, token->len);
    buf[token->len] = 0;
    return buf;
}

struct token ztokget(const char* start, const char* end, unsigned int type)
{
    struct token token;
    token.str = start;
    token.len = end - start;
    token.type = type;
    return token;
}

struct token ztoknext(const char* str)
{
    unsigned int type;
    const char *tokend, *tokstart = str;
    
    tokend = zlex_next(tokstart, &type);
    if (type == ZTOK_NON) {
        tokstart = tokend;
        tokend = zlex_next(tokstart, &type);
    }
    
    return ztokget(tokstart, tokend, type);
}

struct token ztoknum(const long n)
{
    struct token tok;
    char* str = zmalloc(0xf);
    tok.type = ZTOK_DEF;
    tok.len = zltoa(n, str, 10);
    tok.str = str;
    return tok;
}

struct token ztokstr(const char* str)
{
    char* buf;
    struct token tok;
    tok.type = ZTOK_DEF;
    tok.len = zstrlen(str);
    buf = zmalloc(tok.len + 1);
    zmemcpy(buf, str, tok.len);
    buf[tok.len] = 0;
    tok.str = buf;
    return tok;
}

struct token ztokappend(const struct token* t1, const struct token* t2)
{
    char* buf;
    struct token tok;
    tok.type = ZTOK_DEF;
    tok.len = t1->len + t2->len - 2;
    buf = zmalloc(tok.len + 1);
    zmemcpy(buf, t1->str, t1->len - 1);
    zmemcpy(buf + t1->len - 1, t2->str + 1, t2->len - 1);
    buf[tok.len] = 0;
    tok.str = buf;
    
    if (t1->type == ZTOK_DEF) {
        zfree((void*)(size_t)t1->str);
    }

    return tok;
}
