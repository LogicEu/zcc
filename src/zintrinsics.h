#ifndef ZCC_INTRINSICS_H
#define ZCC_INTRINSICS_H

#include <utopia/utopia.h>
#include <zlexer.h>

char* zstrbuf(const char* str, const size_t len);

struct vector zcc_includes_std(void);
struct map zcc_defines_std(void);
size_t zcc_map_search(const struct map* map, const ztok_t tok);
size_t zcc_hash_search(const struct hash* map, const ztok_t tok);

#endif /* ZCC_INTRINSICS_H */
