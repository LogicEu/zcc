#include <preproc.h>

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

extern char* strrange(const char* str, const range_t range);
extern range_t tokenrange(const range_t* toks, const size_t count, range_t range);

static array_t stdincludes(void)
{
    static const char* stddirs[] = {"/usr/include/", "/usr/local/include/",
        ("/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/"
        "Developer/SDKs/MacOSX.sdk/usr/include")
    };

    array_t includes = array_create(sizeof(char*));
    array_push_block(&includes, stddirs, sizeof(stddirs) / sizeof(stddirs[0]));
    return includes;
}

static void ptu_print(const ptu_t* ptu)
{
    const char* s = ptu->text.data;
    const size_t linecount = ptu->lines.size, tokencount = ptu->tokens.size;
    const range_t* lines = ptu->lines.data, *tokens = ptu->tokens.data;
    for (size_t i = 0; i < linecount; ++i) {
        const range_t toks = tokenrange(tokens, tokencount, lines[i]);
        for (size_t j = toks.start; j < toks.end; ++j) {
            ppc_log("'%s' ", strrange(s, tokens[j]));
        }
        ppc_log("\n");
        //ppc_log("'%s'\n", strrange(s, lines[i]));
    }
}

int main(const int argc, const char** argv)
{
    int status = EXIT_SUCCESS, ppprint = 0;
    array_t infiles = array_create(sizeof(char*));
    array_t includes = stdincludes();

    for (int i = 1; i < argc; ++i) {
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
        ppc_log("Missing input file.\n");
        status = EXIT_FAILURE;
        goto exit;
    }

    char** filepaths = infiles.data;
    const size_t filecount = infiles.size;
    for (size_t i = 0; i < filecount; ++i) {
        ptu_t ptu = ptu_read(filepaths[i]);
        if (ptu.text.size) {
            ptu_preprocess(&ptu, &includes);
            if (ppprint) {
                ptu_print(&ptu);
                ppc_log("%s\n", ptu.text.data);
            }
            ptu_free(&ptu);
        }
        else ppc_log("Could not open file '%s'.\n", filepaths[i]);
    }

exit:
    array_free(&infiles);
    array_free(&includes);
    return status ;
}
