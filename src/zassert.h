#ifndef ZCC_ASSERT_H
#define ZCC_ASSERT_H

#include <zstd.h>
#include <zio.h>

static const char* zassertmsg = "zassert failed! (%s)\nFile '%s', Function '%s', Line '%zu'.\n";

#define zassert(expr)           \
do {                            \
    if (!(expr)) {              \
        zcc_log(                \
            zassertmsg,         \
            #expr,              \
            __FILE__,           \
            __FUNCTION__,       \
            __LINE__            \
        );                      \
        zexit(ZCC_EXIT_FAILURE);\
    }                           \
} while (0)

#endif