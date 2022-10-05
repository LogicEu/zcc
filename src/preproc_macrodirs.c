#include <preproc.h>
#include <string.h>

#define string_wrap_sized(str, size) (string_t){str, size + 1, size};
#define array_wrap_sized(data, size, bytes) (array_t){data, bytes, size + 1, size};

typedef struct long2 {
    long x, y;
} long2;

static inline long2 long2_add(const long2 dst, const long2 src)
{
    return (long2){dst.x + src.x, dst.y + src.y};
}

static void rangesoffset(const array_t* ranges, const long off, const size_t from)
{
    range_t* n = ranges->data;
    for (size_t i = from; i < ranges->size; ++i) {
        n[i].start += off;
        n[i].end += off;
    }
}

static void ptu_define_free(map_t* defines)
{
    string_t* defstrs = defines->values, *defkeys = defines->keys;
    const size_t defsize = defines->size;
    for (size_t i = 0; i < defsize; ++i) {
        string_free(defstrs + i);
        string_free(defkeys + i);
    }
    map_free(defines);
}

static size_t ptu_remove_line(ptu_t* ptu, const range_t toks, const size_t index)
{
    const range_t line = *(range_t*)array_index(&ptu->lines, index);
    const long dif = line.end - line.start + 1;
    
    string_remove_range(&ptu->text, line.start, line.end + 1);
    array_remove(&ptu->lines, index);
    array_remove_block(&ptu->tokens, toks.start, toks.end);
    
    rangesoffset(&ptu->lines, -dif, index);
    rangesoffset(&ptu->tokens, -dif, toks.start);
    return (size_t)dif;
}

static void ptu_merge(ptu_t* dest, ptu_t* src, const range_t toks, const size_t index)
{
    const range_t line = *(range_t*)array_index(&dest->lines, index);
    const size_t linelen = line.end - line.start + 1;
    const long dif = (long)src->text.size - (long)linelen;

    rangesoffset(&src->lines, line.start, 0);
    rangesoffset(&src->tokens, line.start, 0);
   
    string_remove_range(&dest->text, line.start, line.end + 1);
    array_remove(&dest->lines, index);
    array_remove_block(&dest->tokens, toks.start, toks.end);
   
    rangesoffset(&dest->lines, dif, index);
    rangesoffset(&dest->tokens, dif, toks.start);
     
    string_push_at(&dest->text, src->text.data, line.start); 
    array_push_block_at(&dest->lines, src->lines.data, src->lines.size, index);
    array_push_block_at(&dest->tokens, src->tokens.data, src->tokens.size, toks.start);
   
    ptu_free(src);
}

static size_t ptu_include(ptu_t* ptu, const array_t* includes,
                        const range_t line, const size_t index)
{    
    static char dir[0xfff] = "./";

    ptu_t p;
    char filename[0xfff], buf[0xfff];
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
    
    size_t dirlen;
    if (s[incopt] == '"') {
        dirlen = strlen(dir);
        memcpy(buf, dir, dirlen);
        memcpy(buf + dirlen, filename, len + 1);
        p = ptu_read(buf);
    }
    else if (s[incopt] == '<') {
        const size_t count = includes->size;
        const char** idirs = includes->data;;
        for (size_t i = 0; i < count; ++i) {
            dirlen = strlen(idirs[i]);
            memcpy(dir, idirs[i], dirlen);
            if (dir[dirlen - 1] != '/') {
                dir[dirlen++] = '/';
            }
            
            memcpy(buf, dir, dirlen);
            memcpy(buf + dirlen, filename, len + 1);
            p = ptu_read(buf);
            if (p.text.size) {
                break;
            }
        }
    }

    if (!p.text.size) {
        ppc_log("Could not open header file '%s'.\n", filename);
        return 0;
    }

    ptu_merge(ptu, &p, line, index);
    return 1;
}

static size_t ptu_define(ptu_t* ptu, map_t* defines, 
                        const range_t line, const size_t index)
{
    const range_t* toks = ptu->tokens.data;
    const size_t args = line.start + 3 < line.end - 1 ? line.start + 3 : line.end - 1;
    const range_t k = toks[line.start + 2];
    const range_t d = {toks[args].start, toks[line.end - 1].end};

    string_t key = string_ranged(ptu->text.data + k.start, ptu->text.data + k.end);
    string_t str;
    
    if (memcmp(&k, &d, sizeof(range_t))) {
        str = string_ranged(ptu->text.data + d.start, ptu->text.data + d.end);
        str.data[0] *= !(ptu->text.data[k.end] == '(' && k.end == d.start);
    } 
    else str = string_empty();
    
    map_push(defines, &key, &str);
    ptu_remove_line(ptu, line, index);
    return 1;
}

