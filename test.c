#include <zstdio.h>
#include <zstdlib.h>
#include <zstring.h>
#include <zassert.h>
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
#define TOK_DEF 7
#define TOK_DECL 8

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

#define tokend(tok) ((char*)(size_t)tok.str + tok.len)

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
    
    tokend = zlex_next(tokstart, &type);
    if (type == TOK_NON) {
        tokstart = tokend;
        tokend = zlex_next(tokstart, &type);
    }
    
    return tokget(tokstart, tokend, type);
}

static struct token toknum(const long n)
{
    struct token tok;
    char* str = zmalloc(0xf);
    tok.type = TOK_DEF;
    tok.len = zltoa(n, str, 10);
    tok.str = str;
    return tok;
}

static struct token tokstr(const char* str)
{
    char* buf;
    struct token tok;
    tok.type = TOK_DECL;
    tok.len = zstrlen(str);
    buf = zmalloc(tok.len + 1);
    zmemcpy(buf, str, tok.len);
    tok.str = buf;
    return tok;
}

/* SOLVERS AND HELPERS */

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

static int token_precedence(const char* str)
{
    if (!str) {
        return -1;
    }

    switch (*str) {
        case '!': return -1 + 11 * (str[1] == '=');
        case '*':
        case '/':
        case '%': return 5 + 12 * (str[1] == '=');
        case '+': return -1 + 7 * (str[1] != str[0]) + 11 * (str[1] == '=');
        case '-': return -1 + 7 * (str[1] != str[0]) + 11 * (str[1] == '=') - 7 * (str[1] == '>');
        case '<':
        case '>': return 9 - 2 * (str[1] == str[0]);
        case '=': return 17 - 7 * (str[1] == str[0]);
        case '&': return 11 + 6 * (str[1] == '=') + 3 * (str[1] == str[0]);
        case '^': return 12 + 5 * (str[1] == '=');
        case '|': return 13 + 4 * (str[1] == '=') + 2 * (str[1] == str[0]);
        default: return -1;
    }
}

static long zsolve_unary(const long l, const char* p)
{
    switch (*p) {
        case '!': return !l;
        case '~': return ~l;
        case '-': return -l;
        case '+': return +l;
    }
    zassert(0);
    return 0;
}

static long zsolve_binary(const long l, const long r, const char* p)
{
    switch (*p) {
        case '+': return l + r;
        case '-': return l - r;
        case '/': return l / r;
        case '*': return l * r;
        case '%': return l % r;
        case '^': return l ^ r;
        case '>': return p[1] == '>' ? l >> r : l > r;
        case '<': return p[1] == '<' ? l << r : l < r;
        case '&':
            if (p[1] == *p) {
                return l ? l && r : 0;
            }
            return l & r;
        case '|': 
            if (p[1] == *p) {
                return !l ? l || r : 1;
            }
            return l | r;
        case '=':
            if (p[1] == '=') {
                return l == r;
            }
            break;
        case '!':
            if (p[1] == '=') {
                return l != r;
            }
    }
    zassert(0);
    return 0;
}

static int zsolve_expr(const struct treenode* root, long* val)
{
    const struct token *op;
    if (!root) {
        return 0;
    }

    op = root->data;
    if (op->type == TOK_ID || op->type == TOK_STR) {
        return 0;
    }

    if (op->type == TOK_NUM) {
        *val = zatol(tokbuf(op));
        return 1;
    }

    if (op->type == TOK_SYM && *root->children) {
        long l, r;
        if (!zsolve_expr(root->children[0], &l)) {
            return 0;
        }

        if (root->children[1]) {
            if (!zsolve_expr(root->children[1], &r)) {
                return 0;
            }
            *val = zsolve_binary(l, r, op->str);
        } else {
            *val = zsolve_unary(l, op->str);
        }
        return 1;
    }

    return 0;
}

static void ast_free(struct treenode* root)
{
    int i;
    struct token* tok;
    if (!root) {
        return;
    }

    tok = root->data;
    if (tok->type == TOK_DEF || tok->type == TOK_DECL) {
        zfree((void*)(size_t)tok->str);
    }
    zfree(tok);

    for (i = 0; root->children[i]; ++i) {
        ast_free(root->children[i]);
    }

    zfree(root);
}

static void ast_reduce(struct treenode* root)
{
    long i;
    const struct token *op;
    if (!root || !root->children[0]) {
        return;
    }

    for (i = 0; root->children[i]; ++i) {
        ast_reduce(root->children[i]);
    }

    op = root->data;
    if (op->type == TOK_SYM) {
        const struct token* lhs, *rhs;
        lhs = root->children[0]->data;
        if (lhs->type != TOK_NUM && lhs->type != TOK_DEF) {
            return;
        }

        if (!root->children[1]) {
            struct token tok = toknum(zsolve_unary(zatol(tokbuf(lhs)), op->str));
            ast_free(root->children[0]);
            root->children[0] = NULL;
            zmemcpy(root->data, &tok, sizeof(struct token));
            return;
        } 
        
        rhs = root->children[1]->data;
        if (rhs->type == TOK_NUM || rhs->type == TOK_DEF) {
            struct token tok = toknum(zsolve_binary(zatol(tokbuf(lhs)), zatol(tokbuf(rhs)), op->str));
            ast_free(root->children[0]);
            ast_free(root->children[1]);
            zmemset(root->children, 0, sizeof(struct token) * 2);
            zmemcpy(root->data, &tok, sizeof(struct token));
        }
    }
}

