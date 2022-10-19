#include <zpreprocessor.h>
#include <zlexer.h>
#include <zstd.h>
#include <zassert.h>
#include <utopia/utopia.h>

#define ZCC_EXIT_SUCCESS 0
#define ZCC_EXIT_FAILURE 1

#define chrspace(c) (((c) == ' ') || ((c) == '\n') || ((c) == '\t') || ((c) == '\r'))
#define chrstr(c) (((c) == '\'') || ((c) == '"'))
#define chrparen(c) (((c) == '(') || ((c) == '[') || ((c) == '{')) 
#define chrbetween(c, a, b) (((c) >= (a)) && ((c) <= (b)))
#define chrdigit(c) chrbetween(c, '0', '9')
#define chralpha(c) (chrbetween(c, 'A', 'Z') || chrbetween(c, 'a', 'z') || ((c) == '_'))

#define string_wrap_sized(str, size) (string_t){str, size + 1, size};
#define array_wrap_sized(data, size, bytes) (array_t){data, bytes, size + 1, size};

/* debugging */
#if 1
#include <zio.h>
#define ZBUG zcc_log("BUG(%ld)!\n", __LINE__);

static void zcc_logtok(const char* fmt, const ztok_t tok)
{
    const char c = tok.str[tok.len];
    tok.str[tok.len] = 0;
    zcc_log(fmt, tok.str);
    tok.str[tok.len] = c;
}

#endif /* end debugging */

static void zcc_std_defines_push(map_t* defines, const char* keystr, const char* valstr)
{
    string_t key = string_create(keystr);
    string_t value = string_create(valstr);
    map_push(defines, &key, &value);
}

static map_t zcc_std_defines(void)
{
    map_t defines = map_create(sizeof(string_t), sizeof(string_t));
    zcc_std_defines_push(&defines, "__STDC__", "1");
    zcc_std_defines_push(&defines, "__STDC_HOSTED__", "0");
    zcc_std_defines_push(&defines, "__STDC_VERSION__", "201710");
    zcc_std_defines_push(&defines, "__WCHAR_MAX__", "2147483647");
    zcc_std_defines_push(&defines, "__has_feature", "(x) 0");
    zcc_std_defines_push(&defines, "__has_include", "(x) 0");
    zcc_std_defines_push(&defines, "__has_include_next", "(x) 0");
    return defines;
}

static size_t zcc_defines_search(const map_t* defines, const ztok_t tok)
{
    const char c = tok.str[tok.len];
    tok.str[tok.len] = 0;
    const size_t search = map_search(defines, &tok.str);
    tok.str[tok.len] = c;
    return search;
}

static void zcc_defines_free(map_t* defines)
{
    size_t i;
    const size_t count = defines->size;
    string_t* keys = defines->keys;
    string_t* defs = defines->values;
    for (i = 0; i < count; ++i) {
        string_free(keys + i);
        string_free(defs + i);
    }
    map_free(defines);
}

static int zcc_undef(map_t* defines, ztok_t tok, const size_t linecount)
{
    tok = ztok_next(tok);
    if (!tok.str) {
        zcc_log("Macro #undef is empty at line %zu.\n", linecount);
    }
    
    const size_t find = zcc_defines_search(defines, tok);
    if (!find) {
        return ZCC_EXIT_FAILURE;
    }

    string_t k = *(string_t*)map_key_at(defines, find - 1);
    string_t d = *(string_t*)map_value_at(defines, find - 1);
    
    map_remove(defines, &k);
    string_free(&k);
    string_free(&d);
    
    return ZCC_EXIT_SUCCESS;
}

static int zcc_define(map_t* defines, ztok_t tok, const size_t linecount)
{
    tok = ztok_next(tok);
    char* lineend = zstrchr(tok.str, '\n');
    lineend = !lineend ? tok.str + zstrlen(tok.str) : lineend;
    if (!tok.str) {
        zcc_log("Macro #define is empty at line %zu.\n");
        return ZCC_EXIT_FAILURE;
    }
    
    const string_t id = string_ranged(tok.str, tok.str + tok.len);
    const string_t def = string_ranged(tok.str + tok.len, lineend);
    const size_t find = map_push_if(defines, &id, &def);
    if (find) {
        zcc_log("Macro redefinition is not allowed at line %zu.\n", linecount);
        return ZCC_EXIT_FAILURE;
    }

    return ZCC_EXIT_SUCCESS;
}

static ztok_t zcc_include(const char** includes, ztok_t tok, const size_t linecount)
{
    static char dir[0xfff] = "./";
    char filename[0xfff], buf[0xfff];
    size_t dlen, flen;
    ztok_t inc = {NULL, 0, ZTOK_SRC};
    
    tok = ztok_next(tok);
    if (!tok.str) {
        zcc_log("Macro directive #include is empty at line at line %zu.\n", linecount);
        return inc;
    }

    if (*tok.str == '"') {
        dlen = zstrlen(dir);
        flen = tok.len - 2;
        zmemcpy(filename, tok.str + 1, flen);
        filename[flen] = 0;
        zmemcpy(buf, dir, dlen);
        zmemcpy(buf + dlen, filename, flen + 1);
        inc.str = zcc_fread(buf, &inc.len);
        return inc;
    }

    if (*tok.str != '<') {
        zcc_log("Macro directive #include must have \"\" or <> symbol at line %zu.\n", linecount);
        return inc;
    }

    const char* ch = zstrchr(tok.str, '>');
    if (!ch) {
        zcc_log("Macro #include does not close '>' bracket at line %zu.\n", linecount);
        return inc;
    }
    
    size_t i;
    flen = ch - tok.str - 1;
    zmemcpy(filename, tok.str + 1, flen);
    filename[flen] = 0;
    for (i = 0; includes[i] && !inc.str; ++i) {
        dlen = zstrlen(includes[i]);
        zmemcpy(dir, includes[i], dlen);
        if (dir[dlen - 1] != '/') {
            dir[dlen++] = '/';
        }
        zmemcpy(buf, dir, dlen);
        zmemcpy(buf + dlen, filename, flen + 1);
        inc.str = zcc_fread(buf, &inc.len);
    }

    if (!inc.str) {
        zcc_log("Could not open header file '%s' at line %zu.\n", filename, linecount);
    }

    return inc;
}