static size_t ptu_undef(ptu_t* ptu, map_t* defines, 
                        const range_t line, const size_t index)
{
    const range_t k = *(range_t*)array_index(&ptu->tokens, line.start + 2);
    string_t key = string_wrap(strrange(ptu->text.data, k));
    const size_t n = map_search(defines, &key);
    
    if (n) {
        string_t k = *(string_t*)map_key_at(defines, n - 1);
        string_t d = *(string_t*)map_value_at(defines, n - 1);
        map_remove(defines, &key);
        string_free(&k);
        string_free(&d);
    }

    ptu_remove_line(ptu, line, index);
    return 1;
}

static long2 ptu_replace(string_t* sdst, const string_t* ssrc, 
                        array_t* tdst, const array_t* tsrc, const range_t tokrange, const size_t off)
{
    range_t* tokens = tdst->data, *r = tsrc->data;
    const range_t t = {tokens[tokrange.start].start, tokens[tokrange.end - 1].end};
    const size_t tlen = t.end - t.start, hlen = tokrange.end - tokrange.start;

    string_remove_range(sdst, t.start, t.end);
    array_remove_block(tdst, tokrange.start, tokrange.end);

    const long offset = off ? (long)r[off].start : 0;
    const long2 d = {
        (long)tsrc->size - (long)hlen - (long)off,
        (long)ssrc->size - (long)tlen - offset
    };

    rangesoffset(tdst, d.y, tokrange.start);
    string_push_at(sdst, ssrc->data + offset, t.start);
    array_push_block_at(tdst, r + off, tsrc->size - off, tokrange.start);

    tokens = tdst->data;
    const long dif = (long)t.start - (long)tokens[tokrange.start].start;
    for (size_t i = tokrange.start; i < tokrange.start + tsrc->size - off; ++i) {
        tokens[i].start += dif;
        tokens[i].end += dif;
    }

    return d;
}

static long2 ptu_expand_token(ptu_t* ptu, const string_t* str,
                            const size_t index)
{
    const int funcmacro = str->data[0] == 0;
    str->data[0] = funcmacro ? '(' : str->data[0];

    array_t ranges = strtoks(str);
    range_t *tokens = ptu->tokens.data, *r = ranges.data;

    if (!funcmacro) {
        const range_t t = {index, index + 1};
        long2 d = ptu_replace(&ptu->text, str, &ptu->tokens, &ranges, t, 0);
        array_free(&ranges);
        return d;
    }

    const range_t argsparen = parenrange(str->data, 0);
    const range_t argstoks = tokenrange(ranges.data, ranges.size, argsparen);
    const range_t paramsparen = parenrange(ptu->text.data, tokens[index + 1].start);
    const range_t paramstoks = tokenrange(ptu->tokens.data, ptu->tokens.size, paramsparen);
    range_t bodytoks = tokenrange(ranges.data, ranges.size, (range_t){argsparen.end, str->size});
    
    const size_t argcount = (argstoks.end - argstoks.start - 1) / 2;
    if (!argcount) {
        const range_t t = {index, paramstoks.end};
        long2 d = ptu_replace(&ptu->text, str, &ptu->tokens, &ranges, t, bodytoks.start);
        array_free(&ranges);
        str->data[0] = 0;
        return d;
    }

    size_t i, j;
    range_t params[0xff], p = {paramstoks.start + 1, 0};
    for (i = p.start, j = 0; i < paramstoks.end; ++i) {
        const char c = ptu->text.data[tokens[i].start];
        if (c == '(' || c == '{') {
            range_t paren = parenrange(ptu->text.data, tokens[i].start);
            paren = tokenrange(tokens, paramstoks.end, paren);
            i = paren.end - 1;
        }
        else if (c == ',' || c == ')') {
            p.end = i;
            params[j++] = p;
            p.start = i + 1;
        }
    }

    string_t cpy = string_copy(str);
    for (i = bodytoks.start; i < bodytoks.end; ++i) {
        char* ch = cpy.data + r[i].start;
        if (!chralpha(ch[0])) {
            continue;
        }

        size_t found = 0;
        const size_t len = r[i].end - r[i].start;
        for (j = 0; j < argcount; ++j) {
            const size_t k = j * 2 + 1;
            if ((len == r[k].end - r[k].start) && (!memcmp(ch, cpy.data + r[k].start, len))) {
                found = j + 1;
                break;
            }
        }

        if (!found--) {
            continue;
        }
        
        const range_t q = {i, i + 1};
        const range_t f = params[found];
        const range_t l = {tokens[f.start].start, tokens[f.end - 1].end};
        const string_t sdummy = string_wrap_sized(strrange(ptu->text.data, l), l.end - l.start);
        const array_t tdummy = array_wrap_sized(tokens + f.start, f.end - f.start, sizeof(range_t));
        const long2 d = ptu_replace(&cpy, &sdummy, &ranges, &tdummy, q, 0);
        r = ranges.data;

        bodytoks.end += d.x;
        i += d.x;
    }

    const long dif = -(long)(r[bodytoks.start].start - argsparen.start);
    string_remove_range(&cpy, argsparen.start, r[bodytoks.start].start);
    array_remove_block(&ranges, argstoks.start, argstoks.end);
    rangesoffset(&ranges, dif, 0);

    const range_t t = {index, paramstoks.end};
    const long2 d = ptu_replace(&ptu->text, &cpy, &ptu->tokens, &ranges, t, 0);
    string_free(&cpy);
    array_free(&ranges);
    str->data[0] = 0;
    return d;
}

