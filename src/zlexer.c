#include <zlexer.h>

#define chrspace(c) (((c) == ' ') || ((c) == '\n') || ((c) == '\t') || ((c) == '\r'))
#define chrstr(c) (((c) == '\'') || ((c) == '"'))
#define chrparen(c) (((c) == '(') || ((c) == '[') || ((c) == '{')) 
#define chrbetween(c, a, b) (((c) >= (a)) && ((c) <= (b)))
#define chrdigit(c) chrbetween(c, '0', '9')
#define chralpha(c) (chrbetween(c, 'A', 'Z') || chrbetween(c, 'a', 'z') || ((c) == '_'))

char* zcc_lex(const char* str, size_t* len)
{
    *len = 0;
    
    if (!str) {
        return NULL;
    }

    while (*str && (*str == ' ' || *str == '\t')) {
        ++str;
    }

    if (!*str || *str == '\n' || *str == '\r') {
        return NULL;
    }

    const char c = *str;
    size_t i = 0;

    if (chrstr(c)) {
        ++i;
        while (str[i] && str[i] != c) {
            i += (str[i] == '\\') + 1;
        }
        ++i;
    }
    else if (chrdigit(c) || (c == '.' && chrdigit(str[1]))) {
        while (str[i] && (chrdigit(str[i]) || chralpha(str[i]) || str[i] == '.')) {
            ++i;
        }
    }
    else if (chralpha(c)) {
        while (str[i] && (chralpha(str[i]) || chrdigit(str[i]))) {
            ++i;
        }
    }
    else {
        ++i;
        switch(c) {
        case '#':
            i += (str[i] == c);
            break;
        case '-':
            if (str[i] == '>') {
                ++i;
                break;
            }
        case '|':
        case '&':
        case '+':
            if (str[i] == c) {
                ++i;
                break;
            }
        case '!':
        case '%':
        case '^':
        case '~':
        case '*':
        case '/':
        case '=':
            i += (str[i] == '=');
            break;
        case '<':
        case '>':
            i += (str[i] == c);
            i += (str[i] == '=');
        }
    }
    
    *len = i;
    return (char*)(size_t)str;
}