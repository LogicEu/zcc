#ifndef ZCC_LEXER_H
#define ZCC_LEXER_H

#include <utopia/utopia.h>
#include <zctype.h>

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
#define ZTOK_SPC 0x500
#define ZTOK_SRC 0x600

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

#define ZTOK_SYM_LEFT 0x000
#define ZTOK_SYM_RIGHT 0x010
#define ZTOK_SYM_UNARY 0x020

#define ZTOK_SYM_OP_LEFT (ZTOK_SYM | ZTOK_SYM_OP | ZTOK_SYM_LEFT)
#define ZTOK_SYM_OP_RIGHT (ZTOK_SYM | ZTOK_SYM_OP | ZTOK_SYM_RIGHT)
#define ZTOK_SYM_OP_UNARY (ZTOK_SYM | ZTOK_SYM_OP | ZTOK_SYM_UNARY)

#define ZTOK_STR_LITERAL 0x000
#define ZTOK_STR_CHAR 0x001

#define _isstr(c) (((c) == '\'') || ((c) == '"')) 
#define _isparen(c) (((c) == '(') || ((c) == '{') || ((c) == '[')) 
#define _isid(c) (_isalpha(c) || ((c) == '_'))

typedef struct ztok_t {
    char* str;
    size_t len;
    int kind;
} ztok_t;

char* zcc_lex(const char* str, size_t* len, int* flag);
int zcc_lextype(const char* str);

char* zcc_lexnull(const char* str);
char* zcc_lexnone(const char* str);
char* zcc_lexspace(const char* str);
char* zcc_lexline(const char* str);
char* zcc_lexparen(const char* str);
char* zcc_lexstr(const char* str);
char* zcc_lexid(const char* str);
char* zcc_lexnum(const char* str);
char* zcc_lexop(const char* str);

ztok_t ztok_get(const char* str);
ztok_t ztok_next(ztok_t tok);
ztok_t ztok_nextl(ztok_t tok);
ztok_t ztok_step(ztok_t tok, size_t steps);
ztok_t ztok_stepl(ztok_t tok, size_t steps);

struct vector zcc_tokenize(const char* str);
struct vector zcc_tokenize_line(const char* str);
struct vector zcc_tokenize_range(const char* start, const char* end);

#endif /* ZCC_LEXER_H */