/* PARSER */

typedef struct treenode* (*parser_f)(const char*, char**);

struct treenode* zparse_object(const char* str, char** end);
struct treenode* zparse_operator_postfix(const char* str, char** end);
struct treenode* zparse_operator_access(const char* str, char** end);
struct treenode* zparse_term(const char* str, char** end);
struct treenode* zparse_paren(const char* str, char** end, parser_f parser);
struct treenode* zparse_any(const char* str, char** end, parser_f parser, struct treenode* node);
struct treenode* zparse_enum(const char* str, char** end, parser_f parser, const char c, struct treenode* node);

int zparse_check(const char* str, char** end, const char c)
{
    struct token tok = toknext(str);
    if (tok.type != TOK_NULL && tok.len == 1 && tok.str[0] == c) {
        *end = tokend(tok);
        return 1;
    }
    return 0;
}

struct treenode* zparse_char(const char* str, char** end, const char c)
{
    struct token tok = toknext(str);
    if (tok.type != TOK_NULL && tok.len == 1 && tok.str[0] == c) {
        *end = tokend(tok);
        return treenode_create(&tok, sizeof(struct token));
    }
    return NULL;
}

struct treenode* zparse_token(const char* str, char** end, const char* symbol)
{
    size_t len;
    struct token tok = toknext(str);
    len = zstrlen(symbol);
    if (tok.type != TOK_NULL && len == tok.len && !zmemcmp(tok.str, symbol, len)) {
        *end = tokend(tok);
        return treenode_create(&tok, sizeof(struct token));
    }
    return NULL;
}

struct treenode* zparse_identifier(const char* str, char** end)
{
    struct token tok = toknext(str);
    if (tok.type == TOK_ID) {
        *end = tokend(tok);
        return treenode_create(&tok, sizeof(struct token));
    }
    return NULL;
}

struct treenode* zparse_number(const char* str, char** end)
{
    struct token tok = toknext(str);
    if (tok.type == TOK_NUM) {
        *end = tokend(tok);
        return treenode_create(&tok, sizeof(struct token));
    }
    return NULL;
}

struct treenode* zparse_operand(const char* str, char** end)
{
    struct token tok = toknext(str);
    if (tok.type == TOK_NUM || tok.type == TOK_STR) {
        *end = tokend(tok);
        return treenode_create(&tok, sizeof(struct token));
    } else if (tok.type == TOK_ID) {
        struct treenode* identifier, *postfix;
        *end = tokend(tok);
        identifier = treenode_create(&tok, sizeof(struct token));
        zparse_any(*end, end, &zparse_operator_access, identifier);
        postfix = zparse_operator_postfix(*end, end);
        if (postfix) {
            treenode_push(identifier, postfix);
        }
        return identifier;
    }
    return NULL;
}

struct treenode* zparse_sizeof(const char* str, char** end)
{
    struct treenode* sizeofnode = zparse_token(str, end, "sizeof");
    if (sizeofnode) {
        struct treenode* expr = zparse_paren(*end, end, &zparse_object);
        if (!expr) {
            expr = zparse_term(*end, end);
            if (!expr) {
                zprintf("Illegal sizeof operand.\n");
                treenode_free(sizeofnode);
                return NULL;
            }
        }
        treenode_push(sizeofnode, expr);
    }
    return sizeofnode;
}

struct treenode* zparse_operator_unary(const char* str, char** end)
{
    char c;
    struct token tok = toknext(str);

    c = tok.str[0];
    if (c != '+' && c != '-' && c != '~' && c != '!' && c != '&' && c != '*') {
        return NULL;
    }

    *end = tokend(tok);
    return treenode_create(&tok, sizeof(struct token));
}

struct treenode* zparse_operator_binary(const char* str, char** end)
{
    static const char* nonbinary = "]}),;:?~\\@#$`'\"";

    int i;
    char c;
    struct token tok = toknext(str);
    
    if (tok.type != TOK_SYM) {
        return NULL;
    }

    c = *tok.str;
    for (i = 0; nonbinary[i]; ++i) {
        if (c == nonbinary[i]) {
            return NULL; 
        }
    }
    
    *end = tokend(tok);
    return treenode_create(&tok, sizeof(struct token));
}

struct treenode* zparse_expr(const char* str, char** end);

struct treenode* zparse_operator_ternary(const char* str, char** end)
{
    struct treenode* operator = zparse_char(str, end, '?');
    if (operator) {
        struct treenode* expr = zparse_expr(*end, end);
        if (!expr) {
            zprintf("Expected expression after '?' ternary operator.\n");
            treenode_free(operator);
            return NULL;
        }
        treenode_push(operator, expr);

        if (!zparse_check(*end, end, ':')) {
            zprintf("Expected ':' after first ternary expression.\n");
            treenode_free(operator);
            return NULL;
        }

        expr = zparse_expr(*end, end);
        if (!expr) {
            zprintf("Expected expression after ':' ternary operator.\n");
            treenode_free(operator);
            return NULL;
        }
        treenode_push(operator, expr);
    }
    return operator;
}

struct treenode* zparse_funcall(const char* str, char** end)
{
    struct treenode* args, *check;
    struct token tok = tokstr("()");
    args = treenode_create(&tok, sizeof(struct token));
    check = zparse_enum(str, end, &zparse_expr, ',', args);
    if (!zparse_check(check ? *end : str, end, ')')) {
        zprintf("Expected closing parenthesis after function call.\n");
        treenode_free(args);
        args = NULL;
    }
    return args;
}

