#include <zlexer.h>
#include <zassert.h>

static char* zcc_lexnull(const char* str)
{
    (void)str;
    return NULL;
}

char* zcc_lexline(const char* str)
{    
    while (*str && *str != '\n') {
        ++str;
    }
    return (char*)(size_t)str;
}

char* zcc_lexspace(const char* str)
{
    while (*str && (*str == ' ' || *str == '\t')) {
        ++str;
    }

    if (!*str || *str == '\n' || *str == '\r') {
        return NULL;
    }

    return (char*)(size_t)str;
}

char* zcc_lexstr(const char* str)
{
    zassert(chrstr(*str));
    
    const char c = *str++;
    while (*str && *str != c) {
        str += (*str == '\\') + 1;
    }
    return (char*)(size_t)++str;
}

char* zcc_lexparen(const char* str)
{
    zassert(chrparen(*str));

    char c = *str + 1;
    c += (*str++ != '(');
    while (*str && *str != c) {
        switch (*str) {
            case '(': case '{': case '[': {
                str = zcc_lexparen(str);
                break;
            }
            case '"': case '\'': {
                str = zcc_lexstr(str);
                break;
            }
            default: ++str;
        }
    }
    return (char*)(size_t)++str;
}

char* zcc_lexnum(const char* str)
{
    zassert(chrdigit(*str) || (*str == '.' && chrdigit(str[1])));
    while (*str && (chrdigit(*str) || chralpha(*str) || *str == '.')) {
        str += 1 + ((*str == 'e' || *str == 'E' || *str == 'p' || *str == 'P') && (str[1] == '-' || str[1] == '+'));
    }
    return (char*)(size_t)str;
}

char* zcc_lexid(const char* str)
{
    zassert(chralpha(*str));
    while (*str && (chralpha(*str) || chrdigit(*str))) {
        ++str;
    }
    return (char*)(size_t)str;
}

char* zcc_lexop(const char* str)
{
    zassert(!chralpha(*str) && !chrdigit(*str));
    const char* c = str++;
    switch(*c) {
        case '#':
            str += (*str == *c);
            break;
        case '-':
            if (*str == '>') {
                ++str;
                break;
            }
        case '|':
        case '&':
        case '+':
            if (*str == *c) {
                ++str;
                break;
            }
        case '!':
        case '%':
        case '^':
        case '~':
        case '*':
        case '/':
        case '=':
            str += (*str == '=');
            break;
        case '<':
        case '>':
            str += (*str == *c);
            str += (*str == '=');
    }
    return (char*)(size_t)str;
}

int zcc_lextype(const char* str)
{
    if (chrstr(*str)) {
        return ZTOK_STR;
    }
    
    if (chrdigit(*str) || (*str == '.' && chrdigit(str[1]))) {
        return ZTOK_NUM;
    }
    
    if (chralpha(*str)) {
        return ZTOK_ID;
    }
    
    if (*str >= 32) {
        return ZTOK_SYM;
    }
    
    return ZTOK_NULL;
}

char* zcc_lex(const char* str, size_t* len, int* flag)
{
    static char* (*zlex_funcs[5])(const char*) = {
        &zcc_lexnull,
        &zcc_lexid,
        &zcc_lexnum,
        &zcc_lexop,
        &zcc_lexstr
    };

    if (str) {
        str = zcc_lexspace(str);
    }

    if (!str) {
        return NULL;
    }

    const char* c = str;
    const int type = (zcc_lextype(str) >> 8);

    str = zlex_funcs[type](str);
    *len = str - c;
    *flag = type;
    
    return (char*)(size_t)c;
}

ztok_t ztok_get(const char* str)
{
    ztok_t tok;
    tok.str = zcc_lex(str, &tok.len, &tok.kind);
    return tok;
}

ztok_t ztok_next(ztok_t tok)
{
    tok.str = zcc_lex(tok.str + tok.len, &tok.len, &tok.kind);
    return tok;
}

ztok_t ztok_continue(ztok_t tok, const size_t steps)
{
    size_t i;
    for (i = 0; i < steps; ++i) {
        tok = ztok_next(tok);
    }
    return tok;
}