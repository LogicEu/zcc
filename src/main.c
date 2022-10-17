#include <zio.h>
#include <zcc.h>
#include <utopia/utopia.h>

#define ZCC_EXIT_SUCCESS 0
#define ZCC_EXIT_FAILURE 1

static array_t zcc_std_includes(void)
{
    static const char* stddirs[] = {"/usr/include/", "/usr/local/include/",
        ("/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/"
        "/usr/lib/clang/13.1.6/include"),
        ("/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/"
        "Developer/SDKs/MacOSX.sdk/usr/include")
    };

    array_t includes = array_create(sizeof(char*));
    array_push_block(&includes, stddirs, sizeof(stddirs) / sizeof(stddirs[0]));
    return includes;
}

int main(const int argc, const char** argv)
{
    int status = ZCC_EXIT_SUCCESS, ppprint = 0, i;
    array_t infiles = array_create(sizeof(char*));
    array_t includes = zcc_std_includes();

    for (i = 1; i < argc; ++i) {
        if (argv[i][0] == '-') {
            if (argv[i][1] == 'I') {
                const char* ptr = argv[i] + 2;
                array_push(&includes, &ptr);
            }
            if (argv[i][1] == 'E') {
                ++ppprint;
            }
        } 
        else array_push(&infiles, &argv[i]);
    }

    if (!infiles.size) {
        zcc_log("Missing input file.\n");
        status = ZCC_EXIT_FAILURE;
        goto exit;
    }

    char* null = NULL;
    array_push(&includes, &null);

    size_t len;
    char** filepaths = infiles.data;
    const int filecount = (int)infiles.size;
    for (i = 0; i < filecount; ++i) {
        char* src = zcc_fread(filepaths[i], &len);
        if (src) {
            src = zcc_preprocess_text(src, &len);
            src = zcc_preprocess_macros(src, &len, includes.data);
            if (ppprint) {
                zcc_log("%s\n", src);
            }
            zcc_compile(src);
            zfree(src);
        }
        else zcc_log("zcc could not open translation unit '%s'.\n", filepaths[i]);
    }

exit:
    array_free(&infiles);
    array_free(&includes);
    return status;
}