static long2 ptu_expand(ptu_t* ptu, const map_t* defines,
                        range_t linetoks, const size_t index)
{
    size_t i, n;
    long2 ret = {0, 0}, d;
    range_t* lines = ptu->lines.data, *tokens;
    for (i = linetoks.start; i < linetoks.end; ++i) {
        tokens = ptu->tokens.data;
        char* str = strrange(ptu->text.data, tokens[i]);
        if (!(n = map_search(defines, &str))) {
            continue;
        }

        string_t* val = map_value_at(defines, n - 1);
        if (!val->data) {
            continue;
        }

        d = ptu_expand_token(ptu, val, i--);
        ret = long2_add(ret, d);
        linetoks.end += d.x;
        lines[index].end += d.y;
        rangesoffset(&ptu->lines, d.y, index + 1);
    }
    return ret;
}

static long2 ptu_reduce(ptu_t* ptu, const map_t* defines, 
                        range_t linetoks, const size_t index)
{
    static const char* def = "defined";
    
    char buf[0xff];
    array_t a;
    string_t s;
    range_t r, rbuf;
    long2 ret = {0, 0}, d;

    range_t *tokens = ptu->tokens.data, *lines = ptu->lines.data;
    char* str = strrange(ptu->text.data, tokens[linetoks.start + 1]);
    if (!strcmp(str, "ifdef") || !strcmp(str, "ifndef")) {
        strcpy(buf, "if ");
        if (ptu->text.data[tokens[linetoks.start + 1].start + 2] == 'n') {
            strcat(buf, "! ");
        }
        strcat(buf, def);
        s = string_wrap(buf);
        a = strtoks(&s);
        rangesoffset(&a, (long)tokens[linetoks.start + 1].start, 0);
        r = (range_t){linetoks.start + 1, linetoks.start + 2};
        d = ptu_replace(&ptu->text, &s, &ptu->tokens, &a, r, 0);
        ret = long2_add(ret, d);
        linetoks.end += d.x;
        lines[index].end += d.y;
        rangesoffset(&ptu->lines, d.y, index + 1);
        array_free(&a);
    }

    rbuf = (range_t){0, 1};
    a = array_wrap_sized(&rbuf, 1, sizeof(range_t));

    size_t i, n;
    for (i = linetoks.start + 2; i < linetoks.end; ++i) {
        tokens = ptu->tokens.data, lines = ptu->lines.data;
        str = strrange(ptu->text.data, tokens[i]);
        if (!strcmp(str, def)) {
            s = string_wrap_sized(buf, 1);
            int k = (ptu->text.data[tokens[i + 1].start] == '(');
            str = strrange(ptu->text.data, tokens[i + 1 + k]);
            s.data[0] = !!map_search(defines, &str) + '0';
            s.data[1] = 0;
            r = (range_t){i, i + (1 + k) * 2};
            d = ptu_replace(&ptu->text, &s, &ptu->tokens, &a, r, 0);
        }
        else if ((n = map_search(defines, &str))) {
            s = *(string_t*)map_value_at(defines, n - 1);
            if (!s.data) {
                continue;
            }
            d = ptu_expand_token(ptu, &s, i--);
        }
        else continue;

        ret = long2_add(ret, d);
        linetoks.end += d.x;
        lines[index].end += d.y;
        rangesoffset(&ptu->lines, d.y, index + 1);
    }
    return ret;
}