struct treenode* zparse_operator_access(const char* str, char** end)
{
    char c;
    struct treenode* node;
    struct token tok = toknext(str);

    c = tok.str[0];
    if (c == '(') {
        *end = tokend(tok);
        node = zparse_funcall(*end, end);
        if (!node) {
            zprintf("Expected function call.\n");
        }
        return node;
    } else if (c == '[') {
        *end = tokend(tok);
        node = zparse_expr(*end, end);
        if (!node) {
            zprintf("Expected expression inside [] array accessor.\n");
            treenode_free(node);
            return NULL;
        }
        if (!zparse_check(*end, end, ']')) {
            zprintf("Expected closing ] parenthesis in array accessor.\n");
            treenode_free(node);
            return NULL;
        }
        return node;
    } else if (c == '.' || (c == '-' && tok.str[1] == '>')) {
        struct treenode* identifier;
        *end = tokend(tok);
        node = treenode_create(&tok, sizeof(struct token));
        identifier = zparse_identifier(*end, end);
        if (!identifier) {
            zprintf("Expected identifier after %s operator.\n", tokbuf(&tok));
            treenode_free(node);
            return NULL;
        }
        treenode_push(node, identifier);
        return node;
    } else {
        return NULL;
    }

    return treenode_create(&tok, sizeof(struct token));
}

struct treenode* zparse_operator_postfix(const char* str, char** end)
{
    char c;
    struct token tok = toknext(str);

    c = tok.str[0];
    if ((c != '-' || tok.str[1] != '-') || (c != '+' || tok.str[1] != '+')) {
        return NULL;
    }

    *end = tokend(tok);
    return treenode_create(&tok, sizeof(struct token));
}

struct treenode* zparse_term(const char* str, char** end)
{
    struct treenode *term = zparse_operator_unary(str, end);
    if (term) {
        struct treenode* realterm = zparse_term(*end, end);
        if (!realterm) {
            zprintf("Expected term after unary operator %s in expression.\n", tokbuf(term->data));
            treenode_free(term);
            return NULL;
        }
        treenode_push(term, realterm);
        return term;
    }

    if (zparse_check(str, end, '(')) {
        term = zparse_paren(str, end, &zparse_object);
        if (term) {
            struct treenode* realterm = zparse_term(*end, end);
            if (!realterm) {
                zprintf("Expected term after type cast operator %s in expression.\n", tokbuf(term->data));
                treenode_free(term);
                return NULL;
            }
            treenode_push(term, realterm);
            return term;
        }

        term = zparse_paren(str, end, &zparse_expr);
        return term;
    }

    term = zparse_sizeof(str, end);
    if (term) {
        return term;
    }

    return zparse_operand(str, end);
}

static struct treenode* zparse_expr_rhs(const char* str, char** end, struct treenode* lhs, int min_precedence)
{
    struct treenode* rhs, *op;
    struct token lookahead;
    int next_precedence, precedence;

    lookahead = toknext(str);
    if (lookahead.str[0] == '?') {
        op = zparse_operator_ternary(*end, end);
        if (!op) {
            return NULL;
        }
        treenode_push(op, lhs);
        return op;
    }

    precedence = token_precedence(lookahead.str);

