#include <zstdlib.h>
#include <zio.h>

int main(int argc, char** argv)
{
    if (argc < 2) {
        zprintf("Missing file.\n");
        return Z_EXIT_FAILURE;
    }
    
    size_t size;
    char* file = zcc_fread(argv[1], &size);
    if (!file) {
        zprintf("Could not open file '%s'.\n");
        return Z_EXIT_FAILURE;
    }

    zprintf("%s\n%zu\n", file, size);
    zfree(file);
    return Z_EXIT_SUCCESS;
}
