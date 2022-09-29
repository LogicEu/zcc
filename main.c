#include <utopia/utopia.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define chrspace(c) (((c) == ' ') || ((c) == '\n') || ((c) == '\t'))
#define chrstr(c) (((c) == '\'') || ((c) == '"'))
#define chrbetween(c, a, b) (((c) >= (a)) && ((c) <= (b)))
#define chrdigit(c) chrbetween(c, '0', '9')
#define chralpha(c) (chrbetween(c, 'A', 'Z') || chrbetween(c, 'a', 'z') || ((c) == '_'))

typedef struct range_t {
    size_t start, end;
} range_t;

typedef struct ptu_t {
    char* name;
    string_t text;
    array_t lines;
    array_t tokens;
} ptu_t;

static inline void printrng(const char* str, const range_t range)
{
    for (size_t i = range.start; i < range.end; ++i) {
        putchar(str[i]);
    }
}

static range_t ptu_tokens_at(const range_t* toks, const size_t count, range_t range)
{
    size_t i;
    for (i = 0; i < count && toks[i].start < range.start; ++i);
    for (range.start = i; i < count && toks[i].end <= range.end; ++i);
    range.end = i;
    return range;
}

static void ptu_print_tokens(const ptu_t* ptu)
{
    const char* s = ptu->text.data;
    const size_t linecount = ptu->lines.size, tokencount = ptu->tokens.size;
    const range_t* lines = ptu->lines.data, *tokens = ptu->tokens.data;
    for (size_t i = 0; i < linecount; ++i) {
        const range_t toks = ptu_tokens_at(tokens, tokencount, lines[i]);
        for (size_t j = toks.start; j < toks.end; ++j) {
            putchar('\'');
            printrng(s, tokens[j]);
            putchar('\'');
            putchar(' ');
        }
        putchar('\n');
        
        /*putchar('\'');
        printrng(s, lines[i]);
        putchar('\'');
        putchar('\n');*/
    }
}

static void ptu_free(ptu_t* ptu)
{
    string_free(&ptu->text);
    array_free(&ptu->lines);
    array_free(&ptu->tokens);
}

static inline size_t eatblock(const char* str, const char ch, const size_t end, size_t i)
{
    while (i + 1 < end && str[i + 1] != ch) { 
        i += (str[i + 1] == '\\') + 1;
    } 
    return i + 1;
}

static inline size_t eatsymbol(const char* str, const size_t i)
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
   
    if (i > r.start) { 
        r.end = i;
        array_push(&ptu->lines, &r);
    }
}


