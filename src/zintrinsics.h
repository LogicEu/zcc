#ifndef ZCC_INTRINSICS_H
#define ZCC_INTRINSICS_H

#include <utopia/utopia.h>
#include <zlexer.h>

char* zstrbuf(const char* str, const size_t len);

array_t zcc_includes_std(void);
size_t zcc_map_search(const map_t* map, const ztok_t tok);
size_t zcc_hash_search(const hash_t* map, const ztok_t tok);

#endif