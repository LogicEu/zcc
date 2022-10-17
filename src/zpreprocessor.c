#include <zpreprocessor.h>
#include <zlexer.h>
#include <zstd.h>
#include <utopia/utopia.h>

#if 1
#include <zio.h>
#define ZBUG zcc_log("BUG(%ld)!\n", __LINE__);
#endif

#define chrspace(c) (((c) == ' ') || ((c) == '\n') || ((c) == '\t') || ((c) == '\r'))
#define chrstr(c) (((c) == '\'') || ((c) == '"'))
#define chrparen(c) (((c) == '(') || ((c) == '[') || ((c) == '{')) 
#define chrbetween(c, a, b) (((c) >= (a)) && ((c) <= (b)))
#define chrdigit(c) chrbetween(c, '0', '9')
#define chralpha(c) (chrbetween(c, 'A', 'Z') || chrbetween(c, 'a', 'z') || ((c) == '_'))

#define string_wrap_sized(str, size) (string_t){str, size + 1, size};
#define array_wrap_sized(data, size, bytes) (array_t){data, bytes, size + 1, size};

static void zcc_logtok(const char* str, const size_t len)
{
    size_t i;
    for (i = 0; i < len; ++i) {
        zcc_log("%c", str[i]);
    }
    zcc_log(" ");
}

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

char* zcc_preprocess_macros(char* src, size_t* size, const char** includes)
{
    static const char inc[] = "include", def[] = "define", ifdef[] = "if", undef[] = "undef";

    map_t defines = zcc_std_defines();

    size_t linecount = 0, len, i, find;
    string_t text = string_wrap_sized(src, *size);
    char* linestart = src, *tok, *t;
    char *lineend = zstrchr(linestart, '\n');

    while (lineend) {
        ++linecount;
        i = linestart - src;

        tok = zcc_lex(linestart, &len);
        if (!tok || !len) {
            linestart = lineend + 1;
            lineend = zstrchr(linestart, '\n');
            continue;
        }

        if (*tok == '#') {
            tok = zcc_lex(++tok, &len);
            
            if (!zmemcmp(tok, inc, sizeof(inc) - 1)) {

                static char dir[0xfff] = "./";
                char filename[0xfff], buf[0xfff], *inc = NULL;
                size_t ilen, dlen, flen, j;
                
                tok = zcc_lex(tok + len, &len);
                if (tok && len) {
                    if (*tok == '"') {
                        dlen = zstrlen(dir);
                        flen = len - 2;
                        zmemcpy(filename, tok + 1, flen);
                        filename[flen] = 0;
                        zmemcpy(buf, dir, dlen);
                        zmemcpy(buf + dlen, filename, flen + 1);
                        inc = zcc_fread(buf, &ilen);
                    }
                    else if (*tok == '<') {
                        t = zstrchr(tok, '>');
                        if (t) {
                            flen = t - tok - 1;
                            zmemcpy(filename, tok + 1, flen);
                            filename[flen] = 0;
                            for (j = 0; includes[j] && !inc; ++j) {
                                dlen = zstrlen(includes[j]);
                                zmemcpy(dir, includes[j], dlen);
                                if (dir[dlen - 1] != '/') {
                                    dir[dlen++] = '/';
                                }
                                zmemcpy(buf, dir, dlen);
                                zmemcpy(buf + dlen, filename, flen + 1);
                                inc = zcc_fread(buf, &ilen);
                            }
                        }
                        else zcc_log("Macro #include does not close '>' bracket at line %zu.\n", linecount);
                    }
                    else zcc_log("Macro directive #include must have \"\" or <> symbol at line %zu.\n", linecount);
                }
                
                if (inc && ilen) {
                    zcc_preprocess_text(inc, &ilen);
                    string_push_at(&text, inc, lineend + 1 - src);
                    zfree(inc);
                    src = text.data;
                    linestart = src + i;
                    lineend = zstrchr(linestart, '\n');
                }
                else zcc_log("Could not open header file '%s' at line %zu.\n", filename, linecount);
            }
            else if (!zmemcmp(tok, def, sizeof(def) - 1)) {
                tok = zcc_lex(tok + len, &len);
                if (tok && len) {
                    string_t id = string_ranged(tok, tok + len);
                    string_t def = string_ranged(tok + len, lineend);
                    find = map_push_if(&defines, &id, &def);
                    if (find) {
                        zcc_log("Macro redefinition is not allowed at line %zu.\n", linecount);
                    }
                }
                else zcc_log("Macro #define is empty at line %zu.\n");
            }
            else if (!zmemcmp(tok, undef, sizeof(undef) - 1)) {
                tok = zcc_lex(tok + len, &len);
                if (tok && len) {
                    const char c = tok[len];
                    tok[len] = 0;
                    find = map_search(&defines, &tok);
                    if (find) {
                        string_t k = *(string_t*)map_key_at(&defines, find - 1);
                        string_t d = *(string_t*)map_value_at(&defines, find - 1);
                        map_remove(&defines, &tok);
                        string_free(&k);
                        string_free(&d);
                    }
                    tok[len] = c;
                }
                else zcc_log("Macro #undef is empty at line %zu.\n", linecount);
            }
            else if (!zmemcmp(tok, ifdef, sizeof(ifdef) - 1)) {
                
            }
            /*else {
                zcc_log("Illegal macro directive at line %zu.\n", linecount);
                zcc_logtok(linestart, lineend - linestart);
                zcc_log("\n");
            }*/

            string_remove_range(&text, i, i + lineend - linestart);
            lineend = zstrchr(linestart, '\n');
            continue;
        }

        while (tok) {
            if (chralpha(*tok)) {
                const char c = tok[len];
                tok[len] = 0;
                find = map_search(&defines, &tok);
                tok[len] = c;
                if (find) {
                    string_remove_range(&text, tok - src, tok - src + len);
                    char* macro = *(char**)map_value_at(&defines, find - 1);
                    t = zcc_lex(macro + (*macro == '('), &len);
                    /*zcc_log("{[%s]}\n", t);*/
                    if (*macro != '(' || *t == ')') {
                        string_push_at(&text, t, tok - src);
                        src = text.data;
                        linestart = src + i;
                        lineend = zstrchr(linestart, '\n');
                    }
                    else {

                    }
                }
            }

            tok = zcc_lex(tok + len, &len);
        }

        linestart = lineend + 1;
        lineend = zstrchr(linestart, '\n');
    }

    zcc_defines_free(&defines);
    
    *size = text.size;
    return text.data;
}

char* zcc_preprocess_text(char* str, size_t* size)
{
    char *ch;
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