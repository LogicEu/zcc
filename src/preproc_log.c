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

void ppc_log_range(const char* str, const range_t range)
{
    ppc_log("%s\n", strrange(str, range));
}

void ppc_log_tokrange(const char* str, const range_t* tokens, const range_t range)
{
    for (size_t i = range.start; i < range.end; ++i) {
        ppc_log("'%s' ", strrange(str, tokens[i]));
    }
    ppc_log("\n");
}