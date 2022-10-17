#include <zcompiler.h>
#include <utopia/utopia.h>

#define chrspace(c) (((c) == ' ') || ((c) == '\n') || ((c) == '\t') || ((c) == '\r'))
#define chrstr(c) (((c) == '\'') || ((c) == '"'))
#define chrparen(c) (((c) == '(') || ((c) == '[') || ((c) == '{')) 
#define chrbetween(c, a, b) (((c) >= (a)) && ((c) <= (b)))
#define chrdigit(c) chrbetween(c, '0', '9')
#define chralpha(c) (chrbetween(c, 'A', 'Z') || chrbetween(c, 'a', 'z') || ((c) == '_'))

typedef struct ztok_t {
    char* ptr;
    size_t size;
} ztoken_t;

typedef struct znode_t {
    array_t children;
    string_t token;
} znode_t;

znode_t znode_create(const char* tok)
{
    znode_t node;
    node.children = array_create(sizeof(znode_t));
    node.token = string_create(tok);
    return node;
}

void znode_free(znode_t* node)
{
    size_t i;
    const size_t childrencount = node->children.size;
    znode_t* n = node->children.data;
    for (i = 0; i < childrencount; ++i, ++n) {
        znode_free(n);
    }
    array_free(&node->children);
    string_free(&node->token);
}

#if 0
static void znode_abstree(const char* str)
{
    size_t len;
    const char* tok = zcc_lex_tokens(str, &len);
    while (tok) {
        zcc_log("'%s'\n", tok);
        switch (flag) {
            case CLASS_NULL: 
                break;
            case CLASS_ID:
                // if (!zstrcmp(tok, ))
                // if reserved_keyword
                // else if existing_variable or func
                // else new variable or func
                break;
            case CLASS_NUM:
                // check type and suffix
                break;
            case CLASS_STR:
                // char or string literal type
                break;
            case CLASS_OP:
                // check precedence
                // check assosiativity
                break;
        }
        tok = zcc_lex_tokens(NULL, &flag);
    }
}
#endif
void zcc_compile(const char* str)
{
    (void)str;
}