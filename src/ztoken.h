#ifndef ZCC_TOKEN_H
#define ZCC_TOKEN_H

#define ZTOK_NULL 0x00
#define ZTOK_ID 0x01
#define ZTOK_NUM 0x02
#define ZTOK_SYM 0x03
#define ZTOK_STR 0x04
#define ZTOK_NON 0x05
#define ZTOK_DEF 0x06

#define ZTOK_SYM_SEPARATOR 0x000
#define ZTOK_SYM_OP 0x010

#define ZTOK_SYM_LEFT 0x000
#define ZTOK_SYM_RIGHT 0x100
#define ZTOK_SYM_UNARY 0x200

#define ZTOK_SYM_OP_LEFT (ZTOK_SYM | ZTOK_SYM_OP | ZTOK_SYM_LEFT)
#define ZTOK_SYM_OP_RIGHT (ZTOK_SYM | ZTOK_SYM_OP | ZTOK_SYM_RIGHT)
#define ZTOK_SYM_OP_UNARY (ZTOK_SYM | ZTOK_SYM_OP | ZTOK_SYM_UNARY)

struct token {
    const char* str;
    unsigned int len;
    unsigned int type;
};

#define tokend(tok) ((char*)(size_t)tok.str + tok.len)

struct token ztokstr(const char* str);
struct token ztoknum(const long n);
struct token ztoknext(const char* str);
struct token ztokget(const char* start, const char* end, unsigned int type);
struct token ztokappend(const struct token* t1, const struct token* t2);
char* ztokbuf(const struct token* token);

#endif /* ZCC_TOKEN_H */
