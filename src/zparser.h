#ifndef ZCC_PARSER_H
#define ZCC_PARSER_H

#include <zstddef.h>
#include <zlexer.h>
#include <utopia/utopia.h>

#define ZKEY_AUTO 0
#define ZKEY_BREAK 1
#define ZKEY_CASE 2
#define ZKEY_CHAR 3
#define ZKEY_CONST 4
#define ZKEY_CONTINUE 5
#define ZKEY_DEFAULT 6
#define ZKEY_DO 7
#define ZKEY_DOUBLE 8
#define ZKEY_ELSE 9
#define ZKEY_ENUM 10
#define ZKEY_EXTERN 11
#define ZKEY_FLOAT 12
#define ZKEY_FOR 13
#define ZKEY_GOTO 14
#define ZKEY_IF 15
#define ZKEY_INT 16
#define ZKEY_LONG 17
#define ZKEY_REGISTER 18
#define ZKEY_RETURN 19
#define ZKEY_SHORT 20
#define ZKEY_SIGNED 21
#define ZKEY_SIZEOF 22
#define ZKEY_STATIC 23
#define ZKEY_STRUCT 24
#define ZKEY_SWITCH 25
#define ZKEY_TYPEOF 26
#define ZKEY_UNION 27
#define ZKEY_UNSIGNED 28
#define ZKEY_VOID 29
#define ZKEY_VOLATILE 30
#define ZKEY_WHILE 31

#include <zintrinsics.h>

typedef struct znode_t {
    ztok_t token;
    vector_t children;
} znode_t;

int zcc_parse(const char* str);
znode_t znode_create(ztok_t tok);
void znode_connect(znode_t* parent, const znode_t* child);
void znode_free(znode_t* node);

#endif