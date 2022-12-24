#include <zsys.h>
#include <zstdio.h>
#include <zstdlib.h>
#include <zstdarg.h>
#include <zio.h>

int zcc_log(const char* fmt, ...)
{
    int ret;
    va_list ap;
    va_start(ap, fmt);
    ret = zvprintf(fmt, ap);
    va_end(ap);
    return ret;
}

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
