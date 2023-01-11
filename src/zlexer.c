#include <zlexer.h>
#include <zstring.h>
#include <zdbg.h>

/* Basic composable lexing / tokenizing functions */

char* zcc_lexnull(const char* str)
{
    (void)str;
    return NULL;
}

char* zcc_lexnone(const char* str)
{
    while (*str && !_isgraph(*str)) {
        ++str;
    }
    return *str ? (char*)(size_t)str : NULL;
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
    char c;
    zassert(_isstr(*str));
    
    c = *str++;
    while (*str && *str != c) {
        str += (*str == '\\') + 1;
    }
    return (char*)(size_t)++str;
}

char* zcc_lexparen(const char* str)
{
    char c;
    zassert(_isparen(*str));

    c = *str + 1;
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
    const char* c;
    zassert(!_isid(*str) && !_isdigit(*str));
    c = str++;
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

static unsigned int zlex_type(const char* str)
{
    if (!str || !*str) {
        return ZTOK_NULL;
    }

    if (!_isgraph(*str)) {
        return ZTOK_NON;
    }

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

char* zlex_next(const char* str, unsigned int* typeptr)
{
    static char* (*zlex_funcs[])(const char*) = {
        &zcc_lexnull,
        &zcc_lexid,
        &zcc_lexnum,
        &zcc_lexop,
        &zcc_lexstr,
        &zcc_lexnone
    };
    
    *typeptr = zlex_type(str);
    return zlex_funcs[*typeptr](str);
}

char* zcc_lex(const char* str, unsigned int* len, unsigned int* typeptr)
{
    static char* (*zlex_funcs[5])(const char*) = {
        &zcc_lexnull,
        &zcc_lexid,
        &zcc_lexnum,
        &zcc_lexop,
        &zcc_lexstr
    };


    const char* c;
    if (str) {
        str = zcc_lexspace(str);
    }

    if (!str) {
        return NULL;
    }

    c = str;
    *typeptr = zlex_type(str);
    str = zlex_funcs[*typeptr](str);
    *len = str - c;
    
    return (char*)(size_t)c;
}

/* Handy struct to handle tokens */

struct token ztok_get(const char* str)
{
    struct token tok;
    tok.str = zcc_lex(str, &tok.len, &tok.type);
    return tok;
}

struct token ztok_next(struct token tok)
{
    tok.str = zcc_lex(tok.str + tok.len + (tok.str[tok.len] == '\n'), &tok.len, &tok.type);
    return tok;
}

struct token ztok_nextl(struct token tok)
{
    tok.str = zcc_lex(tok.str + tok.len, &tok.len, &tok.type);
    return tok;
}

/* Tokenize strings and ranges of strings */

struct vector zcc_tokenize(const char* str)
{
    struct token tok;
    struct vector tokens = vector_create(sizeof(struct token));
    tok = ztok_get(str);
    while (tok.str) {
        vector_push(&tokens, &tok);
        tok = ztok_next(tok);
    }
    return tokens;
}

struct vector zcc_tokenize_line(const char* str)
{
    struct token tok;
    struct vector tokens = vector_create(sizeof(struct token));
    tok = ztok_get(str);
    while (tok.str) {
        vector_push(&tokens, &tok);
        tok = ztok_nextl(tok);
    }
    return tokens;
}

struct vector zcc_tokenize_range(const char* start, const char* end)
{
    struct token tok;
    struct vector tokens = vector_create(sizeof(struct token));
    tok = ztok_get(start);
    while (tok.str && tok.str < end) {
        vector_push(&tokens, &tok);
        tok = ztok_next(tok);
    }
    return tokens;
}
