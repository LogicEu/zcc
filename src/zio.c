#include <zio.h>
#include <zstdlib.h>
#include <stdio.h>
#include <stdarg.h>

int zcc_log(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int ret = vfprintf(stdout, fmt, args);
    va_end(args);
    return ret;
}

char* zcc_fread(const char* filename, size_t* size)
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