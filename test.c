#include <zstdio.h>
#include <zstdlib.h>
#include <zstring.h>
#include <zlexer.h>
#include <zio.h>
#include <utopia/utopia.h>

/* LEXER */

#define TOK_NULL 0
#define TOK_ID 1
#define TOK_NUM 2
#define TOK_SYM 3
#define TOK_STR 4
#define TOK_NON 5
#define TOK_UNDEF 6

static int zlex_type(const char* str)
{
    if (!str || !*str) {
        return TOK_NULL;
    }

    if (!_isgraph(*str)) {
        return TOK_NON;
    }

    if (_isstr(*str)) {
        return TOK_STR;
    }
    
    if (_isdigit(*str) || (*str == '.' && _isdigit(str[1]))) {
        return TOK_NUM;
    }
    
    if (_isid(*str)) {
        return TOK_ID;
    }
    
    if (_ispunct(*str)) {
        return TOK_SYM;
    }
    
    return TOK_UNDEF;
}

static char* zlex_next(const char* str, unsigned int* typeptr)
{
    static char* (*zlex_funcs[])(const char*) = {
        &zcc_lexnull,
        &zcc_lexid,
        &zcc_lexnum,
        &zcc_lexop,
        &zcc_lexstr,
        &zcc_lexnone
    };
    
    *typeptr = zlex_type(str);
    return zlex_funcs[*typeptr](str);
}

/* TOKEN STRUCTURE */

struct token {
    const char* str;
    unsigned int len;
    unsigned int type;
};

static char* tokbuf(const struct token* token)
{
    static char buf[0xff];
    zmemcpy(buf, token->str, token->len);
    buf[token->len] = 0;
    return buf;
}

static struct token tokget(const char* start, const char* end, unsigned int type)
{
    struct token token;
    token.str = start;
    token.len = end - start;
    token.type = type;
    return token;
}

static struct token toknext(const char* str)
{
    unsigned int type;
    const char *tokend, *tokstart = str;
    struct token tok;
    tokend = zlex_next(tokstart, &type);
    while (type == TOK_NON) {
        tokstart = tokend;
        tokend = zlex_next(tokstart, &type);
    }
    tok = tokget(tokstart, tokend, type);
    return tok;
}

#define tokend(tok) ((char*)(size_t)tok->str + tok->len)

/* PARSER */

struct treenode* zparse_expression(const char* str, char** end);
struct treenode* zparse_expr(const char* str, char** end);

struct treenode* zparse_operand(const char* str, char** end)
{
    struct token tok = toknext(str);
    if (!str || !*str) {
        return NULL;
    }

    if (tok.type == TOK_NUM || tok.type == TOK_ID) {
        *end = tokend(&tok);
        return treenode_create(&tok, sizeof(struct token));
    }
    return NULL;
}

struct treenode* zparse_operator(const char* str, char** end)
{
    static const char* nonoperators = "]}),";

    int i;
    struct token tok = toknext(str);
    if (tok.type != TOK_SYM) {
        return NULL;
    }
    
    for (i = 0; nonoperators[i]; ++i) {
        if (tok.str[0] == nonoperators[i]) {
            return NULL; 
        }
    }

    *end = tokend(&tok);
    return treenode_create(&tok, sizeof(struct token));
}

struct treenode* zparse_char(const char* str, char** end, const char c)
{
    struct token tok = toknext(str);
    if (tok.str[0] == c) {
        *end = tokend(&tok);
        return treenode_create(&tok, sizeof(struct token));
    }
    return NULL;
}

struct treenode* zparse_symbol(const char* str, char** end, const char* symbol)
{
    struct token tok = toknext(str);
    if (!zmemcmp(str, symbol, zstrlen(symbol))) {
        *end = tokend(&tok);
        return treenode_create(&tok, sizeof(struct token));
    }
    return NULL;
}

