#include <zstdlib.h>
#include <zstring.h>
#include <zio.h>
#include <zintrinsics.h>
#include <zlexer.h>
#include <zparser.h>
#include <zpreprocessor.h>
#include <zassert.h>

extern int zcc_precomments;
extern int zcc_printdefines;
extern void zmalloc_inspect(void);

static int zcc_defines_define(struct map* defines, const char* str)
{
    char* eq, *s;
    struct token tok = ztok_get(str);
    eq = zstrchr(str, '=');
    s = zstrbuf(tok.str, tok.len);

    if (eq) {
        *eq = ' ';
        zcc_log("%s, %s\n", s, tok.str + tok.len);
        return zcc_defines_push(defines, s, tok.str + tok.len);
    }
    
    zcc_log("%s, %s\n", s, "1");
    return zcc_defines_push(defines, s, "1");
}

int main(const int argc, const char** argv)
{
    size_t len;
    char* src;
    const char* null = NULL, **filepaths;
    int i, filecount, status = Z_EXIT_SUCCESS;
    int ppprint = 0, printdefs = 0, preproc = 1;
    
    struct vector infiles, includes;
    struct map defines = zcc_defines_std();

    infiles = vector_create(sizeof(char*));
    includes = zcc_includes_std();

    for (i = 1; i < argc; ++i) {
        if (argv[i][0] == '-') {
            if (argv[i][1] == 'I') {
                const char* ptr = argv[i] + 2;
                vector_push(&includes, &ptr);
            }
            else if (argv[i][1] == 'D') {
                if (i == argc - 1) {
                    zcc_log("Missing input for option '%s'.\n", argv);
                    return Z_EXIT_FAILURE;
                }
                else zcc_defines_define(&defines, argv[++i]);
            }
            else if (argv[i][1] == 'U') {
                if (i == argc - 1) {
                    zcc_log("Missing input for option '%s'.\n", argv);
                    return Z_EXIT_FAILURE;
                }
                else zcc_defines_undef(&defines, argv[++i]);
            }
            else if (argv[i][1] == 'E') {
                ppprint = 1;
            }
            else if (argv[i][1] == 'C') {
                zcc_precomments = 0;
            }
            else if (argv[i][1] == 'd' && argv[i][2] == 'M') {
                printdefs = 1;
            }
            else if (!zstrcmp(argv[i] + 1, "undef")) {
                zcc_defines_free(&defines, 0);
            }
            else if (!zstrcmp(argv[i] + 1, "fpreprocessed")) {
                preproc = 0;
            }
        } 
        else vector_push(&infiles, &argv[i]);
    }

    vector_push(&includes, &null);

    if (!infiles.size) {
        zcc_log("Missing input file.\n");
        status = Z_EXIT_FAILURE;
        goto exit;
    }

    if (ppprint && printdefs) {
        zcc_printdefines = 1;
        ppprint = 0;
    }
    
    /* zatexit(&zmalloc_inspect); */
    
    filepaths = infiles.data;
    filecount = (int)infiles.size;
    for (i = 0; i < filecount; ++i) {
        src = zcc_fread(filepaths[i], &len);
        if (src) {
            struct treenode* ast;
            src = zcc_preprocess_text(src, &len);
            if (preproc) {
                src = zcc_preprocess_macros(src, &len, &defines, includes.data);
            }

            if (ppprint) {
                zcc_log("%s\n", src);
            }

            ast = zparse_source(src);
            if (ast) {
                zparse_tree_print(ast, 0);
                zparse_free(ast);
            }
            zfree(src);
        }
        else zcc_log("zcc could not open translation unit '%s'.\n", filepaths[i]);
    }

exit:
    zcc_defines_free(&defines, 0);
    vector_free(&infiles);
    vector_free(&includes);
    return status;
}
