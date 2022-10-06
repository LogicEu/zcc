#include <preproc.h>

static size_t eatblock(const char* str, const char ch, const size_t end, size_t i)
{
    while (i + 1 < end && str[i + 1] != ch) { 
        i += (str[i + 1] == '\\') + 1;
    } 
    return i + 1;
}

static size_t eatsymbol(const char* str, const size_t i)
{
    size_t j = 1;
    switch(str[i]) {
    case '#':
        j += (str[i + 1] == str[i]);
        break;
    case '-':
        j += (str[i + 1] == '>');
    case '|':
    case '&':
    case '+':
        j += (str[i + 1] == str[i]);
    case '!':
    case '%':
    case '^':
    case '~':
    case '*':
    case '/':
    case '=':
        j += (str[i + 1] == '=');
        break;
    case '<':
    case '>':
        j += (str[i + j] == str[i]);
        j += (str[i + j] == '=');
        break;
    }
    return i + j;
}

array_t strlines(string_t* str)
{
    size_t i;
    range_t r = {0, 0};
    const char* s = str->data;
    array_t lines = array_create(sizeof(range_t));
    for (i = 0; s[i]; ++i) {
        if (s[i] == '\n') {
            if (i > 0 && s[i - 1] == '\\') {
                string_remove_index(str, i--);
                string_remove_index(str, i--);
            } else {
                r.end = i;
                array_push(&lines, &r);
                r.start = i + 1;
            }
        }
    }
   
    if (i > r.start) { 
        r.end = i;
        array_push(&lines, &r);
    }
    return lines;
}

array_t strtoks(const string_t* str)
{
    size_t i;
    range_t r = {0, 0};
    const char* s = str->data;
    array_t tokens = array_create(sizeof(range_t));
    for (i = 0; s[i]; ++i) {
        r.start = i;   
        if (chrstr(s[i])) {
            i = eatblock(s, s[i], str->size, i) + 1;
        }
        else if (chrdigit(s[i]) || (s[i] == '.' && chrdigit(s[i + 1]))) {
            while (s[i] && (chrdigit(s[i]) || chralpha(s[i]) || s[i] == '.')) {
                ++i;
            }
        }
        else if (chralpha(s[i])) {
            while (s[i] && (chralpha(s[i]) || chrdigit(s[i]))) {
                ++i;
            }
        }
        else if (!chrspace(s[i])) {
            i = eatsymbol(s, i);
        }
        else continue;
        r.end = i--;
        array_push(&tokens, &r);
    }

    return tokens;
}

void strtext(string_t* text, array_t* lines)
{
    size_t i, j, k, d, n;
    char* s = text->data, *t;
    range_t* r = lines->data;
    for (i = 0, j = 0, d = 0; s[i]; ++i) {
        n = 0; 
        switch (s[i]) {
        case '"':
        case '\'':
            i = eatblock(s, s[i], r[j].end, i) + 1;
            break;
        case '/':
            if (s[i + 1] == '/') {
                string_remove_range(text, i, r[j].end);
                d += r[j].end - i;
                r[j].end = i;
            }
            else if (s[i + 1] == '*') {
                t = strstr(s + i + 2, "*/");
                if (!t) {
                    return ppc_log("Comment is not closed on line %zu.\n", j + 1);
                }
                
                n = t - (s + i) + 2;
                string_remove_range(text, i + 1, i + n);
                s[i] = ' ';

                k = j;
                while (i + n > r[k].end) {
                    ++k;
                    r[k].end -= d;
                }
               
                d += n - 1;
                r[k].start = r[j].start;
                r[k].end -= n - 1;
                array_remove_block(lines, j, k);
            }
            break;
        }

        if (!n && i >= r[j].end) {
            r[++j].start -= d;
            r[j].end -= d;
        }
    }
}