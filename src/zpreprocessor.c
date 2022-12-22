#include <zpreprocessor.h>
#include <zlexer.h>
#include <zsolver.h>
#include <zio.h>
#include <zstdlib.h>
#include <zdbg.h>
#include <zstring.h>
#include <zintrinsics.h>

int zcc_printdefines = 0;
int zcc_precomments = 1;

/* some useful string manipulation helpers */

#define string_wrap_sized(str, size) (string_t){str, size + 1, size};
#define vector_wrap_sized(data, size, bytes) (vector_t){data, bytes, size + 1, size};

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
        while (len && _isspace(string->data[len])) {
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

static string_t zcc_concatenate(const ztok_t t1, const ztok_t t2)
{
    string_t s = string_ranged(t1.str, t1.str + t1.len);
    string_push_tok(&s, t2);
    return s;
}

static void string_insert_before(string_t* string, const char mark, const char insert)
{
    size_t i;
    const char in[2] = {insert, 0};
    for (i = 0; i < string->size; ++i) {
        if (string->data[i] == mark) {
            string_push_at(string, in, i++);
        }
    }
}

/* macro handler struct */

typedef struct zmacro_t {
    string_t str;
    vector_t args;
    vector_t body;
} zmacro_t;

static int zmacro_args(vector_t* args, string_t* string, ztok_t tok, const size_t linecount)
{
    static const char vdots[] = "...", vargs[] = "__VA_ARGS__";

    /*zcc_log("->%s\n", tok.str);*/
    while (tok.str && *tok.str != ')') {
        if (*tok.str == ',') {
            tok = ztok_next(tok);
            if (*tok.str == ')' || *tok.str == ',') {
                zcc_log("Macro function definition does not allow empty argument parameter at line %zu.\n", linecount);
                return Z_EXIT_FAILURE;
            }
            continue;
        }
        
        if (!zmemcmp(tok.str, vdots, sizeof(vdots) - 1)) {
            if (tok.str[sizeof(vdots) - 1] != ')') {
                zcc_log("Missing ')' in macro parameter list at line %zu.\n", linecount);
                return Z_EXIT_FAILURE;
            }
            size_t n = tok.str - string->data;
            string_remove_range(string, n, n + sizeof(vdots) - 1);
            string_push_at(string, vargs, n);
            tok.kind = ZTOK_ID;
            tok.str = string->data + n;
            tok.len = sizeof(vargs) - 1;
        }
        else if (!_isid(*tok.str)) {
            zcc_log("Macro function definition only allows valid identifiers as argument parameter at line %zu.\n", linecount);
            /*zcc_log("%s\n", tok.str);*/
            zabort();
            return Z_EXIT_FAILURE;
        }
        /*zcc_logtok("<%s>\n", tok);*/
        vector_push(args, &tok);
        tok = ztok_next(tok);
    }

    if (!tok.str || *tok.str != ')') {
        zcc_log("Macro function definition does not close parenthesis at line %zu.\n", linecount);
        return Z_EXIT_FAILURE;
    }

    return Z_EXIT_SUCCESS;
}

static void zmacro_free(zmacro_t* macro)
{
    string_free(&macro->str);
    vector_free(&macro->args);
    vector_free(&macro->body);
}

static zmacro_t zmacro_create(const char* str, const size_t linecount)
{
    zmacro_t macro;
    macro.str = string_create(str);
    macro.args = vector_create(sizeof(ztok_t));
    macro.body = vector_create(sizeof(ztok_t));

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
        tok = ztok_next(*(ztok_t*)vector_peek(&macro.args));
    }
    else tok = ztok_next(tok);

    tok = ztok_next(tok);
    macro.body = zcc_tokenize(tok.str);
    return macro;
}

/* functions to handle define macros */

int zcc_defines_push(map_t* defines, const char* keystr, const char* valstr)
{
    string_t key = string_create(keystr);
    zmacro_t body = zmacro_create(valstr, 0);
    if (map_push_if(defines, &key, &body)) {
        zcc_log("Macro redefinition is not allowed (%s).\n", keystr);
        string_free(&key);
        zmacro_free(&body);
        return Z_EXIT_FAILURE;
    }
    return Z_EXIT_SUCCESS;
}

