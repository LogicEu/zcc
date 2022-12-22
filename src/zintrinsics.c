#include <zintrinsics.h>
#include <zstring.h>

/* shared string buffer across zcc */

char* zstrbuf(const char* str, const size_t len)
{
    static char buf[0xffff];
    zmemcpy(buf, str, len);
    buf[len] = 0;
    return buf;
}

vector_t zcc_includes_std(void)
{
    static const char* stddirs[] = {"/usr/include/", "/usr/local/include/"
#ifdef __APPLE__
        ,("/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/"
        "/usr/lib/clang/13.1.6/include"),
        ("/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/"
        "Developer/SDKs/MacOSX.sdk/usr/include")
#endif
    };

    vector_t includes = vector_create(sizeof(char*));
    vector_push_block(&includes, stddirs, sizeof(stddirs) / sizeof(stddirs[0]));
    return includes;
}

size_t zcc_map_search(const map_t* map, const ztok_t tok)
{
    const char* s = zstrbuf(tok.str, tok.len);
    return map_search(map, &s);
}

size_t zcc_hash_search(const hash_t* map, const ztok_t tok)
{
    const char* s = zstrbuf(tok.str, tok.len);
    return hash_search(map, &s);
}