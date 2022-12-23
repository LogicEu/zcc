#include <zlexer.h>
#include <zstring.h>
#include <zdbg.h>

/* Basic composable lexing / tokenizing functions */

static char* zcc_lexnull(const char* str)
{
    (void)str;
    return NULL;
}

char* zcc_lexline(const char* str)
{    
    while (*str && *str != '\n' && *str != '\r') {
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
    zassert(_isstr(*str));
    
    const char c = *str++;
    while (*str && *str != c) {
        str += (*str == '\\') + 1;
    }
    return (char*)(size_t)++str;
}

char* zcc_lexparen(const char* str)
{
    zassert(_isparen(*str));

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
    return (char*)(size_t)str + !!*str;
}

char* zcc_lexnum(const char* str)
{
    zassert(_isdigit(*str) || (*str == '.' && _isdigit(str[1])));
    while (*str && (_isdigit(*str) || _isid(*str) || *str == '.')) {
        str += 1 + ((*str == 'e' || *str == 'E' || *str == 'p' || *str == 'P') && (str[1] == '-' || str[1] == '+'));
    }
    return (char*)(size_t)str;
}

char* zcc_lexid(const char* str)
{
    zassert(_isid(*str));
    while (*str && (_isid(*str) || _isdigit(*str))) {
        ++str;
    }
    return (char*)(size_t)str;
}

char* zcc_lexop(const char* str)
{
    zassert(!_isid(*str) && !_isdigit(*str));
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
        case '.':
            str += 2 * (str[0] == *c && str[1] == *c);
    }
    return (char*)(size_t)str;
}

int zcc_lextype(const char* str)
{
    if (_isstr(*str)) {
        return ZTOK_STR;
    }
    
    if (_isdigit(*str) || (*str == '.' && _isdigit(str[1]))) {
        return ZTOK_NUM;
    }
    
    if (_isid(*str)) {
        return ZTOK_ID;
    }
    
    if (_ispunct(*str)) {
        return ZTOK_SYM;
    }
    
    return ZTOK_NULL;
}

/* Main generic tokenization function */

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

/* Handy struct to handle tokens */

ztok_t ztok_get(const char* str)
{
    ztok_t tok;
    tok.str = zcc_lex(str, &tok.len, &tok.kind);
    return tok;
}

ztok_t ztok_next(ztok_t tok)
{
    tok.str = zcc_lex(tok.str + tok.len + (tok.str[tok.len] == '\n'), &tok.len, &tok.kind);
    return tok;
}

ztok_t ztok_nextl(ztok_t tok)
{
    tok.str = zcc_lex(tok.str + tok.len, &tok.len, &tok.kind);
    return tok;
}

ztok_t ztok_step(ztok_t tok, const size_t steps)
{
    size_t i;
    for (i = 0; i < steps; ++i) {
        tok = ztok_next(tok);
    }
    return tok;
}

ztok_t ztok_stepl(ztok_t tok, const size_t steps)
{
    size_t i;
    for (i = 0; i < steps; ++i) {
        tok = ztok_nextl(tok);
    }
    return tok;
}

/* Tokenize strings and ranges of strings */

struct vector zcc_tokenize(const char* str)
{
    struct vector tokens = vector_create(sizeof(ztok_t));
    ztok_t tok = ztok_get(str);
    while (tok.str) {
        vector_push(&tokens, &tok);
        tok = ztok_next(tok);
    }
    return tokens;
}

struct vector zcc_tokenize_line(const char* str)
{
    struct vector tokens = vector_create(sizeof(ztok_t));
    ztok_t tok = ztok_get(str);
    while (tok.str) {
        vector_push(&tokens, &tok);
        tok = ztok_nextl(tok);
    }
    return tokens;
}

struct vector zcc_tokenize_range(const char* start, const char* end)
{
    struct vector tokens = vector_create(sizeof(ztok_t));
    ztok_t tok = ztok_get(start);
    while (tok.str && tok.str < end) {
        vector_push(&tokens, &tok);
        tok = ztok_next(tok);
    }
    return tokens;
}
