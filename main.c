#include <utopia/utopia.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ISNONE(c) (((c) == ' ') || ((c) == '\n') || ((c) == '\t'))
#define ISSTR(c) (((c) == '\'') || ((c) == '"'))
#define ISBETWEEN(c, a, b) (((c) >= (a)) && ((c) <= (b)))
#define ISNUM(c) ISBETWEEN(c, '0', '9')
#define ISID(c) (ISBETWEEN(c, 'A', 'Z') || ISBETWEEN(c, 'a', 'z') || ((c) == '_'))
#define ISSYMBOL(c) (ISBETWEEN(c, 33, 126) && !ISNUM(c) && !ISID(c))

typedef struct range_t {
    size_t start, end;
} range_t;

typedef struct ptu_t {
    char* name;
    string_t text;
    array_t lines;
    array_t tokens;
} ptu_t;

static inline size_t eatblock(const char* str, const char ch, const size_t end, size_t i)
{
    while (i + 1 < end && str[i + 1] != ch) { 
        i += (str[i + 1] == '\\') + 1;
    } 
    return i + 1;
}

static inline void printrng(const char* str, const range_t range)
{
    for (size_t i = range.start; i < range.end; ++i) {
        putchar(str[i]);
    }
}

static void ptu_print_lines(const ptu_t* ptu)
{
    const range_t* r = ptu->lines.data;
    const size_t count = ptu->lines.size;
    for (size_t i = 0; i < count; ++i) {
        putchar('\'');
        printrng(ptu->text.data, r[i]);
        putchar('\'');
        putchar('\n');
    }
}

static void ptu_print_tokens(const ptu_t* ptu)
{
    size_t i, j;
    const size_t count = ptu->tokens.size;
    const range_t* r = ptu->tokens.data, *l = ptu->lines.data;
    for (i = 0, j = 0; i < count; ++i) {
        while (r[i].start >= l[j].end) {
            ++j;
        }

        putchar('\'');
        printrng(ptu->text.data, r[i]); 
        putchar('\'');
        putchar(r[i].end >= l[j].end ? '\n' : ' ');

    }
}

static void ptu_free(ptu_t* ptu)
{
    string_free(&ptu->text);
    array_free(&ptu->lines);
    array_free(&ptu->tokens);
}

static void ptu_splice_lines(ptu_t* ptu)
{
    size_t i;
    range_t r = {0, 0};
    for (i = 0; ptu->text.data[i]; ++i) {
        if (ptu->text.data[i] == '\n') {
            if (i > 0 && ptu->text.data[i - 1] == '\\') {
                string_remove_index(&ptu->text, i--);
                string_remove_index(&ptu->text, i--);
            } else {
                r.end = i;
                array_push(&ptu->lines, &r);
                r.start = i + 1;
            }
        }
    }
    
    r.end = i;
    array_push(&ptu->lines, &r);
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

static void ptu_splice_tokens(ptu_t* ptu)
{
    range_t r = {0, 0};
    const char* s = ptu->text.data;
    for (size_t i = 0; s[i]; ++i) {
        
        r.start = i;
        
        if (ISSTR(s[i])) {
            i = eatblock(s, s[i], ptu->text.size, i) + 1;
        }
        else if (ISNUM(s[i]) || (s[i] == '.' && ISNUM(s[i + 1]))) {
            while (s[i] && (ISNUM(s[i]) || ISID(s[i]) || s[i] == '.')) {
                ++i;
            }
        }
        else if (ISID(s[i])) {
            while (s[i] && (ISID(s[i]) || ISNUM(s[i]))) {
                ++i;
            }
        }
        else if (!ISNONE(s[i])) {
            i = eatsymbol(s, i);
        }
        else continue;

        r.end = i--;
        array_push(&ptu->tokens, &r);
    }
}

/* needs more error checking for block closures */
static void ptu_preprocess_text(ptu_t* ptu)
{
    ptu_splice_lines(ptu);
    
    char* s = ptu->text.data, *t;
    size_t i, j, k, d, n;
    range_t* r = ptu->lines.data;
    for (i = 0, j = 0, d = 0; s[i]; ++i) {
        n = 0; 
        switch (s[i]) {
        case '"':
        case '\'':
            i = eatblock(s, s[i], r[j].end, i) + 1;
            break;
        case '/':
            if (s[i + 1] == '/') {
                string_remove_range(&ptu->text, i, r[j].end);
                d += r[j].end - i;
                r[j].end = i;
            }
            else if (s[i + 1] == '*') {
                t = strstr(s + i + 2, "*/");
                if (!t) {
                    printf("Comment is not closed on line %zu.\n", j + 1);
                    return ptu_free(ptu);
                }
                
                n = t - (s + i) + 2;
                string_remove_range(&ptu->text, i + 1, i + n);
                s[i] = ' ';

                k = j;
                while (i + n > r[k].end) {
                    ++k;
                    r[k].end -= d;
                }
               
                d += n - 1;
                r[k].start = r[j].start;
                r[k].end -= n - 1;
                array_remove_block(&ptu->lines, j, k);
            }
            break;
        }

        if (!n && i >= r[j].end) {
            r[++j].start -= d;
            r[j].end -= d;
        }

    }

    ptu_splice_tokens(ptu);
}

static ptu_t ptu_read(const char* filename)
{
    ptu_t ptu;
    ptu.name = (char*)(size_t)filename;
    ptu.text = string_empty();
    ptu.lines = array_create(sizeof(range_t));
    ptu.tokens = array_create(sizeof(range_t));
    
    FILE* file = fopen(filename, "rb");
    if (!file) {
        printf("Could not open file '%s'.\n", filename);
        return ptu;
    }
    
    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    fseek(file, 0, SEEK_SET);
    ptu.text = string_reserve(size + 1);
    fread(ptu.text.data, 1, size, file);
    fclose(file);
    ptu.text.data[size] = 0;
    ptu.text.size = size;
    ptu_preprocess_text(&ptu);
    return ptu;
}

int main(const int argc, const char** argv)
{
    int status = EXIT_SUCCESS;
    array_t infiles = array_create(sizeof(char*));
    array_t includes = array_create(sizeof(char*));

    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] == 45) {
            if (argv[i][1] == 'I') {
                const char* ptr = argv[i] + 2;
                array_push(&includes, &ptr);
            }
        } 
        else array_push(&infiles, &argv[i]);
    }

    if (!infiles.size) {
        printf("Missing input file.\n");
        status = EXIT_FAILURE;
        goto exit;
    }

    char** filepaths = infiles.data;
    const size_t filecount = infiles.size;
    for (size_t i = 0; i < filecount; ++i) {
        ptu_t ptu = ptu_read(filepaths[i]);
        if (ptu.text.size) {
            ptu_print_lines(&ptu);
            ptu_print_tokens(&ptu);
        }
        ptu_free(&ptu);
    }

exit:
    array_free(&infiles);
    array_free(&includes);
    return status ;
}