struct treenode* zparse_term(const char* str, char** end)
{
    struct treenode *symbol = zparse_char(str, end, '(');
    if (symbol) {
        struct treenode* expr = zparse_expression(*end, end);
        treenode_free(symbol);
        if (!expr) {
            return NULL;
        }

        symbol = zparse_char(*end, end, ')');
        if (!symbol) {
            treenode_free(expr);
            return NULL;
        }

        treenode_free(symbol);
        return expr;
    }

    return zparse_operand(str, end);
}

struct treenode* zparse_expr(const char* str, char** end)
{
    struct treenode* operator, *term;
    if (!str || !*str) {
        return NULL;
    }

    term = zparse_term(str, end);
    if (!term) {
        return NULL;
    }

    operator = zparse_operator(*end, end);
    if (!operator) {
        return term;
    }
    treenode_push(operator, term);

    term = zparse_expr(*end, end);
    if (!term) {
        treenode_free(operator);
        *end = (char*)(size_t)str;
        return NULL;
    }

    treenode_push(operator, term);
    return operator;    
}

static int token_precedence(const char* str)
{
    if (!str) {
        return -1;
    }

    switch (*str) {
        case '(': return 0;
        case '!': return 3 + 7 * (str[1] == '=');
        case '*':
        case '/':
        case '%': return 5;
        case '+':
        case '-': return 3 + 3 * (str[1] != str[0]);
        case '<':
        case '>': return 9 - 2 * (str[1] == str[0]);
        case '=': return 10;
        case '^': return 12;
        case '&': return 11 + 3 * (str[1] == str[0]);
        case '|': return 13 + 2 * (str[1] == str[0]);
        default: return -1;
    }
}

static struct treenode* zparse_expression_precedence(const char* str, char** end, struct treenode* lhs, int min_precedence)
{
    struct treenode* rhs, *op;
    struct token lookahead;
    int next_precedence, precedence;

    lookahead = toknext(str);
    precedence = token_precedence(lookahead.str);

    while (precedence != -1 && precedence <= min_precedence) {
        op = zparse_operator(*end, end);
        if (!op) {
            break;
        }

        rhs = zparse_term(*end, end);
        if (!rhs) {
            treenode_push(op, lhs);
            lhs = op;
            break;
        }
        
        lookahead = toknext(*end);
        next_precedence = token_precedence(lookahead.str);

        while (next_precedence != -1 && next_precedence < precedence) {
            rhs = zparse_expression_precedence(*end, end, rhs, precedence - 1);
            lookahead = toknext(*end);
            next_precedence = token_precedence(lookahead.str);
        }
        
        treenode_push(op, lhs);
        treenode_push(op, rhs);
        lhs = op;

        precedence = token_precedence(lookahead.str);
    }

    return lhs;
}

struct treenode* zparse_expression(const char* str, char** end)
{
    struct treenode* lhs = zparse_term(str, end);
    return lhs ? zparse_expression_precedence(*end, end, lhs, 17) : NULL;
}

struct tree zparse(const char* str)
{
    char* end;
    struct tree tree;
    tree.bytes = sizeof(struct token);
    tree.root = zparse_expression(str, &end);
    return tree;
}

static void treenode_print(const struct treenode* node, const size_t lvl)
{
    size_t i;
    if (!node) {
        zprintf("Null Tree\n");
        return;
    }

    for (i = 0; i < lvl; ++i) {
        zprintf("  ");
    }

    zprintf("╚> %s\n", tokbuf(node->data));
    for (i = 0; node->children[i]; ++i) {
        treenode_print(node->children[i], lvl + 1);
    }
}

int main(int argc, char** argv)
{
    size_t len;
    char* data;
    struct tree abs;

    if (argc < 2) {
        zprintf("Missing input file.\n");
        return Z_EXIT_FAILURE;
    }

    data = zcc_fread(argv[1], &len);
    if (!data) {
        zprintf("Could not read file %s\n", argv[1]);
        return Z_EXIT_FAILURE;
    }

    abs = zparse(data);
    treenode_print(abs.root, 0);

    tree_free(&abs);
    zfree(data);
    return Z_EXIT_SUCCESS;
}
