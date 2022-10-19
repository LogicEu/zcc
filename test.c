#include <zio.h>
#include <zassert.h>
#include <zcc.h>

#define chrstr(c) (((c) == '\'') || ((c) == '"'))
#define chrparen(c) (((c) == '(') || ((c) == '[') || ((c) == '{')) 

int main(int argc, char** argv)
{
    zassert(argc > 1);
    size_t len;
    char* src = zcc_fread(argv[1], &len);
    zassert(src);
    char* p = zstrchr(src, '\n');
    while (*p) {
        p = zstrchr(++p, '\n');
    }
    zfree(src);
    return 0;
}