static void ptu_splice_tokens(ptu_t* ptu)
{
    range_t r = {0, 0};
    const char* s = ptu->text.data;
    for (size_t i = 0; s[i]; ++i) {
        
        r.start = i;
        
        if (chrstr(s[i])) {
            i = eatblock(s, s[i], ptu->text.size, i) + 1;
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
        array_push(&ptu->tokens, &r);
    }
}

static void ptu_preprocess_text(ptu_t* ptu)
{
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
    ptu_splice_lines(&ptu);
    ptu_preprocess_text(&ptu);
    ptu_splice_tokens(&ptu);
    return ptu;
}

static void rangesoffset(const array_t* ranges, const size_t off, const size_t from)
{
    range_t* n = ranges->data;
    for (size_t i = from; i < ranges->size; ++i) {
        n[i].start += off;
        n[i].end += off;
    }
}

static void ptu_merge(ptu_t* dest, ptu_t* src, const range_t toks, const size_t index)
{
    const range_t* lines = dest->lines.data;
    const range_t line = lines[index];
    const size_t linelen = line.end - line.start + 1;

    rangesoffset(&src->lines, line.start, 0);
    rangesoffset(&src->tokens, line.start, 0);
   
    string_remove_range(&dest->text, line.start, line.end + 1);
    array_remove(&dest->lines, index);
    array_remove_block(&dest->tokens, toks.start, toks.end);
   
    rangesoffset(&dest->lines, src->text.size - linelen, index);
    rangesoffset(&dest->tokens, src->text.size - linelen, toks.start); 
     
    string_push_at(&dest->text, src->text.data, line.start); 
    array_push_block_at(&dest->lines, src->lines.data, src->lines.size, index);
    array_push_block_at(&dest->tokens, src->tokens.data, src->tokens.size, toks.start);
   
    ptu_free(src);
}

static void ptu_include(ptu_t* ptu, const array_t* includes,
                        const range_t line, size_t* index)
{    
    ptu_t p;
    char filename[0xfff];
    const char* s = ptu->text.data; 
    const range_t* toks = ptu->tokens.data;
    const size_t incopt = toks[line.start + 2].start;
    const size_t cmp = (s[incopt] == '"');
    const size_t start = line.start + 2 + !cmp;
    const size_t end = cmp ? start : line.end - 1;
    const range_t r = {toks[start].start + cmp, toks[end].end - 1}; 

    const size_t len = r.end - r.start;
    memcpy(filename, s + r.start, len);
    filename[len] = 0;
    
    if (s[incopt] == '"') {
        p = ptu_read(filename);
    }
    else if (s[incopt] == '<') {
        char dir[0xfff];
        const size_t count = includes->size;
        const char** idirs = includes->data;;
        for (size_t i = 0, dirlen; i < count; ++i) {
            dirlen = strlen(idirs[i]);
            memcpy(dir, idirs[i], dirlen);
            if (dir[dirlen - 1] != '/') {
                dir[dirlen++] = '/';
            }
            memcpy(dir + dirlen, filename, len + 1);
            p = ptu_read(dir);
            if (p.text.size) {
                break;
            }
        }
    }

    
    if (!p.text.size) {
        printf("Could not open header file '%s'.\n", filename);
        return;
    }
    
    ptu_merge(ptu, &p, line, (*index)--);
}

/*static void ptu_define(ptu_t* ptu, map_t* defines, const range_t line)
{
    const char* s = ptu->text.data;
    const range_t* toks = ptu->tokens.data;
    for (size_t i = line.start; i < line.end; ++i) {
        printrng(s, toks[i]);
        putchar(' ');
    }
    putchar('\n');
}

static void ptu_ifdef(ptu_t* ptu, const map_t* defines, const range_t line)
{
    const char* s = ptu->text.data;
    const range_t* toks = ptu->tokens.data;
    for (size_t i = line.start; i < line.end; ++i) {
        printrng(s, toks[i]);
        putchar(' ');
    }
    putchar('\n');
}

static void ptu_undef(ptu_t* ptu, map_t* defines, const range_t line)
{
    
}*/

static void ptu_preprocess_macros(ptu_t* ptu, const array_t* includes)
{
    static const char* inc = "include", *def = "define", *ifdef = "if", *undef = "undef";
    const size_t inclen = strlen(inc), deflen = strlen(def);
    const size_t ifdeflen = strlen(ifdef), undeflen = strlen(undef);

    map_t defines = map_create(sizeof(string_t), sizeof(string_t));

    size_t i, j;
    for (i = 0; i < ptu->lines.size; ++i) {
        const range_t* lines = ptu->lines.data;
        if (lines[i].start == lines[i].end) {
            continue;
        }

        const range_t* tokens = ptu->tokens.data;
        const range_t linetoks = ptu_tokens_at(tokens, ptu->tokens.size, lines[i]);
        if (linetoks.start == linetoks.end) {
            continue;
        }

        j = tokens[linetoks.start].start;
        if (ptu->text.data[j] == '#') {
            j = tokens[linetoks.start + 1].start;
            if (!memcmp(ptu->text.data + j, inc, inclen)) {
                ptu_include(ptu, includes, linetoks, &i);
            }
            /*else if (!memcmp(s + j, def, deflen)) {
                ptu_define(ptu, &defines, linetoks);
            }
            else if (!memcmp(s + j, ifdef, ifdeflen)) {
                ptu_ifdef(ptu, &defines, linetoks);
            }
            else if (!memcmp(s + j, undef, undeflen)) {
                ptu_undef(ptu, &defines, linetoks);
            }*/
        } 
    }
}

static array_t stdincludes(void)
{
    static const char* stddirs[] = {".", "/usr/include/", "/usr/local/include/",
        ("/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/"
        "Developer/SDKs/MacOSX.sdk/usr/include")
    };

    array_t includes = array_create(sizeof(char*));
    array_push_block(&includes, stddirs, sizeof(stddirs) / sizeof(stddirs[0]));
    return includes;
}

int main(const int argc, const char** argv)
{
    int status = EXIT_SUCCESS;
    array_t infiles = array_create(sizeof(char*));
    array_t includes = stdincludes();

    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] == '-') {
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
            ptu_preprocess_macros(&ptu, &includes);
            ptu_print_tokens(&ptu);
            ptu_free(&ptu);
        }
        else printf("Could not open file '%s'.\n", filepaths[i]);
    }

exit:
    array_free(&infiles);
    array_free(&includes);
    return status ;
}
