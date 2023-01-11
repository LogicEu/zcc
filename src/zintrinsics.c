#include <zstring.h>
#include <ztoken.h>
#include <zintrinsics.h>

/* shared string buffer across zcc */

char* zstrbuf(const char* str, const size_t len)
{
    static char buf[0xffff];
    zmemcpy(buf, str, len);
    buf[len] = 0;
    return buf;
}

/* constant reserved C keywords */

struct hash zcc_keywords_std(void)
{
    static const char* reserved[] = {
        "auto", "break", "case", "char", "const", "continue", "default", 
        "do", "double", "else", "enum", "extern", "float", "for", "goto",
        "if", "int", "long", "register", "return", "short", "signed",
        "sizeof", "static", "struct", "switch", "typedef", "union",
        "unsigned", "void", "volatile", "while"
    };

    size_t i;
    struct hash keywords = hash_create(sizeof(char*));
    for (i = 0; i < sizeof(reserved) / sizeof(reserved[0]); ++i) {
        hash_push(&keywords, reserved + i);
    }
    return keywords;
}

struct vector zcc_includes_std(void)
{
    static const char* stddirs[] = {"/usr/include/", "/usr/local/include/"
#ifdef __APPLE__
        ,("/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/"
        "/usr/lib/clang/13.1.6/include"),
        ("/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/"
        "Developer/SDKs/MacOSX.sdk/usr/include")
#endif
    };

    struct vector includes = vector_create(sizeof(char*));
    vector_push_block(&includes, stddirs, sizeof(stddirs) / sizeof(stddirs[0]));
    return includes;
}

size_t zcc_map_search(const struct map* map, const struct token tok)
{
    const char* s = zstrbuf(tok.str, tok.len);
    return map_search(map, &s);
}

size_t zcc_hash_search(const struct hash* map, const struct token tok)
{
    const char* s = zstrbuf(tok.str, tok.len);
    return hash_search(map, &s);
}
