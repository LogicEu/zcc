#include <preproc.h>
#include <stdio.h>
#include <stdarg.h>

void ppc_log(const char* restrict fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vfprintf(stdout, fmt, args);
    va_end(args);
}