#ifndef ZCC_IO_H
#define ZCC_IO_H

#include <zstddef.h>
#include <zstdio.h>

#define zcc_log(...) zprintf(__VA_ARGS__)

char* zcc_fread(const char* filename, size_t* size);

#endif