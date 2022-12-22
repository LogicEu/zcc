#include <zsys.h>
#include <zstdio.h>
#include <zstdlib.h>

static char* zfile_read(const char* file)
{
    char c, *data;
    size_t i = 0;
    int fd = zopen(file, O_RDONLY);
    if (fd == -1) {
        return NULL;
    }
    
    while ((c = zread(fd, &c, 1)) != Z_EOF) {
        ++i;
    }

    data = zmalloc(i);
    zclose(fd);

    fd = zopen(file, O_RDONLY);
    if (fd == -1) {
        zfree(data);
        return NULL;
    }

    zread(fd, data, i);
    zclose(fd);
    return data;
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        zprintf("Missing input file.\n");
        return Z_EXIT_FAILURE;
    }

    char* data = zfile_read(argv[i]);
    if (!data) {
        zprintf("Could not read file %s\n", argv[i]);
        return Z_EXIT_FAILURE;
    }

    zfree(&data);
    return Z_EXIT_SUCCESS;
}
