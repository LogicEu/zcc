#include <preproc.h>
#include <string.h>

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

static void rangesoffset(const array_t* ranges, const long off, const size_t from)
{
    range_t* n = ranges->data;
    for (size_t i = from; i < ranges->size; ++i) {
        n[i].start += off;
        n[i].end += off;
    }
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
    
    //ppc_log("#define %zu, %zu, %p -> '%s' -> '%s'\n", str.capacity, str.size, str.data, key.data, str.data);
    
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
        //ppc_log("#undef %zu, %zu, %p -> '%s'-> '%s'\n", k.capacity, k.size, k.data, k.data, d.data);
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

void ptu_preprocess(ptu_t* ptu, const array_t* includes)
{
    static const char* inc = "include", *def = "define", *ifdef = "if", *undef = "undef";
    const size_t inclen = strlen(inc), deflen = strlen(def);
    const size_t ifdeflen = strlen(ifdef), undeflen = strlen(undef);

    map_t defines = map_create(sizeof(string_t), sizeof(string_t));

    size_t i, j, n;
    for (i = 0; i < ptu->lines.size; ++i) {
        range_t* lines = ptu->lines.data;

        //ppc_log("%s\n", strrange(ptu->text.data, lines[i]));

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

        for (j = linetoks.start; j < linetoks.end; ++j) {
            tokens = ptu->tokens.data;
            char* str = strrange(ptu->text.data, tokens[j]);
            if (!(n = map_search(&defines, &str))) {
                continue;
            }

            string_t* val = map_value_at(&defines, n - 1);
            if (!val->data) {
                continue;
            }

            ppc_log("'%s' -> '%s'\n", str, val->data);

            array_t ranges = strtoks(val);

            if (val->data[0] == '(') {
                //for (n = 1; n < ranges.size; ++n) {
                //    tok = strrange(val->data, r[n]);  
                //}
            } else {
                const range_t t = tokens[j];
                string_remove_range(&ptu->text, t.start, t.end);
                array_remove(&ptu->tokens, j);

                const long dif = (long)val->size - (long)(t.end - t.start);
                lines[i].end += dif;
                linetoks.end += ranges.size - 1;
                rangesoffset(&ptu->lines, dif, i + 1);
                rangesoffset(&ptu->tokens, dif, j);
                rangesoffset(&ranges, t.start, 0);

                string_push_at(&ptu->text, val->data, t.start);
                array_push_block_at(&ptu->tokens, ranges.data, ranges.size, j);
            }

            array_free(&ranges);
        }
    }

    string_t* defstrs = defines.values, *defkeys = defines.keys;
    const size_t defsize = map_size(&defines);
    for (i = 0; i < defsize; ++i) {
        //ppc_log("Map %zu: '%s' -> '%s'\n", i, defkeys[i].data, defstrs[i].data);
        string_free(defstrs + i);
        string_free(defkeys + i);
    }
    map_free(&defines);
}