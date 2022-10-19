#ifndef ZCC_LEXER_H
#define ZCC_LEXER_H

#include <zstddef.h>

/* flag masks */
#define ZTOK_KIND_MASK 0x100
#define ZTOK_SUBKIND_MASK 0x001
#define ZTOK_QUALIFIER_MASK 0x010

/* main 5 token types */
#define ZTOK_NULL 0x000
#define ZTOK_ID 0x100
#define ZTOK_NUM 0x200
#define ZTOK_SYM 0x300
#define ZTOK_STR 0x400
#define ZTOK_SRC 0x500

/* identifier subtypes */
#define ZTOK_ID_USER 0x100
#define ZTOK_ID_KEYWORD 0x101
#define ZTOK_ID_MACRO 0x102

/* number literal subtypes */
#define ZTOK_NUM_SIGNED 0x000
#define ZTOK_NUM_UNSIGNED 0x010
#define ZTOK_NUM_FLOAT 0x020

#define ZTOK_NUM_SIZE_8 0x001
#define ZTOK_NUM_SIZE_16 0x002
#define ZTOK_NUM_SIZE_32 0x004
#define ZTOK_NUM_SIZE_64 0x008

#define ZTOK_NUM_I8 (ZTOK_NUM | ZTOK_NUM_SIGNED | ZTOK_NUM_SIZE_8)
#define ZTOK_NUM_I16 (ZTOK_NUM | ZTOK_NUM_SIGNED | ZTOK_NUM_SIZE_16)
#define ZTOK_NUM_I32 (ZTOK_NUM | ZTOK_NUM_SIGNED | ZTOK_NUM_SIZE_32)
#define ZTOK_NUM_I64 (ZTOK_NUM | ZTOK_NUM_SIGNED | ZTOK_NUM_SIZE_64)
#define ZTOK_NUM_U8 (ZTOK_NUM | ZTOK_NUM_UNSIGNED | ZTOK_NUM_SIZE_8)
#define ZTOK_NUM_U16 (ZTOK_NUM | ZTOK_NUM_UNSIGNED | ZTOK_NUM_SIZE_16)
#define ZTOK_NUM_U32 (ZTOK_NUM | ZTOK_NUM_UNSIGNED | ZTOK_NUM_SIZE_32)
#define ZTOK_NUM_U64 (ZTOK_NUM | ZTOK_NUM_UNSIGNED | ZTOK_NUM_SIZE_64)
#define ZTOK_NUM_F32 (ZTOK_NUM | ZTOK_NUM_FLOAT | ZTOK_NUM_SIZE_32)
#define ZTOK_NUM_F64 (ZTOK_NUM | ZTOK_NUM_FLOAT | ZTOK_NUM_SIZE_64)

#define ZTOK_SYM_SEPARATOR 0x000
#define ZTOK_SYM_OP 0x001

#define ZTOK_STR_LITERAL 0x000
#define ZTOK_STR_CHAR 0x001

/* basic string boolean macros */
#define chrspace(c) (((c) == ' ') || ((c) == '\n') || ((c) == '\t') || ((c) == '\r'))
#define chrstr(c) (((c) == '\'') || ((c) == '"'))
#define chrparen(c) (((c) == '(') || ((c) == '[') || ((c) == '{')) 
#define chrbetween(c, a, b) (((c) >= (a)) && ((c) <= (b)))
#define chrdigit(c) chrbetween(c, '0', '9')
#define chralpha(c) (chrbetween(c, 'A', 'Z') || chrbetween(c, 'a', 'z') || ((c) == '_'))

typedef struct ztok_t {
    char* str;
    size_t len;
    int kind;
} ztok_t;

char* zcc_lex(const char* str, size_t* len, int* flag);
int zcc_lextype(const char* str);

char* zcc_lexspace(const char* str);
char* zcc_lexline(const char* str);
char* zcc_lexparen(const char* str);
char* zcc_lexstr(const char* str);
char* zcc_lexid(const char* str);
char* zcc_lexnum(const char* str);
char* zcc_lexop(const char* str);

ztok_t ztok_get(const char* str);
ztok_t ztok_next(ztok_t tok);

#endif