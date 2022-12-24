#ifndef ZCC_IO_H
#define ZCC_IO_H

#include <zstddef.h>

int zcc_log(const char* fmt, ...);
char* zcc_fread(const char* filename, size_t* size);

#endif /* ZCC_IO_H */
