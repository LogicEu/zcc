#include <zsys.h>
#include <zstdio.h>
#include <zstdlib.h>
#include <zlexer.h>

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

static void zprinttok(const ztok_t* tok) 
{
    size_t i;
    zputchar('\'');
    for (i = 0; i < tok->len; ++i) {
        zputchar(tok->str[i]);
    }
    zputchar('\'');
    zputchar(tok->str[tok->len] == '\n' ? '\n' : ' ');
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

    size_t i;
    struct vector tokvec = zcc_tokenize(data);
    const ztok_t* tokens = tokvec.data;
    for (i = 0; i < tokvec.size; ++i) {
        zprinttok(tokens + i);
        zputchar(tokens[i].str[tokens[i].len] == '\n' ? '\n' : ' ');
    }

    vector_free(&tokvec);
    zfree(data);
    return Z_EXIT_SUCCESS;
}
