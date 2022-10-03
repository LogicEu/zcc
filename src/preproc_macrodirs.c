#include <preproc.h>
#include <string.h>

#define string_wrap_sized(str, size) (string_t){str, size + 1, size};
#define array_wrap_sized(data, size, bytes) (array_t){data, bytes, size + 1, size};

typedef struct long2 {
    long x, y;
} long2;

extern char* strrange(const char* str, const range_t range)
{
    static char buffer[0xfff];
    const size_t len = range.end - range.start;
    memcpy(buffer, str + range.start, len);
    buffer[len] = 0;
    return buffer;
}

extern range_t tokenrange(const range_t* toks, const size_t count, range_t range)
{
    size_t i;
    for (i = 0; i < count && toks[i].start < range.start; ++i);
    for (range.start = i; i < count && toks[i].end <= range.end; ++i);
    range.end = i;
    return range;
}

static inline void ppc_log_range(const char* str, const range_t range)
{
    ppc_log("%s\n", strrange(str, range));
}

static inline void ppc_log_tokrange(const char* str, const range_t* tokens, const range_t range)
{
    for (size_t i = range.start; i < range.end; ++i) {
        ppc_log("'%s' ", strrange(str, tokens[i]));
    }
    ppc_log("\n");
}

static range_t parenrange(const char* str, const size_t index)
{
    char ch = str[index];
    switch (ch) {
        case '{': 
        case '[': ++ch;
        case '(': ++ch;
    }
    
    size_t i, scope;
    for (i = index + 1, scope = 1; scope && str[i]; ++i) {
        scope += (str[i] == str[index]);
        scope -= (str[i] == ch);
    }
    return (range_t){index, i};
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
    string_t str = !memcmp(&k, &d, sizeof(range_t)) ? string_empty() : string_ranged(ptu->text.data + d.start, ptu->text.data + d.end);

    str.data[0] *= !(ptu->text.data[k.end] == '(' && k.end == d.start);
    
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

static size_t ptu_ifdef(ptu_t* ptu, const map_t* defines, 
                        const range_t line, const size_t index)
{
    (void)ptu;
    (void)defines;
    (void)line;
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
    //ppc_log_tokrange(ptu->text.data, tokens, paramstoks);
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

    //ppc_log("%zu\n", j);
    //for (i = 0; i < j; ++i) {
    //    ppc_log_tokrange(ptu->text.data, tokens, params[i]);
    //}

    //return (long2){0, 0};
    string_t cpy = string_copy(str);

    for (i = bodytoks.start; i < bodytoks.end; ++i) {
        //ppc_log("--> ");
        //ppc_log_range(cpy.data, r[i]);
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
        
        //ppc_log("->\n");
        const range_t q = {i, i + 1};
        //ppc_log("%zu, %zu, %zu\n", q.start, q.end, found);
        //ppc_log("%zu, %zu\n", params[found].start, params[found].end);
        const range_t f = params[found];
        //ppc_log("%zu, %zu\n", f.start, f.end);
        const range_t l = {tokens[f.start].start, tokens[f.end - 1].end};
        //ppc_log("%zu, %zu\n", l.start, l.end);
        const string_t sdummy = string_wrap_sized(strrange(ptu->text.data, l), l.end - l.start);
        //ppc_log("%s\n", sdummy.data);
        //ppc_log_tokrange(ptu->text.data, tokens, f);
        //ppc_log_range(ptu->text.data, l);
        //ppc_log("[{%s}] %zu, %zu, %zu\n", sdummy.data, i, l.start, l.end);
        
        const array_t tdummy = array_wrap_sized(tokens + f.start, f.end - f.start, sizeof(range_t));
        const long2 d = ptu_replace(&cpy, &sdummy, &ranges, &tdummy, q, 0);
        //ppc_log("[[%s]]\n", cpy.data);
        r = ranges.data;
        
        /*const range_t t = {r[q.start].start, r[q.end - 1].end};
        string_remove_range(&cpy, t.start, t.end);
        array_remove_block(&ranges, q.start, q.end);

        const long2 d = {
            (long)(tdummy.size) - (long)(q.end - q.start),
            (long)(sdummy.size) - (long)(t.end - t.start)
        };

        rangesoffset(&ranges, d.y, q.start);
        string_push_at(&cpy, sdummy.data, t.start);
        array_push_block_at(&ranges, tdummy.data, tdummy.size, q.start);

        r = ranges.data;
        const long dif = (long)t.start - (long)r[q.start].start;
        for (j = q.start; j < q.start + tdummy.size; ++j) {
            r[j].start += dif;
            r[j].end += dif;
        }*/

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

static void ptu_expand(ptu_t* ptu, const map_t* defines,
                        range_t linetoks, const size_t index)
{
    size_t i, n;
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

        const long2 d = ptu_expand_token(ptu, val, i);
        linetoks.end += d.x;
        lines[index].end += d.y;
        rangesoffset(&ptu->lines, d.y, index + 1);
        i -= !(d.x == 0 && d.y == 0);
    }
}

void ptu_preprocess(ptu_t* ptu, const array_t* includes)
{
    static const char* inc = "include", *def = "define", *ifdef = "if", *undef = "undef";
    const size_t inclen = strlen(inc), deflen = strlen(def);
    const size_t ifdeflen = strlen(ifdef), undeflen = strlen(undef);

    map_t defines = map_create(sizeof(string_t), sizeof(string_t));

    size_t i, j;
    for (i = 0; i < ptu->lines.size; ++i) {
        range_t* lines = ptu->lines.data;
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