static int zcc_ifdef(const map_t* defines, ztok_t tok, const size_t linecount)
{
    (void)defines;
    (void)tok;
    (void)linecount;
    return ZCC_EXIT_SUCCESS;
}

char* zcc_preprocess_macros(char* src, size_t* size, const char** includes)
{
    static const char inc[] = "include", def[] = "define", ifdef[] = "if", undef[] = "undef";

    zassert(src);

    map_t defines = zcc_std_defines();

    size_t linecount = 0, i;
    string_t text = string_wrap_sized(src, *size);
    char* linestart = src;
    char* lineend = zcc_lexline(src);
    ztok_t tok;
 
    while (lineend) {
        ++linecount;
        i = linestart - src;

        tok = ztok_get(linestart);

        if (!tok.str) {
            goto zlexline;
        }

        if (*tok.str == '#') {
            tok = ztok_next(tok);
            if (!zmemcmp(tok.str, inc, sizeof(inc) - 1)) {
                ztok_t inc = zcc_include(includes, tok, linecount);
                if (inc.str) {
                    zcc_preprocess_text(inc.str, &inc.len);
                    string_push_at(&text, inc.str, lineend + 1 - src);
                    zfree(inc.str);
                    src = text.data;
                    linestart = src + i;
                    lineend = zcc_lexline(linestart);
                }
            }
            else if (!zmemcmp(tok.str, def, sizeof(def) - 1)) {
                zcc_define(&defines, tok, linecount);
            }
            else if (!zmemcmp(tok.str, undef, sizeof(undef) - 1)) {
                zcc_undef(&defines, tok, linecount);
            }
            else if (!zmemcmp(tok.str, ifdef, sizeof(ifdef) - 1)) {
                zcc_ifdef(&defines, tok, linecount);
            }
            /*else zcc_log("Illegal macro directive at line %zu.\n", linecount);*/

            string_remove_range(&text, i, i + lineend - linestart);
            lineend = zcc_lexline(linestart);
            continue;
        }

        while (tok.str) {
            if (!chralpha(*tok.str)) {
                goto zlextok;
            }

            const size_t find = zcc_defines_search(&defines, tok);
            if (!find) {
                goto zlextok;
            }
            
            ztok_t t;
            char* macro = *(char**)map_value_at(&defines, find - 1);

            if (*macro != '(') {
                t = ztok_get(macro);
                const size_t n = tok.str - src;
                string_remove_range(&text, n, n + tok.len);
                string_push_at(&text, t.str, n);
                src = text.data;
                linestart = src + i;
                lineend = zcc_lexline(linestart);
                continue;
            }

            tok = ztok_next(tok);
            if (*tok.str != '(') {
                zcc_logtok("%s\n", tok);
                zcc_log("zcc warning: Macro function call must include parenthesis at line %zu.\n");
                continue;
            }

            /*t = ztok_get(macro + 1);*/

            /* macro function */

zlextok:
            tok = ztok_next(tok);
        }
zlexline:
        linestart = lineend + !!*lineend;
        lineend = zcc_lexline(linestart);
    }

    zcc_defines_free(&defines);
    *size = text.size;
    return text.data;
}

char* zcc_preprocess_text(char* str, size_t* size)
{
    char* ch;
    size_t linecount = 0, i;
    string_t text = string_wrap_sized(str, *size);
    for (i = 0; str[i]; ++i) {
        ch = str + i;
        switch (*ch) {
        case '\n':
            if (i > 0 && str[i - 1] == '\\') {
                string_remove_index(&text, i--);
                string_remove_index(&text, i--);
            }
            ++linecount;
            break;
        case '"':
        case '\'':
            /*i = zcc_lexstr(ch) - str;*/
            ++i;
            while (str[i] && str[i] != *ch) {
                i += (str[i] == '\\');
                ++i;
            } 
            ++i;
            break;
        case '/':
            if (str[i + 1] == '/') {
                while (*ch && *ch != '\n') {
                    ++ch;
                }
                string_remove_range(&text, i, i + ch - (str + i));
            }
            else if (str[i + 1] == '*') {
                ch = zstrstr(str + i + 2, "*/");
                if (!ch) {
                    zcc_log("Comment is not closed on line %zu.\n", linecount + 1);
                    str[i] = 0;
                    return text.data;
                }
                
                string_remove_range(&text, i + 1, i + ch - (str + i) + 2);
                str[i] = ' ';
            }
            break;
        }
    }

    *size = text.size;
    return text.data;
}