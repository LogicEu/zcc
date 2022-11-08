#include <zparser.h>
#include <zdbg.h>

/* constant reserved C keywords */

hash_t zcc_keywords_std(void)
{
    static const char* reserved[] = {
        "auto", "break", "case", "char", "const", "continue", "default", 
        "do", "double", "else", "enum", "extern", "float", "for", "goto",
        "if", "int", "long", "register", "return", "short", "signed",
        "sizeof", "static", "struct", "switch", "typedef", "union",
        "unsigned", "void", "volatile", "while"
    };

    size_t i;
    hash_t keywords = hash_create(sizeof(char*));
    for (i = 0; i < sizeof(reserved) / sizeof(reserved[0]); ++i) {
        hash_push(&keywords, reserved + i);
    }
    return keywords;
}

/* znode and abstract tree construction */

znode_t znode_create(ztok_t tok)
{
    znode_t node;
    node.token = tok;
    node.children = array_create(sizeof(znode_t));
    return node;
}

void znode_free(znode_t* node)
{
    size_t i;
    znode_t* child = node->children.data;
    const size_t count = node->children.size;
    for (i = 0; i < count; ++i) {
        znode_free(child++);
    }
    array_free(&node->children);
}

void znode_connect(znode_t* parent, const znode_t* child)
{
    array_push(&parent->children, child);
}

int zcc_parse(const char* str)
{
    ztok_t tok = ztok_get(str);
    while (!tok.str) {
        str = zcc_lexline(str);
        if (!*str) {
            break;
        }
        tok = ztok_get(++str);
    }
    
    const hash_t keywords = zcc_keywords_std();
    map_t identifiers = map_create(sizeof(char*), sizeof(znode_t));

    while (tok.str) {
        const char c = tok.str[tok.len];
        tok.str[tok.len] = 0;
        /*zcc_log("'%s' ", tok.str);*/
        tok.str[tok.len] = c;
        
        /* catch token as nodes ... */
        if (_isid(*tok.str)) {
            size_t find = zcc_hash_search(&keywords, tok);
            if (find--) {
                switch (find) {

                }
                goto zgototoknext;
            }

            find = zcc_map_search(&identifiers, tok);
            if (find--) {
                goto zgototoknext;
            }

            /* push to new var, func or label node to identifiers */
        }
        
zgototoknext:

        tok = ztok_nextl(tok);
        while (!tok.str) {
            str = zcc_lexline(str);
            if (!*str) {
                break;
            }
            tok = ztok_get(++str);
            /*zcc_log("\n%zu.- ", ++linecount);*/
        }
    }
    /*zcc_log("\n");*/
    return Z_EXIT_SUCCESS;
}