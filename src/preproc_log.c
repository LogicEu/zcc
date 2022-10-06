#include <preproc.h>
#include <stdio.h>
#include <stdarg.h>

void ppc_log(const char* fmt, ...)
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
    size_t i;
    for (i = range.start; i < range.end; ++i) {
        ppc_log("'%s' ", strrange(str, tokens[i]));
    }
    ppc_log("\n");
}

string_t ppc_read(const char* filename)
{
    FILE* file = fopen(filename, "rb");
    if (!file) {
        return string_empty();
    }
    
    fseek(file, 0, SEEK_END);
    string_t str = string_reserve(ftell(file) + 1);
    fseek(file, 0, SEEK_SET);
    fread(str.data, 1, str.capacity - 1, file);
    str.size = str.capacity - 1;
    str.data[str.size] = 0;
    fclose(file);
    return str;
}