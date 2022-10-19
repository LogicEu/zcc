#ifndef ZCC_PARSER_H
#define ZCC_PARSER_H

#include <zstddef.h>
#include <zlexer.h>
#include <utopia/utopia.h>

typedef struct znode_t {
    ztok_t token;
    array_t children;
} znode_t;

int zcc_parse(const char* str);
znode_t znode_create(ztok_t tok);
void znode_connect(znode_t* parent, const znode_t* child);
void znode_free(znode_t* node);

#endif