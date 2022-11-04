#include <stdio.h>
#include <zstddef.h>
#include <zstdlib.h>
#include <zstdio.h>

static char* zfread(const char* filename, size_t* size)
{
    FILE* file = fopen(filename, "rb");
    if (!file) {
        *size = 0;
        return NULL;
    }
                
    fseek(file, 0, SEEK_END);
    size_t len = ftell(file);
    char* buffer = zmalloc(len + 1);
    fseek(file, 0, SEEK_SET);
    fread(buffer, 1, len, file);
    fclose(file);
    buffer[len] = 0;
    *size = len;
    return buffer;
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        zprintf("Missing file.\n");
        return Z_EXIT_FAILURE;
    }
    size_t len;
    char* buf = zfread(argv[1], &len);
    zprintf("%zu\n", len);
    zprintf("%s\n", buf);
    zfree(buf);
    return Z_EXIT_SUCCESS;
}
