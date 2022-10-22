#include <zpreprocessor.h>
#include <zlexer.h>
#include <zsolver.h>
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

static void string_wipe(string_t* string)
{
    size_t i;
    for (i = 0; string->data[i]; ++i) {
        if (string->data[i] == ' ') {
            while (string->data[i] == ' ') {
                string_remove_index(string, i);
            }
            string_push_at(string, " ", i++);
        }
    }
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

static int zmacro_args(array_t* args, string_t* string, ztok_t tok, const size_t linecount)
{
    static const char vdots[] = "...", vargs[] = "__VA_ARGS__";

    /*zcc_log("->%s\n", tok.str);*/
    while (tok.str && *tok.str != ')') {
        if (*tok.str == ',') {
            tok = ztok_next(tok);
            if (*tok.str == ')' || *tok.str == ',') {
                zcc_log("Macro function definition does not allow empty argument parameter at line %zu.\n", linecount);
                return ZCC_EXIT_FAILURE;
            }
            continue;
        }
        
        if (!zmemcmp(tok.str, vdots, sizeof(vdots) - 1)) {
            if (tok.str[sizeof(vdots) - 1] != ')') {
                zcc_log("Missing ')' in macro parameter list at line %zu.\n", linecount);
                return ZCC_EXIT_FAILURE;
            }
            size_t n = tok.str - string->data;
            string_remove_range(string, n, n + sizeof(vdots) - 1);
            string_push_at(string, vargs, n);
            tok.kind = ZTOK_ID;
            tok.str = string->data + n;
            tok.len = sizeof(vargs) - 1;
        }
        else if (!chralpha(*tok.str)) {
            zcc_log("Macro function definition only allows valid identifiers as argument parameter at line %zu.\n", linecount);
            zcc_log("%s\n", tok.str);
            zexit(1);
            return ZCC_EXIT_FAILURE;
        }
        /*zcc_logtok("<%s>\n", tok);*/
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

    string_wipe(&macro.str);

    /* simple macro */
    if (*macro.str.data != '(') {
        macro.body = zcc_tokenize(macro.str.data);
        return macro;
    }

    /* macro function */
    ztok_t tok = ztok_get(macro.str.data);
    if (zmacro_args(&macro.args, &macro.str, ztok_next(tok), linecount)) {
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
    zcc_std_defines_push(&defines, "__APPLE__", "1");
    zcc_std_defines_push(&defines, "__x86_64__", "1");
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
        switch (toks[i].str[0]) {
            case '"':
                tmp = string_insert_before(&s, '\\', '\\');
                s = string_insert_before(&tmp, '"', '\\');
                string_free(&tmp);
                break;
            case '\'':
                s = string_insert_before(&s, '\\', '\\');
        }
        string_concat(&str, &s);
        if (i + 1 < args->size && toks[i + 1].str > toks[i].str + toks[i].len) {
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

static string_t zcc_expand(const array_t* tokens, const map_t* defines, size_t* refs, const size_t linecount)
{
    size_t refcount;
    for (refcount = 0; refs[refcount]; ++refcount);

    const size_t perm = ((long)refs[refcount - 1] == -1);
    if (perm) {
        refs[--refcount] = 0;
    }

    size_t i, j;
    string_t line = string_empty();
    ztok_t* toks = tokens->data;
    const size_t count = tokens->size;   
    for (i = 0; i < count; ++i) {

        if (i + 2 < count && toks[i + 1].str[0] == '#' && toks[i + 1].str[1] == '#') {
            string_t s = zcc_concatenate(toks[i], toks[i + 2]);
            string_remove_trail(&line);
            string_concat(&line, &s);
            string_free(&s);
            i += 2;
            goto zlexspace;
        }

        if (!chralpha(*toks[i].str)) {
            goto zlextok;
        }

        const size_t find = zcc_defines_search(defines, toks[i]);
        if (!find) {
            goto zlextok;
        }

        for (j = 0; !perm && j < refcount; ++j) {
            if (find == refs[j]) {
                goto zlextok;
            }
        }

        zmacro_t* macro = map_value_at(defines, find - 1);
        if (!macro->str.data) {
            continue;
        }

        if (*macro->str.data != '(') {
            refs[refcount++] = find;
            string_t sub = zcc_expand(&macro->body, defines, refs, linecount);
            if (!perm) {
                refs[--refcount] = 0;
            }
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
            if (chrparen(*toks[j].str)) {
                close = zcc_lexparen(toks[j].str);
                while (j < count && toks[j].str < close) {
                    ++j;
                }
            }

            const int n = (toks[j].str < close && j + 1 < count && toks[j + 1].str + toks[j + 1].len >= close);
            if (*toks[j].str == ',' || toks[j].str >= close || j + 1 >= count || n) {
                j += n;
                array_t arg = array_wrap_sized(toks + i, j - i, sizeof(ztok_t));
                array_push(&args, &arg);
                i = j + 1;
            }
            ++j;
        }

        i -= !!args.size;

        if (args.size != macro->args.size) {
            ztok_t t = ztok_get("__VA_ARGS__");
            size_t found = zcc_macro_search(&macro->args, t);
            if (found--) {
                array_t* argarr = args.data;
                while (found + 1 < args.size) {
                    array_push_block(argarr + found, argarr[found + 1].data, argarr[found + 1].size);
                    array_remove(&args, found + 1);
                }
            }
            else {
                zcc_log("Macro function call has different number of arguments at line %zu.\n", linecount);
                string_free(&line);
                break;
            }
        }

        const size_t bcount = macro->body.size;
        const ztok_t* body = macro->body.data;
        const array_t* argstrs = args.data;

        string_t subst = string_empty();
        for (j = 0; j < bcount; ++j) {
            size_t found;
            if (j + 2 < bcount && *body[j + 1].str == '#' && body[j + 1].str[1] == '#') {
                size_t n;
                for (n = j + 2; j <= n; j += 2) {
                    found = zcc_macro_search(&macro->args, body[j]);
                    if (found--) {
                        ztok_t* a = argstrs[found].data;
                        ztok_t b = a[argstrs[found].size - 1];
                        string_push(&subst, zstrbuf(a[0].str, b.str + b.len - a[0].str));
                    }
                    else string_push_tok(&subst, body[j]);
                }
            }
            else if (j + 1 < bcount && *body[j].str == '#' && body[j].str[1] != '#') {
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
                    refs[refcount] = -1;
                    string_t s = zcc_expand(argstrs + found, defines, refs, linecount);
                    for (refcount = 0; refs[refcount]; ++refcount);
                    string_concat(&subst, &s);
                    string_free(&s);
                }
                else string_push_tok(&subst, body[j]);
            }
            
            if (j + 1 < bcount && body[j + 1].str > body[j].str + body[j].len) {
                string_push(&subst, " ");
            }
        }

        array_t subtoks = zcc_tokenize(subst.data);
        refs[refcount++] = find;
        string_t s = zcc_expand(&subtoks, defines, refs, linecount);
        if (!perm) {
            refs[--refcount] = 0;
        }

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

static string_t zcc_expand_line(const array_t* tokens, const map_t* defines, const size_t linecount)
{
    size_t refs[0xfff];
    zmemset(refs, 0, sizeof(refs));
    string_t s = zcc_expand(tokens, defines, refs, linecount);
    return s;
}

static string_t zcc_ifdef_preexpand(const map_t* defines, ztok_t tok, const size_t linecount)
{
    static const char ifdef[] = "ifdef", ifndef[] = "ifndef", defined[] = "defined";
    
    size_t find;
    string_t s = string_ranged(tok.str, zcc_lexline(tok.str));
    /*zcc_log("<%s>\n", s.data);*/

    if (!zmemcmp(s.data, ifdef, sizeof(ifdef) - 1)) {
        string_remove_range(&s, 0, sizeof(ifdef) - 1);
        string_push_at(&s, "if defined", 0);
    }
    else if (!zmemcmp(s.data, ifndef, sizeof(ifndef) - 1)) {
        string_remove_range(&s, 0, sizeof(ifndef) - 1);
        string_push_at(&s, "if !defined", 0);
    }

    tok = ztok_get(s.data);
    tok = ztok_next(tok);
    while (tok.str) {
        if (*tok.str == '\'') {
            int n = tok.str[1 + (tok.str[1] == '\\')];
            n = zitoa(n, tok.str, 10);
            tok.len = n;
            string_remove_range(&s, tok.str + n - s.data, tok.str + tok.len - s.data);
        }
        else if (chralpha(*tok.str)) {
            if (!zmemcmp(tok.str, defined, sizeof(defined) - 1)) {
                string_remove_range(&s, tok.str - s.data, tok.str + tok.len - s.data);
                tok = ztok_get(tok.str);
                if (*tok.str == '(') {
                    tok = ztok_next(tok);
                }
                find = zcc_defines_search(defines, tok);
                string_remove_range(&s, tok.str + 1 - s.data, tok.str + tok.len - s.data);
                *tok.str = !!find + '0';
                tok.len = 1;
            }
            else {
                find = zcc_defines_search(defines, tok);
                if (find) {
                    /*zcc_logtok("<<<%s>>>\n", tok);*/
                    zmacro_t* m = map_value_at(defines, find - 1);
                    /*zcc_log("{{{%s}}}\n", m->str.data);*/
                    if (*m->str.data == '(') {
                        tok = ztok_next(tok);
                        if (!tok.str || *tok.str != '(') {
                            continue;
                        }
                        char* close = zcc_lexparen(tok.str);
                        if (!close) {
                            zcc_log("Macro function call in #if line does not close parenthesis at line %zu\n", linecount);
                            continue;
                        }
                        tok = ztok_get(close);
                    }
                }
                else {
                    string_remove_range(&s, tok.str + 1 - s.data, tok.str + tok.len - s.data);
                    *tok.str = '0';
                    tok.len = 1;
                }
            }
        }

        tok = ztok_next(tok);
    }

    return s;
}

static long zcc_ifdef_solve(const map_t* defines, ztok_t tok, size_t linecount)
{
    /*zcc_logtok(">%s<}\n", tok);*/
    string_t tmp = zcc_ifdef_preexpand(defines, tok, linecount);
    /*zcc_log("{%s}\n", tmp.data);*/
    array_t a = zcc_tokenize(tmp.data);
    array_remove(&a, 0);

    string_t s = zcc_expand_line(&a, defines, linecount);
    const long n = zcc_solve(s.data);
    /*zcc_log("[%s]\n%ld\n----------\n", s.data, !!n);*/
    
    array_free(&a);
    string_free(&tmp);
    string_free(&s);
    return n;
}

static string_t zcc_ifdef(const map_t* defines, char** linestart, size_t linecount)
{
    static const char ifstr[] = "if", elsestr[] = "else", elif[] = "elif", endif[] = "endif";

    char* lend = zcc_lexline(*linestart), *lstart;
    ztok_t tok = ztok_continue(ztok_get(*linestart), 1);

    string_t s = string_empty();
    long n = zcc_ifdef_solve(defines, tok, linecount);
    long f = !!n;
    long b = f;

    lstart = lend + !!*lend;
    lend = zcc_lexline(lstart);

    size_t scope = 0;
    while (*lend) {
        ++linecount;

        tok = ztok_get(lstart);
        if (!tok.str || *tok.str != '#') {
            goto zlexifdef;
        }

        tok = ztok_next(tok);
        if (!zmemcmp(tok.str, elsestr, sizeof(elsestr) - 1) && !scope) {
            if (!b) {
                b = 1;
                f = 1;
                goto zlexifdefend;
            }
            f = 0;
        }
        else if (!zmemcmp(tok.str, elif, sizeof(elif) - 1) && !scope) {
            if (!b) {
                n = zcc_ifdef_solve(defines, tok, linecount);
                f = !!n;
                b = f;
                goto zlexifdefend;
            }
            f = 0;
        }
        else if (!zmemcmp(tok.str, endif, sizeof(endif) - 1)) {
            if (scope) {
                --scope;
            }
            else break;
        }
        else if (!zmemcmp(tok.str, ifstr, sizeof(ifstr) - 1)) {
            ++scope;
        }

zlexifdef:
        if (f) {
            string_push(&s, zstrbuf(lstart, lend - lstart + 1));
        }
zlexifdefend:
        lstart = lend + !!*lend;
        lend = zcc_lexline(lstart);
    }

    if (!*lend) {
        zcc_log("Missing closing #endif directive at line %zu.\n", linecount);
    }

    *linestart = lstart;
    return s;
}

char* zcc_preprocess_macros(char* src, size_t* size, const char** includes)
{
    static const char inc[] = "include", def[] = "define", ifdef[] = "if", undef[] = "undef";
    static const char warning[] = "warning", error[] = "error";

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
        /*zcc_log(">> %s", zstrbuf(linestart, lineend - linestart + 1));*/
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
                const char* mark = linestart;
                string_t inc = zcc_ifdef(&defines, &linestart, linecount);
                lineend = zcc_lexline(linestart);
                string_remove_range(&text, mark - text.data, lineend - text.data);
                string_push_at(&text, inc.data, i);
                string_free(&inc);
                linestart = text.data + i;
                lineend = zcc_lexline(linestart);
                continue;
            }
            else if (!zmemcmp(tok.str, warning, sizeof(warning) - 1)) {
                zcc_log("%s", zstrbuf(linestart, lineend - linestart + 1));
            }
            else if (!zmemcmp(tok.str, error, sizeof(error) - 1)) {
                zcc_log("%s", zstrbuf(linestart, lineend - linestart + 1));
                zexit(ZCC_EXIT_FAILURE);
            }
            else zcc_log("Illegal macro directive at line %zu.\n%s", linecount, zstrbuf(linestart, lineend - linestart + 1));

            string_remove_range(&text, i, i + lineend - linestart + 1);
            lineend = zcc_lexline(linestart);
            continue;
        }

        array_t linetoks = zcc_tokenize(linestart);
        string_t l = zcc_expand_line(&linetoks, &defines, linecount);
        
        const size_t n = tok.str - text.data;
        string_remove_range(&text, n, n + lineend - tok.str);
        string_push_at(&text, l.data, n);
        linestart = text.data + i;
        lineend = zcc_lexline(linestart);

        array_free(&linetoks);
        string_free(&l);

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