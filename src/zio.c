#include <zsys.h>
#include <zstdlib.h>
#include <zio.h>

char* zcc_fread(const char* path, size_t* size)
{
    void* data = NULL;
    size_t len = 0;
    int fd = zopen(path, O_RDONLY);
    if (fd > STDERR_FILENO) {
        struct stat st;
        zfstat(fd, &st);
        len = (size_t)st.st_size;
        data = zmalloc(len);
        if (len && data) {
            zread(fd, data, len);
        }
        zclose(fd);
    }
    *size = len;
    return data;
}