map_t zcc_defines_std(void)
{
    map_t defines = map_create(sizeof(string_t), sizeof(zmacro_t));

    zcc_defines_push(&defines, "__STDC__", "1");
    zcc_defines_push(&defines, "__STDC_HOSTED__", "0");
    zcc_defines_push(&defines, "__STDC_VERSION__", "201710");
    zcc_defines_push(&defines, "__WCHAR_MAX__", "2147483647");
    zcc_defines_push(&defines, "__has_feature", "(x) 0");
    zcc_defines_push(&defines, "__has_include", "(x) 0");
    zcc_defines_push(&defines, "__has_include_next", "(x) 0");

#ifdef __APPLE__    
    zcc_defines_push(&defines, "__APPLE__", "1");
#elif __linux
    zcc_defines_push(&defines, "__linux", "1");
#endif

#ifdef __x86_64__
    zcc_defines_push(&defines, "__x86_64__", "1");
#elif __arm__
    zcc_defines_push(&defines, "__arm__", "1");
#endif

    return defines;
}

void zcc_defines_free(map_t* defines, const size_t from)
{
    size_t i;
    const size_t count = defines->size;
    string_t* keys = defines->keys;
    zmacro_t* defs = defines->values;
    for (i = from; i < count; ++i) {
        /*zcc_log("%s -> %s\n", keys[i].data, defs[i].str.data);*/
        string_free(keys + i);
        zmacro_free(defs + i);
    }
    map_free(defines);
}

/* preprocessing directive function implementations */

int zcc_defines_undef(map_t* defines, const char* key)
{
    const size_t find = map_search(defines, &key);
    if (!find) {
        return Z_EXIT_FAILURE;
    }

    string_t k = *(string_t*)map_key_at(defines, find - 1);
    zmacro_t d = *(zmacro_t*)map_value_at(defines, find - 1);

    map_remove(defines, &k);
    string_free(&k);
    zmacro_free(&d);

    return Z_EXIT_SUCCESS;
}

static int zcc_undef(map_t* defines, ztok_t tok, const size_t linecount)
{
    tok = ztok_nextl(tok);
    if (!tok.str) {
        zcc_log("Macro #undef is empty at line %zu.\n", linecount);
        return Z_EXIT_FAILURE;
    }
    
    return zcc_defines_undef(defines, zstrbuf(tok.str, tok.len));
}

static int zcc_define(map_t* defines, ztok_t tok)
{
    tok = ztok_nextl(tok);
    if (!tok.str) {
        zcc_log("Macro #define is empty at line %zu.\n");
        return Z_EXIT_FAILURE;
    }

    char* end = zstrchr(tok.str, '\n'), buf[0xfff];
    end = !end ? tok.str + zstrlen(tok.str) : end;

    const char* linestr = zstrbuf(tok.str, end - tok.str);
    tok = ztok_get(linestr);

    zmemcpy(buf, tok.str, tok.len);
    buf[tok.len] = 0;

    return zcc_defines_push(defines, buf, tok.str + tok.len);
}

