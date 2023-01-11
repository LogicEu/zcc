#include <zstdlib.h>
#include <zlexer.h>
#include <ztoken.h>
#include <zparser.h>
#include <zio.h>

int main(const int argc, const char** argv)
{
    size_t len;
    char* module, *end = NULL;
    const char* path;
    struct treenode* ast;

    if (argc < 2) {
        zcc_log("Missing input file.\n");
        return Z_EXIT_FAILURE;
    }

    path = argv[1];
    module = zcc_fread(path, &len);
    if (!module) {
        zcc_log("Could not open file '%s'\n", path);
        return Z_EXIT_FAILURE;
    }

    ast = zparse_module(module, &end);
    if (ast) {
        zparse_tree_print(ast, 0);
        zparse_free(ast);
    }

    zfree(module);
    return Z_EXIT_SUCCESS;
}
