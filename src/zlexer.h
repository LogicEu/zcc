#ifndef ZCC_LEXER_H
#define ZCC_LEXER_H

#include <zctype.h>
#include <ztoken.h>
#include <utopia/vector.h>

#define _isstr(c) (((c) == '\'') || ((c) == '"')) 
#define _isparen(c) (((c) == '(') || ((c) == '{') || ((c) == '[')) 
#define _isid(c) (_isalpha(c) || ((c) == '_'))

char* zlex_next(const char* str, unsigned int* typeptr);
char* zcc_lex(const char* str, unsigned int* len, unsigned int* flag);

char* zcc_lexnull(const char* str);
char* zcc_lexnone(const char* str);
char* zcc_lexspace(const char* str);
char* zcc_lexline(const char* str);
char* zcc_lexparen(const char* str);
char* zcc_lexstr(const char* str);
char* zcc_lexid(const char* str);
char* zcc_lexnum(const char* str);
char* zcc_lexop(const char* str);

struct token ztok_get(const char* str);
struct token ztok_next(struct token tok);
struct token ztok_nextl(struct token tok);
struct token ztok_step(struct token tok, size_t steps);
struct token ztok_stepl(struct token tok, size_t steps);

struct vector zcc_tokenize(const char* str);
struct vector zcc_tokenize_line(const char* str);
struct vector zcc_tokenize_range(const char* start, const char* end);

#endif /* ZCC_LEXER_H */