static ztok_t zcc_include(const char** includes, ztok_t tok, const size_t linecount)
{
    static char dir[0xfff] = "./";
    char filename[0xfff], buf[0xfff];
    size_t dlen, flen;
    ztok_t inc = {NULL, 0, ZTOK_SRC};
    
    tok = ztok_nextl(tok);
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
        zcc_log("Macro directive #include must have \"\" or <> symbol at line %zu.'%s'\n", linecount, zstrbuf(tok.str, tok.len));
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

static string_t zcc_stringify(const vector_t* args)
{
    size_t i;
    const ztok_t* toks = args->data;
    string_t str = string_create("\"");
    for (i = 0; i < args->size; ++i) {
        
        string_t s = string_ranged(toks[i].str, toks[i].str + toks[i].len);
        switch (toks[i].str[0]) {
            case '"':
                string_insert_before(&s, '\\', '\\');
                string_insert_before(&s, '"', '\\');
                break;
            case '\'':
                string_insert_before(&s, '\\', '\\');
        }

        string_concat(&str, &s);
        string_free(&s);

        if (i + 1 < args->size && toks[i + 1].str > toks[i].str + toks[i].len) {
            string_push(&str, " ");
        }
    }
    string_push(&str, "\"");
    return str;
}

size_t zcc_macro_search(const vector_t* params, const ztok_t tok)
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

static string_t zcc_expand(const vector_t* tokens, const map_t* defines, size_t* refs, const size_t linecount)
{
    zassert(refs);
    size_t refcount;
    for (refcount = 0; refs[refcount]; ++refcount);

    const size_t perm = refcount ? ((long)refs[refcount - 1] == -1) : 0;
    if (perm) {
        refs[--refcount] = 0;
    }
    
    size_t i, j;
    string_t line = string_empty();
    ztok_t* toks = tokens->data;
    const size_t count = tokens->size;   
    
    for (i = 0; i < count; ++i) {
        
        if (refcount && i + 2 < count && toks[i + 1].str[0] == '#' && toks[i + 1].str[1] == '#') {
            string_t s = zcc_concatenate(toks[i], toks[i + 2]);
            string_remove_trail(&line);
            string_concat(&line, &s);
            string_free(&s);
            i += 2;
            goto zlexspace;
        }

        if (!_isid(*toks[i].str)) {
            goto zlextok;
        }

        const size_t find = zcc_map_search(defines, toks[i]);
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
        
        vector_t args = vector_create(sizeof(vector_t));

        j = ++i;
        while (j < count && toks[j].str + toks[j].len < close) {
            /*zcc_log("'%p' -->> '%zu'\n", toks[j].str, toks[j].len);*/
            if (_isparen(*toks[j].str)) {
                close = zcc_lexparen(toks[j].str);
                while (j < count && toks[j].str < close) {
                    ++j;
                }
            }

            const int n = (toks[j].str < close && j + 1 < count && toks[j + 1].str + toks[j + 1].len >= close);
            if (*toks[j].str == ',' || toks[j].str >= close || j + 1 >= count || n) {
                j += n;
                vector_t arg = vector_wrap_sized(toks + i, j - i, sizeof(ztok_t));
                vector_push(&args, &arg);
                i = j + 1;
            }
            ++j;
        }
        i -= !!args.size;


        if (args.size != macro->args.size) {
            ztok_t t = ztok_get("__VA_ARGS__");
            size_t found = zcc_macro_search(&macro->args, t);
            if (found) {
                vector_t* argarr = args.data;
                while (found < args.size) {
                    const size_t size = (size_t)(argarr[found].data - argarr[found - 1].data + argarr[found].size * argarr[found].bytes) / argarr[found].bytes;
                    argarr[found - 1].size = size;
                    argarr[found - 1].capacity = size;
                    vector_remove(&args, found);
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
        const vector_t* argstrs = args.data;
        
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
        
        vector_t subtoks = zcc_tokenize_line(subst.data);
        refs[refcount++] = find;
        string_t s = zcc_expand(&subtoks, defines, refs, linecount);
        if (!perm) {
            refs[--refcount] = 0;
        }

        string_concat(&line, &s);
        
        string_free(&s);
        string_free(&subst);
        vector_free(&subtoks);
        vector_free(&args);
        
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

static string_t zcc_expand_line(const vector_t* tokens, const map_t* defines, const size_t linecount)
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
    tok = ztok_nextl(tok);
    while (tok.str) {
        /*zcc_logtok("[%s]\n", tok);*/
        if (*tok.str == '\'') {
            int n = tok.str[1 + (tok.str[1] == '\\')];
            n = zitoa(n, tok.str, 10);
            tok.len = n;
            string_remove_range(&s, tok.str + n - s.data, tok.str + tok.len - s.data);
        }
        else if (_isid(*tok.str)) {
            if (!zmemcmp(tok.str, defined, sizeof(defined) - 1)) {
                string_remove_range(&s, tok.str - s.data, tok.str + tok.len - s.data);
                tok = ztok_get(tok.str);
                if (*tok.str == '(') {
                    tok = ztok_nextl(tok);
                }
                find = zcc_map_search(defines, tok);
                string_remove_range(&s, tok.str + 1 - s.data, tok.str + tok.len - s.data);
                *tok.str = !!find + '0';
                tok.len = 1;
            }
            else {
                find = zcc_map_search(defines, tok);
                if (find) {
                    /*zcc_logtok("<<<%s>>>\n", tok);*/
                    zmacro_t* m = map_value_at(defines, find - 1);
                    /*zcc_log("{{{%s}}}\n", m->str.data);*/
                    if (*m->str.data == '(') {
                        tok = ztok_nextl(tok);
                        if (!tok.str || *tok.str != '(') {
                            continue;
                        }
                        char* close = zcc_lexparen(tok.str) - 1;
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

        tok = ztok_nextl(tok);
    }

    return s;
}

static long zcc_ifdef_solve(const map_t* defines, ztok_t tok, size_t linecount)
{
    /*zcc_logtok(">%s<\n", tok);*/
    string_t tmp = zcc_ifdef_preexpand(defines, tok, linecount);
    /*zcc_log("{%s}\n", tmp.data);*/
    vector_t a = zcc_tokenize_line(tmp.data);
    vector_remove(&a, 0);

    string_t s = zcc_expand_line(&a, defines, linecount);
    const long n = zcc_solve(s.data);
    /*zcc_log("[%s]\n%ld\n----------\n", s.data, !!n);*/
    
    vector_free(&a);
    string_free(&tmp);
    string_free(&s);
    return n;
}

static string_t zcc_ifdef(const map_t* defines, char** linestart, size_t linecount)
{
    static const char ifstr[] = "if", elsestr[] = "else", elif[] = "elif", endif[] = "endif";

    char* lend = zcc_lexline(*linestart), *lstart;
    ztok_t tok = ztok_nextl(ztok_get(*linestart));

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

        tok = ztok_nextl(tok);
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

/* 
preprocessor:

1.- Textual Preprocessing:
--> Discard comments, do line slicing, replace dirigraphs...

2.- Macro Directives:
--> For every line in preprocessed text
---> if first_token == '#'
----> macro_directive(second_token)
---> else

*/
#if 0
char* zcc_preprocess_macros(char* src, size_t* size, const map_t* defs, const char** includes)
{
    static const char inc[] = "include", def[] = "define", ifdef[] = "if", undef[] = "undef";
    static const char warning[] = "warning", error[] = "error";

    map_t defines = map_copy(defs);

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
            tok = ztok_nextl(tok);
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
                zcc_define(&defines, tok);
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
                zexit(Z_EXIT_FAILURE);
            }
            else zcc_log("Illegal macro directive at line %zu.\n%s", linecount, zstrbuf(linestart, lineend - linestart + 1));

            string_remove_range(&text, i, i + lineend - linestart + 1);
            lineend = zcc_lexline(linestart);
            continue;
        }

        vector_t linetoks = zcc_tokenize_line(linestart);
        string_t l = zcc_expand_line(&linetoks, &defines, linecount);

        const size_t n = tok.str - text.data;
        string_remove_range(&text, n, n + lineend - tok.str);
        string_push_at(&text, l.data, n);
        linestart = text.data + i;
        lineend = zcc_lexline(linestart);

        vector_free(&linetoks);
        string_free(&l);

zlexline:
        linestart = lineend + !!*lineend;
        lineend = zcc_lexline(linestart);
    }

    zcc_defines_free(&defines, defs->size);
    *size = text.size;
    return text.data;
}
#else
static char* zcc_preprocess_directive(string_t* text, map_t* defines, const char** includes, char* linestart, size_t linecount)
{
    static const char inc[] = "include", def[] = "define", ifdef[] = "if", undef[] = "undef";
    static const char warning[] = "warning", error[] = "error";

    const size_t index = linestart - text->data;
    const char* lineend = zcc_lexline(linestart);

    ztok_t tok = ztok_get(linestart);
    tok = ztok_nextl(tok);

    if (!zmemcmp(tok.str, inc, sizeof(inc) - 1)) {
        ztok_t inc = zcc_include(includes, tok, linecount);
        if (inc.str) {
            zcc_preprocess_text(inc.str, &inc.len);
            string_push_at(text, inc.str, lineend + 1 - text->data);
            zfree(inc.str);
            linestart = text->data + index;
            lineend = zcc_lexline(linestart);
        }
    }
    else if (!zmemcmp(tok.str, def, sizeof(def) - 1)) {
        if (zcc_printdefines) {
            zcc_log("%s\n", zstrbuf(linestart, lineend - linestart));
        }
        zcc_define(defines, tok);
    }
    else if (!zmemcmp(tok.str, undef, sizeof(undef) - 1)) {
        zcc_undef(defines, tok, linecount);
    }
    else if (!zmemcmp(tok.str, ifdef, sizeof(ifdef) - 1)) {
        string_t inc = zcc_ifdef(defines, &linestart, linecount);
        lineend = zcc_lexline(linestart);
        string_remove_range(text, index, lineend - text->data);
        string_push_at(text, inc.data, index);
        string_free(&inc);
        return text->data + index;
    }
    else if (!zmemcmp(tok.str, warning, sizeof(warning) - 1)) {
        zcc_log("%s", zstrbuf(linestart, lineend - linestart + 1));
    }
    else if (!zmemcmp(tok.str, error, sizeof(error) - 1)) {
        zcc_log("%s", zstrbuf(linestart, lineend - linestart + 1));
        zexit(Z_EXIT_FAILURE);
    }
    else {
        zcc_log("Illegal macro directive at line %zu.\n%s", linecount, zstrbuf(linestart, lineend - linestart + 1));
        zexit(Z_EXIT_FAILURE);
    }

    string_remove_range(text, index, index + lineend - linestart + 1);
    return linestart;
}

static char* zcc_preprocess_expand(string_t* text, const map_t* defines, const char* linestart, size_t linecount)
{
    const size_t index = linestart - text->data;
    char* lineend = zcc_lexline(linestart);
    vector_t linetoks = zcc_tokenize_line(linestart);

    size_t refs[0xfff];
    zmemset(refs, 0, sizeof(refs));
    string_t l = zcc_expand(&linetoks, defines, refs, linecount);

    const ztok_t tok = ztok_get(linestart);
    const size_t n = tok.str - text->data;
    string_remove_range(text, n, n + lineend - tok.str);
    
    string_push_at(text, l.data, n);
    vector_free(&linetoks);
    string_free(&l);
    return text->data + index;
}

char* zcc_preprocess_macros(char* src, size_t* size, const map_t* defs, const char** includes)
{
    const size_t defsize = defs->size;
    map_t defines = map_copy(defs);

    size_t linecount = 0;
    string_t text = string_wrap_sized(src, *size);
    char* linestart = text.data;
    char* lineend = zcc_lexline(text.data);
    ztok_t tok;
 
    while (*lineend) {
        zcc_log(">> %s", zstrbuf(linestart, lineend - linestart + 1));
        ++linecount;
        tok = ztok_get(linestart);
        if (tok.str) {
            if (*tok.str == '#') {
                linestart = zcc_preprocess_directive(&text, &defines, includes, linestart, linecount);
                lineend = zcc_lexline(linestart);
                continue;
            } else {
                linestart = zcc_preprocess_expand(&text, &defines, linestart, linecount);
            }
            lineend = zcc_lexline(linestart);
        }
        linestart = lineend + !!*lineend;
        lineend = zcc_lexline(linestart);
    }

    zcc_defines_free(&defines, defsize);
    *size = text.size;
    return text.data;
}
#endif
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
                if (zcc_precomments) {
                    string_remove_range(&text, i, i + ch - (str + i));
                }
                else i = i + ch - (str + i) - 1;
            }
            else if (str[i + 1] == '*') {
                ch = zstrstr(str + i + 2, "*/");
                if (!ch) {
                    zcc_log("Comment is not closed on line %zu.\n", linecount + 1);
                    str[i] = 0;
                    return text.data;
                }
                if (zcc_precomments) {
                    string_remove_range(&text, i + 1, i + ch - (str + i) + 2);
                    str[i] = ' ';
                }
                else i = i + ch - (str + i) + 1;
            }
            break;
        }
    }

    *size = text.size;
    return text.data;
}