static size_t ptu_ifdef(ptu_t* ptu, const map_t* defines, 
                        range_t linetoks, const size_t index)
{
    static const char* ifstr = "if";

    long2 d = ptu_reduce(ptu, defines, linetoks, index);
    linetoks.end += d.x;

    range_t* lines = ptu->lines.data, *tokens = ptu->tokens.data;
    //ppc_log_tokrange(ptu->text.data, tokens, linetoks);
    bnode_t* ast = tree_parse(ptu->text.data, tokens + linetoks.start + 2, linetoks.end - (linetoks.start + 2));
    long l = tree_eval(ast, ptu->text.data);
    bnode_free(ast);
    int f = !!l;
    int b = f;

    ptu_remove_line(ptu, linetoks, index);

    for (size_t i = index, scope = 0; i < ptu->lines.size; ++i) {
        lines = ptu->lines.data, tokens = ptu->tokens.data;
        linetoks = tokenrange(tokens, ptu->tokens.size, lines[i]);
        //ppc_log("%zu ", f);
        //ppc_log_range(ptu->text.data, lines[i]);
        if (linetoks.start != linetoks.end && ptu->text.data[tokens[linetoks.start].start] == '#') {
            char* str = strrange(ptu->text.data, tokens[linetoks.start + 1]);
            if (!strcmp(str, "else") && !scope) {
                if (!b) {
                    b = f = 1;
                    ptu_remove_line(ptu, linetoks, i--);
                    continue;
                }
                f = 0;
            }
            else if (!strcmp(str, "elif") && !scope) {
                if (!b) {
                    d = ptu_reduce(ptu, defines, linetoks, i);
                    linetoks.end += d.x;
                    tokens = ptu->tokens.data;
                    ast = tree_parse(ptu->text.data, tokens + linetoks.start + 2, linetoks.end - (linetoks.start + 2));
                    l = tree_eval(ast, ptu->text.data);
                    bnode_free(ast);
                    f = !!l;
                    b = f;
                    ptu_remove_line(ptu, linetoks, i--);
                    continue;
                }
                f = 0;
            }
            else if (!strcmp(str, "endif")) {
                if (!scope) {
                    ptu_remove_line(ptu, linetoks, i--);
                    break;
                }
                else --scope;
            }
            else if (!memcmp(str, ifstr, 2)) {
                ++scope;
            }
        }
        
        if (!f) {
            ptu_remove_line(ptu, linetoks, i--);
        }
    }

    return 1;
}

static map_t stddefines(void)
{
    map_t defines = map_create(sizeof(string_t), sizeof(string_t));
    string_t key, value;
    
    key = string_create("__STDC_VERSION__");
    value = string_create("201710");
    map_push(&defines, &key, &value);

    key = string_create("__has_feature");
    value = string_create("(x) 0");
    value.data[0] = 0;
    map_push(&defines, &key, &value);

    return defines;
}

void ptu_preprocess(ptu_t* ptu, const array_t* includes)
{
    static const char* inc = "include", *def = "define", *ifdef = "if", *undef = "undef";
    const size_t inclen = strlen(inc), deflen = strlen(def);
    const size_t ifdeflen = strlen(ifdef), undeflen = strlen(undef);

    map_t defines = stddefines();

    size_t i, j;
    for (i = 0; i < ptu->lines.size; ++i) {
        range_t* lines = ptu->lines.data;
        //ppc_log_range(ptu->text.data, lines[i]);
        if (lines[i].start == lines[i].end) {
            continue;
        }
    
        range_t* tokens = ptu->tokens.data;
        range_t linetoks = tokenrange(tokens, ptu->tokens.size, lines[i]);
        if (linetoks.start == linetoks.end) {
            continue;
        }

        j = tokens[linetoks.start].start;
        if (ptu->text.data[j] == '#') {
            j = tokens[linetoks.start + 1].start;
            if (!memcmp(ptu->text.data + j, inc, inclen)) {
                i -= ptu_include(ptu, includes, linetoks, i);
            }
            else if (!memcmp(ptu->text.data + j, def, deflen)) {
                i -= ptu_define(ptu, &defines, linetoks, i);
            }
            else if (!memcmp(ptu->text.data + j, undef, undeflen)) {
                i -= ptu_undef(ptu, &defines, linetoks, i);
            }
            else if (!memcmp(ptu->text.data + j, ifdef, ifdeflen)) {
                i -= ptu_ifdef(ptu, &defines, linetoks, i);
            }
            continue;
        }

        ptu_expand(ptu, &defines, linetoks, i);

    }
    ptu_define_free(&defines);
}