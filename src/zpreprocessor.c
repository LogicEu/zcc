#include <zpreprocessor.h>
#include <zlexer.h>
#include <zassert.h>
#include <utopia/utopia.h>

#define string_wrap_sized(str, size) (string_t){str, size + 1, size};
#define array_wrap_sized(data, size, bytes) (array_t){data, bytes, size + 1, size};

static char* zstrbuf(const char* str, const size_t len)
{
    static char buf[0xfff];
    zmemcpy(buf, str, len);
    buf[len] = 0;
    return buf;
}

static void string_push_tok(string_t* string, const ztok_t tok)
{
    string_push(string, zstrbuf(tok.str, tok.len));
}

static void string_remove_trail(string_t* string)
{
    if (string->size) {
        size_t len = string->size - 1;
        while (len && chrspace(string->data[len])) {
            --len;
        }
        string_remove_range(string, len + 1, string->size);
    }
}

static void string_push_space(string_t* string, const ztok_t tok, const char* next)
{
    const char* p = tok.str + tok.len;
    string_push(string, zstrbuf(p, next - p));
}

/* debugging */
#if 1
#include <zio.h>
#define ZBUG zcc_log("BUG(%ld)!\n", __LINE__);

static void zcc_logtok(const char* fmt, const ztok_t tok)
{
    zcc_log(fmt, zstrbuf(tok.str, tok.len));
}

#endif /* end debugging */

typedef struct zmacro_t {
    string_t str;
    array_t args;
    array_t body;
} zmacro_t;

static void zmacro_free(zmacro_t* macro)
{
    string_free(&macro->str);
    array_free(&macro->args);
    array_free(&macro->body);
}

static array_t zcc_tokenize(const char* str)
{
    array_t tokens = array_create(sizeof(ztok_t));
    ztok_t tok = ztok_get(str);
    while (tok.str) {
        array_push(&tokens, &tok);
        tok = ztok_next(tok);
    }
    return tokens;
}

static int zmacro_args(array_t* args, ztok_t tok, const size_t linecount)
{
    while (tok.str && *tok.str != ')') {
        if (*tok.str == ',') {
            tok = ztok_next(tok);
            if (*tok.str == ')' || *tok.str == ',') {
                zcc_log("Macro function definition does not allow empty argument parameter at line %zu.\n", linecount);
                return ZCC_EXIT_FAILURE;
            }
            continue;
        }
        
        if (tok.kind != ZTOK_ID >> 8) {
            zcc_log("Macro function definition only allows valid identifiers as argument parameter at line %zu.\n", linecount);
            return ZCC_EXIT_FAILURE;
        }

        array_push(args, &tok);
        tok = ztok_next(tok);
    }

    if (!tok.str || *tok.str != ')') {
        zcc_log("Macro function definition does not close parenthesis at line %zu.\n", linecount);
        return ZCC_EXIT_FAILURE;
    }

    return ZCC_EXIT_SUCCESS;
}

static zmacro_t zmacro_create(const char* str, const size_t linecount)
{
    zmacro_t macro;
    macro.str = string_create(str);
    macro.args = array_create(sizeof(ztok_t));
    macro.body = array_create(sizeof(ztok_t));

    /* macro without body*/
    if (!macro.str.data) {
        return macro;
    }    

    /* simple macro */
    if (*macro.str.data != '(') {
        macro.body = zcc_tokenize(macro.str.data);
        return macro;
    }

    /* macro function */
    ztok_t tok = ztok_get(macro.str.data);
    if (zmacro_args(&macro.args, ztok_next(tok), linecount)) {
        zmacro_free(&macro);
        return macro;
    }

    if (macro.args.size) {
        tok = ztok_next(*(ztok_t*)array_peek(&macro.args));
    }
    else tok = ztok_next(tok);

    tok = ztok_next(tok);
    macro.body = zcc_tokenize(tok.str);
    return macro;
}

static void zcc_std_defines_push(map_t* defines, const char* keystr, const char* valstr)
{
    string_t key = string_create(keystr);
    zmacro_t value = zmacro_create(valstr, 0);
    map_push(defines, &key, &value);
}

static map_t zcc_std_defines(void)
{
    map_t defines = map_create(sizeof(string_t), sizeof(zmacro_t));
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
    char* s = zstrbuf(tok.str, tok.len);
    return map_search(defines, &s);
}

