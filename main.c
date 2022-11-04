#include <zstdlib.h>
#include <zio.h>
#include <zintrinsics.h>
#include <zlexer.h>
#include <zparser.h>
#include <zpreprocessor.h>

int main(const int argc, const char** argv)
{
    int status = Z_EXIT_SUCCESS, ppprint = 0, i;
    array_t infiles = array_create(sizeof(char*));
    array_t predefs = array_create(sizeof(char*));
    array_t includes = zcc_includes_std();

    for (i = 1; i < argc; ++i) {
        if (argv[i][0] == '-') {
            if (argv[i][1] == 'I') {
                const char* ptr = argv[i] + 2;
                array_push(&includes, &ptr);
            }
            if (argv[i][1] == 'D') {
                if (i == argc - 1) {
                    zcc_log("Missing input for option '-D'.\n");
                    return Z_EXIT_FAILURE;
                }
                else array_push(&predefs, &argv[++i]);
            }
            if (argv[i][1] == 'E') {
                ++ppprint;
            }
        } 
        else array_push(&infiles, &argv[i]);
    }

    if (!infiles.size) {
        zcc_log("Missing input file.\n");
        status = Z_EXIT_FAILURE;
        goto exit;
    }

    char* null = NULL;
    array_push(&includes, &null);
    array_push(&predefs, &null);

    size_t len;
    char** filepaths = infiles.data;
    const int filecount = (int)infiles.size;
    for (i = 0; i < filecount; ++i) {
        char* src = zcc_fread(filepaths[i], &len);
        if (src) {
            src = zcc_preprocess_text(src, &len);
            src = zcc_preprocess_macros(src, &len, predefs.data, includes.data);
            if (ppprint) {
                zcc_log("%s\n", src);
            }
            zcc_parse(src);
            zfree(src);
        }
        else zcc_log("zcc could not open translation unit '%s'.\n", filepaths[i]);
    }

exit:
    array_free(&infiles);
    array_free(&includes);
    return status;
}
