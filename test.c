#include <zsys.h>
#include <zstdio.h>
#include <zstdlib.h>

static char* zfile_read(const char* file)
{
    char *data;
    size_t size;
    struct stat st;
    int fd = zopen(file, O_RDONLY);
    if (fd == -1) {
        return NULL;
    }
    
    zfstat(fd, &st);
    size = st.st_size;
    data = zmalloc(size);
    zread(fd, data, size);
    zclose(fd);
    return data;
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        zprintf("Missing input file.\n");
        return Z_EXIT_FAILURE;
    }

    char* data = zfile_read(argv[1]);
    if (!data) {
        zprintf("Could not read file %s\n", argv[1]);
        return Z_EXIT_FAILURE;
    }

    zfree(data);
    return Z_EXIT_SUCCESS;
}