static void zcc_defines_free(map_t* defines)
{
    size_t i;
    const size_t count = defines->size;
    string_t* keys = defines->keys;
    zmacro_t* defs = defines->values;
    for (i = 0; i < count; ++i) {
        string_free(keys + i);
        zmacro_free(defs + i);
    }
    map_free(defines);
}

static int zcc_undef(map_t* defines, ztok_t tok, const size_t linecount)
{
    tok = ztok_next(tok);
    if (!tok.str) {
        zcc_log("Macro #undef is empty at line %zu.\n", linecount);
        return ZCC_EXIT_FAILURE;
    }
    
    const size_t find = zcc_defines_search(defines, tok);
    if (!find) {
        return ZCC_EXIT_FAILURE;
    }

    string_t k = *(string_t*)map_key_at(defines, find - 1);
    zmacro_t d = *(zmacro_t*)map_value_at(defines, find - 1);
    
    map_remove(defines, &k);
    string_free(&k);
    zmacro_free(&d);
    
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
    
    const char c = *lineend;
    *lineend = 0;
    const string_t id = string_ranged(tok.str, tok.str + tok.len);
    const zmacro_t def = zmacro_create(tok.str + tok.len, linecount);
    *lineend = c;

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

static string_t zcc_concatenate(const ztok_t t1, const ztok_t t2)
{
    string_t s = string_ranged(t1.str, t1.str + t1.len);
    string_push_tok(&s, t2);
    return s;
}

static string_t string_insert_before(string_t* string, const char mark, const char insert)
{
    size_t i;
    const char in[2] = {insert, 0};
    string_t s = string_copy(string);
    for (i = 0; i < s.size; ++i) {
        if (s.data[i] == mark) {
            string_push_at(&s, in, i++);
        }
    }
    return s;
}

static string_t zcc_stringify(const array_t* args)
{
    size_t i;
    const ztok_t* toks = args->data;
    string_t str = string_create("\"");
    for (i = 0; i < args->size; ++i) {
        string_t tmp, s = string_wrap_sized(toks[i].str, toks[i].len);
        switch (toks->str[0]) {
            case '"':
                tmp = string_insert_before(&s, '\\', '\\');
                s = string_insert_before(&tmp, '"', '\\');
                string_free(&tmp);
                break;
            case '\'':
                s = string_insert_before(&s, '\\', '\\');
        }
        string_concat(&str, &s);
        if (i + 1 < args->size) {
            string_push(&str, " ");
        }
    }
    string_push(&str, "\"");
    return str;
}

size_t zcc_macro_search(const array_t* params, const ztok_t tok)
{
    size_t i = 0;
    const ztok_t* args = params->data;
    for (i = 0; i < params->size; ++i) {
        if (tok.len == args[i].len && !zmemcmp(tok.str, args[i].str, tok.len)) {
            return i + 1;
        }
    }
    return 0;
}

static string_t zcc_expand(const array_t* tokens, const char* self, const map_t* defines, const size_t linecount)
{
    size_t i, j;
    string_t line = string_empty();
    ztok_t* toks = tokens->data;
    const size_t count = tokens->size;
    for (i = 0; i < count; ++i) {

        if (i > 0 && i + 1 < count && toks[i].str[0] == '#' && toks[i].str[1] == '#') {
            string_t s = zcc_concatenate(toks[i - 1], toks[i + 1]);
            string_remove_trail(&line);
            string_remove_range(&line, line.size - toks[i - 1].len, line.size);
            string_concat(&line, &s);
            string_free(&s);
            ++i;
            goto zlexspace;
        }

        if (!chralpha(*toks[i].str)) {
            goto zlextok;
        }

        const size_t find = zcc_defines_search(defines, toks[i]);
        if (!find) {
            goto zlextok;
        }

        string_t* key = (string_t*)map_key_at(defines, find - 1);
        if (self && !zmemcmp(key->data, self, key->size)) {
            goto zlextok;
        }

        zmacro_t* macro = map_value_at(defines, find - 1);

        if (*macro->str.data != '(') {
            string_t sub = zcc_expand(&macro->body, toks[i].str, defines, linecount);
            string_concat(&line, &sub);
            string_free(&sub);
            goto zlexspace;
        }

        if (i + 1 == count || *toks[i + 1].str != '(') {
            zcc_log("zcc warning: Macro function call must include parenthesis at line %zu.\n", linecount);
            goto zlextok;
        }
        ++i;

        const char* close = zcc_lexparen(toks[i].str);
        if (!close) {
            zcc_log("Macro function call must close parenthesis at line '%zu'.\n", linecount);
            string_free(&line);
            return line;
        }

        array_t args = array_create(sizeof(array_t));
       
        j = ++i;
        while (j < count && toks[j].str + toks[j].len < close) {
            const int n = (j + 1 < count && *toks[j + 1].str == ')');
            if (chrparen(*toks[j].str)) {
                close = zcc_lexparen(toks[j].str);
                while (j < count && toks[j].str < close) {
                    ++j;
                }
            }
            else if (*toks[j].str == ',' || j + 1 == count || n) {
                j += n;
                array_t arg = array_wrap_sized(toks + i, j - i, sizeof(ztok_t));
                array_push(&args, &arg);
                i = j + 1;
            }
            ++j;
        }
        --i;

        if (args.size != macro->args.size) {
            zcc_log("Macro function call has different number of arguments at line %zu.\n", linecount);
            string_free(&line);
            return line;
        }

        const size_t bcount = macro->body.size;
        const ztok_t* body = macro->body.data;
        const array_t* argstrs = args.data;

        string_t subst = string_empty();
        for (j = 0; j < bcount; ++j) {
            size_t found;
            if (j + 1 < bcount && body[j].str[0] == '#' && body[j].str[1] != '#') {
                found = zcc_macro_search(&macro->args, body[j + 1]);
                if (!found--) {
                    zcc_log("zcc error: '#' macro operator is not followed by macro parameter at line %zu.\n", linecount);
                    string_push_tok(&subst, body[j++]);
                    string_push_tok(&subst, body[j]);
                    continue;
                }
                string_t s = zcc_stringify(argstrs + found);
                string_concat(&subst, &s);
                string_free(&s);
                ++j;
            }
            else {
                found = zcc_macro_search(&macro->args, body[j]);
                if (found--) {
                    string_t s = zcc_expand(argstrs + found, self, defines, linecount);
                    string_concat(&subst, &s);
                    string_free(&s);
                }
                else string_push_tok(&subst, body[j]);
            }
            
            if (j + 1 < bcount) {
                string_push_space(&subst, body[j], body[j + 1].str);
            }
        }

        array_t subtoks = zcc_tokenize(subst.data);
        string_t s = zcc_expand(&subtoks, self, defines, linecount);
        string_concat(&line, &s);
        
        string_free(&s);
        string_free(&subst);
        array_free(&subtoks);
        array_free(&args);

        goto zlexspace;

zlextok:
        string_push_tok(&line, toks[i]);
zlexspace:
        if (i + 1 < count) {
            string_push_space(&line, toks[i], toks[i + 1].str);
        }
    }
    return line;
}

char* zcc_preprocess_macros(char* src, size_t* size, const char** includes)
{
    static const char inc[] = "include", def[] = "define", ifdef[] = "if", undef[] = "undef";

    zassert(src);

    map_t defines = zcc_std_defines();

    size_t linecount = 0, i;
    string_t text = string_wrap_sized(src, *size);
    char* linestart = text.data;
    char* lineend = zcc_lexline(text.data);
    ztok_t tok;
 
    while (*lineend) {

        ++linecount;
        i = linestart - text.data;

        tok = ztok_get(linestart);

        if (!tok.str) {
            goto zlexline;
        }

        if (*tok.str == '#') {
            tok = ztok_next(tok);
            if (!zmemcmp(tok.str, inc, sizeof(inc) - 1)) {
                ztok_t inc = zcc_include(includes,tok, linecount);
                if (inc.str) {
                    zcc_preprocess_text(inc.str, &inc.len);
                    string_push_at(&text, inc.str, lineend + 1 - text.data);
                    zfree(inc.str);
                    linestart = text.data + i;
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

        array_t linetoks = zcc_tokenize(linestart);

        string_t l = zcc_expand(&linetoks, NULL, &defines, linecount);
        if (l.data) {
            const size_t n = tok.str - text.data;
            string_remove_range(&text, n, n + lineend - tok.str);
            string_push_at(&text, l.data, n);
            string_free(&l);
            linestart = text.data + i;
            lineend = zcc_lexline(linestart);
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