    while (precedence != -1 && precedence <= min_precedence) {
        op = zparse_operator_binary(*end, end);
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

        if (lookahead.str[0] == '?') {
            op = zparse_operator_ternary(lookahead.str, end);
            if (!op) {
                break;
            }
            treenode_push(op, lhs);
            lhs = op;
            continue;
        }

        while (next_precedence != -1 && next_precedence < precedence) {
            rhs = zparse_expr_rhs(*end, end, rhs, precedence - 1);
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

struct treenode* zparse_expr(const char* str, char** end)
{
    struct treenode* lhs = zparse_term(str, end);
    return lhs ? zparse_expr_rhs(*end, end, lhs, 20) : NULL;
}

struct treenode* zparse_constexpr(const char* str, char** end)
{
    struct treenode* expr = zparse_expr(str, end);
    if (expr) {
        struct token* tok;
        ast_reduce(expr);
        tok = expr->data;
        if (tok->type != TOK_DEF && tok->type != TOK_NUM && tok->str[0] != '\'') {
            static const char sizeofstr[] = "sizeof";
            if ((tok->len == sizeof(sizeofstr) - 1 && !zmemcmp(tok->str, sizeofstr, sizeof(sizeofstr) - 1)) || 
                (tok->str[0] == '&' && tok->len == 1)) {
                return expr;
            }
            treenode_free(expr);
            expr = NULL;
        }
    }
    return expr;
}

/* COMPOSERS */

struct treenode* zparse_keywords(const char* str, char** end, const char** keywords)
{
    int i;
    for (i = 0; keywords[i]; ++i) {
        struct treenode* node = zparse_token(str, end, keywords[i]);
        if (node) {
            return node;
        }
    }
    return NULL;
}

struct treenode* zparse_or(const char* str, char** end, const parser_f* parsers)
{
    int i;
    for (i = 0; parsers[i]; ++i) {
        struct treenode* node = parsers[i](str, end);
        if (node) {
            return node;
        }
    }
    return NULL;
}

struct treenode* zparse_any(const char* str, char** end, parser_f parser, struct treenode* root)
{
    struct treenode* node = parser(str, end);
    while (node) {
        treenode_push(root, node);
        node = parser(*end, end);
    }
    return root;
}

struct treenode** zparse_chain(const char* str, char** end, const parser_f* parsers)
{
    int i;
    struct treenode* node;
    struct vector chain = vector_create(sizeof(struct treenode*));
    
    *end = (char*)(size_t)str;
    for (i = 0; parsers[i]; ++i) {
        node = parsers[i](*end, end);
        if (node) {
            vector_push(&chain, &node);
        }
    }
    
    if (chain.data) {
        node = NULL;
        vector_push(&chain, &node);
    }
    
    return chain.data;
}

struct treenode* zparse_enum(const char* str, char** end, parser_f parser, const char c, struct treenode* node)
{
    struct treenode* child = parser(str, end);
    if (child) {
        treenode_push(node, child);
        if (!zparse_check(*end, end, c)) {
            return node;
        }
        return zparse_enum(*end, end, parser, c, node);
    }
    return NULL;  
}

struct treenode* zparse_count(const char* str, char** end, parser_f parser, const int count, struct treenode* node)
{
    int childcount = 0;
    struct treenode* child = parser(str, end);
    while (child && childcount < count) {
        treenode_push(node, child);
        ++childcount;
        child = parser(*end, end);
    }

    if (childcount != count) {
        treenode_children_free(node);
        return NULL;
    }
    
    return node;
}

struct treenode* zparse_paren(const char* str, char** end, parser_f parser)
{
    char* mark = *end;
    if (zparse_check(str, end, '(')) {
        struct treenode* node = parser(*end, end);
        if (!node) {
            *end = mark;
            return NULL;
        }
        
        if (!zparse_check(*end, end, ')')) {
            zprintf("Expected closing parenthesis.\n");
            treenode_free(node);
            *end = mark;
            return NULL;
        }
        
        return node;
    }
    return NULL;
}

/* C-Parser */

struct treenode* zparse_storage(const char* str, char** end)
{
    static const char* keywords[] = {"static", "extern", "register", "typedef", NULL};
    return zparse_keywords(str, end, keywords);
}

struct treenode* zparse_qualifier(const char* str, char** end)
{
    static const char* keywords[] = {"const", "volatile", NULL};
    return zparse_keywords(str, end, keywords);
}

struct treenode* zparse_signedness(const char* str, char** end)
{
    static const char* keywords[] = {"signed", "unsigned", NULL};
    return zparse_keywords(str, end, keywords);
}

struct treenode* zparse_sizeness(const char* str, char** end)
{
    static const char* keywords[] = {"short", "long", NULL};
    return zparse_keywords(str, end, keywords);
}

struct treenode* zparse_type(const char* str, char** end)
{
    static const char* keywords[] = {"char", "int", "float", "double", "void", "size_t", "parser_f", NULL};
    return zparse_keywords(str, end, keywords);
}

struct treenode* zparse_array(const char* str, char** end);
struct treenode* zparse_declstat(const char* str, char** end);

struct treenode* zparse_objstruct(const char* str, char** end)
{
    struct treenode* structnode = zparse_token(str, end, "struct");
    if (structnode) {
        struct treenode* identifier;
        identifier = zparse_identifier(*end, end);
        if (!identifier) {
            zprintf("Expected identifier after struct keyword.\n");
            treenode_free(structnode);
            return NULL;
        }

        if (zparse_check(*end, end, '{')) {
            zparse_any(*end, end, &zparse_declstat, structnode);
            if (!zparse_check(*end, end, '}')) {
                zprintf("Expected } after struct definition.\n");
                treenode_free(identifier);
                treenode_free(structnode);
                return NULL;
            }
        }
        treenode_push(identifier, structnode);
        structnode = identifier;
    }
    return structnode;
}

struct treenode* zparse_list(const char* str, char** end)
{
    if (zparse_check(str, end, '{')) {
        struct token tok;
        struct treenode* list;
        tok = tokstr("{}");
        list = treenode_create(&tok, sizeof(struct token));
        zparse_enum(*end, end, &zparse_expr, ',', list);
        if (!zparse_check(*end, end, '}')) {
            zprintf("Expected } after initializer list.\n");
            treenode_free(list);
            return NULL;
        }
        return list;
    }
    return NULL;
}

struct treenode* zparse_assignment(const char* str, char** end, parser_f parser)
{
    struct treenode* assignment = zparse_char(str, end, '=');
    if (assignment) {
        struct treenode* expr = parser(*end, end);
        if (!expr) {
            zprintf("Assignment not followed by expression when declaring variable.\n");
            treenode_free(assignment);
            return NULL;
        }

        treenode_push(assignment, expr);
    }
    return assignment;
}

struct treenode* zparse_enumstat(const char* str, char** end)
{
    struct treenode* identifier = zparse_identifier(str, end);
    if (identifier) {
        struct treenode* assignment = zparse_assignment(*end, end, &zparse_constexpr);
        if (assignment) {
            treenode_push(identifier, assignment);
        }
    }
    return identifier;
}

struct treenode* zparse_objenum(const char* str, char** end)
{
    struct treenode* enumnode = zparse_token(str, end, "enum");
    if (enumnode) {
        struct treenode* identifier = zparse_identifier(*end, end);
        if (!identifier) {
            zprintf("Expected identifier after enum keyword.\n");
            treenode_free(enumnode);
            return NULL;
        }

        if (zparse_check(*end, end, '{')) {
            zparse_enum(*end, end, &zparse_enumstat, ',', enumnode);
            if (!zparse_check(*end, end, '}')) {
                zprintf("Expected } after enum definition.\n");
                treenode_free(identifier);
                treenode_free(enumnode);
                return NULL;
            }
        }
        treenode_push(identifier, enumnode);
        enumnode = identifier;
    }
    return enumnode;
}

struct treenode* zparse_varid(const char* str, char** end);
struct treenode* zparse_declargs(const char* str, char** end);

struct treenode* zparse_funcptr(const char* str, char** end)
{
    struct treenode* varid = zparse_paren(str, end, &zparse_varid);
    if (varid) {
        if (!zparse_check(*end, end, '(')) {
            zprintf("Invalid syntax for function pointers.\n");
            treenode_free(varid);
            return NULL;
        }

        zparse_enum(*end, end, &zparse_declargs, ',', varid);
        if (!zparse_check(*end, end, ')')) {
            zprintf("Expected ) after function pointer declaration.\n");
            treenode_free(varid);
            varid = NULL;
        }
    }
    return varid;
}

struct treenode* zparse_exprdecl(const char* str, char** end)
{
    static const parser_f parsers[] = {
        &zparse_expr,
        &zparse_list,
        NULL
    };

    return zparse_or(str, end, parsers);
}

struct treenode* zparse_indirection(const char* str, char** end)
{
    struct treenode* indirection, *qualifier, *rec;
    indirection = zparse_char(str, end, '*');
    if (!indirection) {
        return NULL;
    }
    
    qualifier = zparse_qualifier(*end, end);
    if (qualifier) {
        treenode_push(indirection, qualifier);
    }

    rec = zparse_indirection(*end, end);
    if (rec) {
        treenode_push(indirection, rec);
    }

    return indirection;
}

struct treenode* zparse_array(const char* str, char** end)
{
    if (zparse_check(str, end, '[')) {
        struct token tok;
        struct treenode* expr, *node;
        expr = zparse_constexpr(*end, end);
        if (!zparse_check(*end, end, ']')) {
            zprintf("Expected ] at array declaration.\n");
            if (expr) {
                treenode_free(expr);
            }
        }

        tok = tokstr("[]");
        node = treenode_create(&tok, sizeof(struct token));
        if (expr) {
            treenode_push(node, expr);
        }
        return node;
    }
    return NULL;
}

struct treenode* zparse_object(const char* str, char** end)
{
    static const parser_f parsers[] = {
        &zparse_type,
        &zparse_objstruct,
        &zparse_objenum,
        NULL
    };

    struct treenode* object = zparse_or(str, end, parsers);
    if (object) {
        struct treenode* indirection = zparse_indirection(*end, end);
        if (indirection) {
            treenode_push(object, indirection);
        }
    }
    return object;
}

struct treenode* zparse_varid(const char* str, char** end)
{
    struct treenode* identifier, *indirection, *array;
    indirection = zparse_indirection(str, end);

    identifier = zparse_identifier(indirection ? *end : str, end);
    if (!identifier) {
        if (indirection) {
            treenode_free(indirection);
        }
        return NULL;
    }

    if (indirection) {
        treenode_push(identifier, indirection);
    }

    array = zparse_array(*end, end);
    if (array) {
        treenode_push(identifier, array);
    }

    return identifier;
}

struct treenode* zparse_funcid(const char* str, char** end)
{
    struct treenode* indirection = zparse_char(str, end, '*');
    if (indirection) {
        struct treenode* varid = zparse_varid(*end, end);
        if (!varid) {
            treenode_free(indirection);
            return NULL;
        }
        treenode_push(varid, indirection);
        indirection = varid;
    }
    return indirection;
}

struct treenode* zparse_variable(const char* str, char** end)
{
    struct treenode* var, *assignment;
    var = zparse_varid(str, end);
    if (!var) {
        var = zparse_funcptr(str, end);
        if (!var) {
            return NULL;
        }
    }

    assignment = zparse_assignment(*end, end, &zparse_exprdecl);
    if (assignment) {
        treenode_push(var, assignment);
    }

    return var;
}

struct treenode* zparse_decl(const char* str, char** end)
{
    static const parser_f parsers[] = {
        &zparse_storage,
        &zparse_qualifier,
        &zparse_signedness,
        &zparse_sizeness,
        NULL
    };

    struct treenode** chain, *type, *indirection = NULL;
    chain = zparse_chain(str, end, parsers);
    type = zparse_object(chain ? *end : str, end);

    if (!type) {
        if (!chain) {
            return NULL;
        } else {
            struct token tok = tokstr("int");
            type = treenode_create(&tok, sizeof(struct token));
            indirection = zparse_indirection(*end, end);
        }
    }

    if (chain) {
        indirection = indirection ? indirection : type->children[0];
        zfree(type->children);
        type->children = chain;
    }

    if (indirection) {
        treenode_push(type, indirection);
    }

    return type;
}

struct treenode* zparse_declstat(const char* str, char** end)
{
    struct treenode* decl = zparse_decl(str, end);
    if (decl) {
        struct token tok;
        struct treenode *parent;
        
        tok = tokstr(":=");
        parent = treenode_create(&tok, sizeof(struct token));
        treenode_push(parent, decl);
        decl = parent;
        
        zparse_enum(*end, end, &zparse_variable, ',', decl);
        if (!zparse_check(*end, end, ';')) {
            zprintf("Expected ';' at the end of declaration.\n");
            zprintf("%s\n", *end);
            treenode_free(decl);
            return NULL;
        }
    }
    return decl;
}

struct treenode* zparse_nullstat(const char* str, char** end)
{
    if (zparse_check(str, end, ';')) {
        struct token tok = tokstr("null");
        return treenode_create(&tok, sizeof(struct token));
    }
    return NULL;
}

struct treenode* zparse_exprdef(const char* str, char** end)
{
    struct treenode* expr = zparse_expr(str, end);
    if (expr) {
        struct token tok;
        struct treenode* parent;
        
        tok = tokstr("(!)");
        parent = treenode_create(&tok, sizeof(struct token));
        treenode_push(parent, expr);
        expr = parent;

        if (zparse_check(*end, end, ',')) {
            zparse_enum(*end, end, &zparse_expr, ',', expr);
        }
    }

    return expr ? expr : zparse_nullstat(str, end);
}

struct treenode* zparse_exprstat(const char* str, char** end)
{
    struct treenode* expr = zparse_expr(str, end);
    if (expr) {
        struct token tok;
        struct treenode* parent;
        
        tok = tokstr("(!)");
        parent = treenode_create(&tok, sizeof(struct token));
        treenode_push(parent, expr);
        expr = parent;

        if (zparse_check(*end, end, ',')) {
            zparse_enum(*end, end, &zparse_expr, ',', expr);
        }

        if (!zparse_check(*end, end, ';')) {
            zprintf("Expected ';' after expression inside scope.\n");
            treenode_free(expr);
            return NULL;
        }
    }

    return expr ? expr : zparse_nullstat(str, end);
}

struct treenode* zparse_return(const char* str, char** end)
{
    struct treenode* retnode = zparse_token(str, end, "return");
    if (retnode) {
        struct treenode* expr;
        expr = zparse_expr(*end, end);
        if (!expr) {
            return retnode;
        }

        if (!zparse_check(*end, end, ';')) {
            zprintf("Expected ';' at the end of return expression.\n");
            treenode_free(retnode);
            treenode_free(expr);
            return NULL;
        }

        treenode_push(retnode, expr);
    }
    return retnode;
}

struct treenode* zparse_while_expr(const char* str, char** end)
{
    struct treenode* whilenode = zparse_token(str, end, "while");
    if (whilenode) {
        struct treenode* expr = zparse_paren(*end, end, &zparse_expr);
        if (!expr) {
            zprintf("Expected parenthesised expression after while kwyword.\n");
            treenode_free(whilenode);
            return NULL;
        }
        treenode_push(whilenode, expr);
    }
    return whilenode;
}

struct treenode* zparse_statement(const char* str, char** end);
struct treenode* zparse_loopstat(const char* str, char** end);
struct treenode* zparse_switchstat(const char* str, char** end);

struct treenode* zparse_while(const char* str, char** end)
{
    struct treenode* whilenode = zparse_while_expr(str, end);
    if (whilenode) {
        struct treenode* scope = zparse_loopstat(*end, end);
        if (scope) {
            treenode_push(whilenode, scope);
        }
    }
    return whilenode;
}

struct treenode* zparse_do(const char* str, char** end)
{
    struct treenode* donode = zparse_token(str, end, "do");
    if (donode) {
        struct treenode* scope, *whilenode;
        scope = zparse_loopstat(*end, end);
        if (!scope) {
            zprintf("Expected scope to match do statement.\n");
            treenode_free(donode);
            return NULL;
        }

        treenode_push(donode, scope);
        whilenode = zparse_while_expr(*end, end);
        if (!whilenode) {
            zprintf("Expected while to match do statement.\n");
            treenode_free(donode);
            return NULL;
        }

        treenode_push(donode, whilenode);
    }
    return donode;
}

struct treenode* zparse_else(const char* str, char** end);

struct treenode* zparse_if(const char* str, char** end)
{
    struct treenode* ifnode = zparse_token(str, end, "if");
    if (ifnode) {
        struct treenode* expr, *scope, *elsenode;
        
        expr = zparse_paren(*end, end, &zparse_expr);
        if (!expr) {
            zprintf("Expected parenthesised expression after if keyword.\n");
            treenode_free(ifnode);
            return NULL;
        }
        treenode_push(ifnode, expr);

        scope = zparse_statement(*end, end);
        if (!scope) {
            zprintf("Expected scope to match if statement.\n");
            treenode_free(ifnode);
            return NULL;
        }
        treenode_push(ifnode, scope);

        elsenode = zparse_else(*end, end);
        if (elsenode) {
            treenode_push(ifnode, elsenode);
        }
    }
    return ifnode;
}

struct treenode* zparse_else(const char* str, char** end)
{
    struct treenode* elsenode = zparse_token(str, end, "else");
    if (elsenode) {
        struct treenode* node;
        node = zparse_if(*end, end);
        if (node) {
            treenode_push(elsenode, node);
            return elsenode;
        }

        node = zparse_statement(*end, end);
        if (node) {
            treenode_push(elsenode, node);
        }
    }
    return elsenode;
}

struct treenode* zparse_for(const char* str, char** end)
{
    struct treenode* fornode = zparse_token(str, end, "for");
    if (fornode) {
        int i;
        struct treenode* scope, *expr;

        if (!zparse_check(*end, end, '(')) {
            zprintf("Expected parenthesis after for keyword.\n");
            treenode_free(fornode);
            return NULL;
        }

        for (i = 0; i < 2; ++i) {
            expr = zparse_exprstat(*end, end);
            if (!expr) {
                zprintf("For loop must contain three expressions within declaration.\n");
                treenode_free(fornode);
                return NULL;
            }
            treenode_push(fornode, expr);
        }

        expr = zparse_exprdef(*end, end);
        if (!expr) {
            zprintf("Invalid for loop syntax declaration.\n");
            treenode_free(fornode);
            return NULL;
        }
        treenode_push(fornode, expr);

        if (!zparse_check(*end, end, ')')) {
            zprintf("Expected closing parenthesis after for loop declaration.\n");
            treenode_free(fornode);
            return NULL;
        }

        scope = zparse_loopstat(*end, end);
        if (!scope) {
            zprintf("Expected body after for loop declaration.\n");
            treenode_free(fornode);
            return NULL;
        }

        treenode_push(fornode, scope);
    }
    return fornode;
}

struct treenode* zparse_goto(const char* str, char** end)
{
    struct treenode* gotonode = zparse_token(str, end, "goto");
    if (gotonode) {
        struct treenode* identifier = zparse_identifier(*end, end);
        if (!identifier) {
            zprintf("Expected label identifier after goto keyword.\n");
            treenode_free(gotonode);
            return NULL;
        }
        
        treenode_push(gotonode, identifier);
        if (!zparse_check(*end, end, ';')) {
            zprintf("Expected semicolon after goto statement.\n");
            treenode_free(gotonode);
            return NULL;
        }
    }
    return gotonode;
}

struct treenode* zparse_label(const char* str, char** end)
{
    struct treenode* label = zparse_identifier(str, end);
    if (label && !zparse_check(*end, end, ':')) {
        treenode_free(label);
        return NULL;
    }
    return label;
}

struct treenode* zparse_continue(const char* str, char** end)
{
    struct treenode* contnode = zparse_token(str, end, "continue");
    if (contnode && !zparse_check(*end, end, ';')) {
        zprintf("Expected semicolon after continue keyword.\n");
        treenode_free(contnode);
        return NULL;
    }
    return contnode;
}

struct treenode* zparse_break(const char* str, char** end)
{
    struct treenode* breaknode = zparse_token(str, end, "break");
    if (breaknode && !zparse_check(*end, end, ';')) {
        zprintf("Expected semicolon after break keyword.\n");
        treenode_free(breaknode);
        return NULL;
    }
    return breaknode;
}

struct treenode* zparse_case(const char* str, char** end)
{
    struct treenode* casenode = zparse_token(str, end, "case");
    if (casenode) {
        struct treenode* constexpr = zparse_constexpr(*end, end);
        if (!constexpr) {
            zprintf("Expected constant expression after case keyword.\n");
            zprintf("%s\n", *end);
            treenode_free(casenode);
            return NULL;
        }

        treenode_push(casenode, constexpr);
        if (!zparse_check(*end, end, ':')) {
            zprintf("Expected semicolon after case statement.\n");
            treenode_free(casenode);
            return NULL;
        }
    }
    return casenode;
}

struct treenode* zparse_default(const char* str, char** end)
{
    struct treenode* defaultnode = zparse_token(str, end, "default");
    if (defaultnode) {
        if (!zparse_check(*end, end, ':')) {
            zprintf("Expected semicolon after default statement.\n");
            treenode_free(defaultnode);
            return NULL;
        }
    }
    return defaultnode;
}

struct treenode* zparse_switchscope(const char* str, char** end);

struct treenode* zparse_switch(const char* str, char** end)
{
    struct treenode* switchnode = zparse_token(str, end, "switch");
    if (switchnode) {
        struct treenode* expr;
        expr = zparse_paren(*end, end, &zparse_expr);
        if (!expr) {
            zprintf("Expected parenthesised expression after switch keyword.\n");
            treenode_free(expr);
            return NULL;
        }
        treenode_push(switchnode, expr);

        expr = zparse_switchscope(*end, end);
        if (!expr) {
            zprintf("Expected body after switch declaration.\n");
            treenode_free(switchnode);
            return NULL;
        }

        treenode_push(switchnode, expr);
    }
    return switchnode;
}

struct treenode* zparse_scope(const char* str, char** end);
struct treenode* zparse_loopscope(const char* str, char** end);

struct treenode* zparse_stat(const char* str, char** end)
{
    static const parser_f parsers[] = {
        &zparse_if,
        &zparse_while,
        &zparse_do,
        &zparse_for,
        &zparse_switch,
        &zparse_return,
        &zparse_goto,
        &zparse_label,
        &zparse_exprstat,
        NULL
    };

    return zparse_or(str, end, parsers);
}

struct treenode* zparse_statement(const char* str, char** end)
{
    static const parser_f parsers[] = {
        &zparse_stat,
        &zparse_scope,
        NULL
    };

    return zparse_or(str, end, parsers);
}

struct treenode* zparse_loopstat(const char* str, char** end)
{
    static const parser_f parsers[] = {
        &zparse_break,
        &zparse_continue,
        &zparse_stat,
        &zparse_loopscope,
        NULL
    };

    return zparse_or(str, end, parsers);
}

struct treenode* zparse_switchstat(const char* str, char** end)
{
    static const parser_f parsers[] = {
        &zparse_case,
        &zparse_default,
        &zparse_loopstat,
        NULL
    };

    return zparse_or(str, end, parsers);
}

struct treenode* zparse_body(const char* str, char** end, parser_f declparser, parser_f statparser)
{
    struct treenode* body, *decls = NULL;
    struct token tok = tokstr("{}");
    body = treenode_create(&tok, sizeof(struct token));

    decls = zparse_any(str, end, declparser, body);
    if (statparser) {
        zparse_any(decls ? *end : str, end, statparser, body);
    }
    
    return body;
}

static struct treenode* zparse_brackets(const char* str, char** end, parser_f declparser, parser_f statparser)
{
    if (zparse_check(str, end, '{')) {
        struct treenode *body = zparse_body(*end, end, declparser, statparser);
        if (!zparse_check(*end, end, '}')) {
            zprintf("Expected '}' at the end of scope.\n");
            ast_free(body);
            return NULL;
        }
        return body;
    }
    return NULL;
}

struct treenode* zparse_scope(const char* str, char** end)
{
    return zparse_brackets(str, end, &zparse_declstat, &zparse_statement);
}

struct treenode* zparse_loopscope(const char* str, char** end)
{
    return zparse_brackets(str, end, &zparse_declstat, &zparse_loopstat);
}

struct treenode* zparse_switchscope(const char* str, char** end)
{
    return zparse_brackets(str, end, &zparse_switchstat, NULL);
}

struct treenode* zparse_declargs(const char* str, char** end)
{
    struct treenode* decl = zparse_decl(str, end);
    if (decl) {
        struct treenode* identifier = zparse_identifier(*end, end);
        if (identifier) {
            treenode_push(decl, identifier);
        }
    }
    return decl;
}

struct treenode* zparse_funcsign(const char* str, char** end)
{
    struct treenode* decl = zparse_decl(str, end);
    if (decl) {
        struct treenode* identifier = zparse_identifier(*end, end);
        if (!identifier) {
            treenode_free(decl);
            return NULL;
        }
        treenode_push(identifier, decl);
        decl = identifier;

        if (!zparse_check(*end, end, '(')) {
            treenode_free(decl);
            return NULL;
        }

        zparse_enum(*end, end, &zparse_declargs, ',', decl);
        if (!zparse_check(*end, end, ')')) {
            zprintf("Expected ) after function parameter declaration.\n");
            treenode_free(decl);
            return NULL;
        }
    }
    return decl;
}

struct treenode* zparse_func(const char* str, char** end)
{
    struct treenode* func = zparse_funcsign(str, end);
    if (func) {
        if (!zparse_check(*end, end, ';')) {
            struct treenode* scope = zparse_scope(*end, end);
            if (!scope) {
                zprintf("Expected scope or semicolon after function declaration.\n");
                treenode_free(func);
                return NULL;
            }
            treenode_push(func, scope);
        }
    }
    return func;
}

struct treenode* zparse_modulestat(const char* str, char** end)
{
    static const parser_f parsers[] = {
        &zparse_func,
        &zparse_declstat,
        NULL
    };

    return zparse_or(str, end, parsers);
}

struct treenode* zparse_module(const char* str, char** end)
{
    struct token tok;
    struct treenode* module;
    tok = tokstr("module");
    module = treenode_create(&tok, sizeof(struct token));
    zparse_any(str, end, &zparse_modulestat, module);
    return module;
}

/*

*** GRAMMAR ***

enum(x, char) := x | x char enum(x, char)
optional(x) := x | EMPTY
opt(x) := optional(x)
or(x, y) := x | y

file := <func> <file> | <decl-statement> <file> | EMTPY
funcdecl := <func-signature> ';'
funcdef := <func-signature> <scope>
func-signature := <decl> <identifier> '(' enum(decl, ',') ')'
func-arguments := <decl> <identifier> '(' enum(decl <identifier>, ',') ')'

scope := '{' <body> '}' | EMPTY
body := opt<decl-statement> opt(enum(statement, (';')))
statement := <expr> ';' | <if-statement> | <for-statement> | <while-statement> | <do-statement> | <return-statement> | <scope>

if-statement := "if" '(' <expr> ')' <scope> opt<else->statement>
else-statement := "else" optional<if-statement> <scope>
switch-statement := "switch"
for-statement := "for" '(' <expr> ';' <expr> ';' <expr> ')' <scope>
while-statement := "while" '(' <expr> ')' <scope>
do-statement := "do" <scope> "while" '(' <expr> ')'
return-statement := "return" <expr> ';'

decl-statement := <decl> enum(var, ',') ';'
decl := <storage> <qualifier> <signed> <size> <type>
var := <identifier> | <identifier> '=' <expr>

storage := "static" | "extern" | "register" | EMPTY
qualifier := "const" | "volatile" | EMPTY
signed := "signed" | "unsigned" | EMPTY
size := "short" | "long" | EMPTY
indirection := '*' <qualifier> | EMPTY
type := <indirection> <type> |  ("char" | "int" | "void") | EMPTY

identifier := IDENTIFIER_TERMINAL

*/

int main(const int argc, const char** argv)
{
    size_t len;
    char* module, *end = NULL;
    const char* path;
    struct treenode* ast;

    if (argc < 2) {
        zprintf("Missing input expression.\n");
        return Z_EXIT_FAILURE;
    }

    path = argv[1];
    module = zcc_fread(path, &len);
    if (!module) {
        zprintf("Could not open file '%s'\n", path);
        return Z_EXIT_FAILURE;
    }

    ast = zparse_module(module, &end);
    treenode_print(ast, 0);
    
    if (ast && *end) {
        zprintf("Unconsummed: '%s'\n", end);
    }

    ast_free(ast);
    zfree(module);
    return Z_EXIT_SUCCESS;
}
