#ifndef ZCC_DEBUG_H
#define ZCC_DEBUG_H

#include <zassert.h>

#ifndef NDEBUG

#include <zintrinsics.h>
#include <zio.h>

#define ZBUG zcc_log("BUG(%s, %s, %ld)!\n", __FILE__, __FUNCTION__, __LINE__);

static void zcc_logtok(const char* fmt, const ztok_t tok)
{
    zcc_log(fmt, zstrbuf(tok.str, tok.len));
}

#else

#define ZBUG ((void)0)
#define zcc_log(fmt, ...) ((void)0)
#define zcc_logtok(fmt, tok) ((void)0)

#endif /* NDEBUG */
#endif /* ZCC_DEBUG_H */