#ifndef ZCC_INTRINSICS_H
#define ZCC_INTRINSICS_H

#include <utopia/utopia.h>
#include <ztoken.h>

char* zstrbuf(const char* str, const size_t len);

struct hash zcc_keywords_std(void);
struct vector zcc_includes_std(void);
struct map zcc_defines_std(void);
size_t zcc_map_search(const struct map* map, const struct token tok);
size_t zcc_hash_search(const struct hash* map, const struct token tok);

#endif /* ZCC_INTRINSICS_H */
