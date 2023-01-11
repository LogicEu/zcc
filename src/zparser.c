#include <zstdlib.h>
#include <zstring.h>
#include <zparser.h>
#include <zlexer.h>
#include <ztoken.h>
#include <zsolver.h>
#include <zio.h>

typedef struct treenode* (*parser_f)(const char*, char**);

static struct treenode* zparse_object(const char* str, char** end);
static struct treenode* zparse_operator_postfix(const char* str, char** end);
static struct treenode* zparse_operator_access(const char* str, char** end);
static struct treenode* zparse_term(const char* str, char** end);
static struct treenode* zparse_paren(const char* str, char** end, parser_f parser);
static struct treenode* zparse_any(const char* str, char** end, parser_f parser, struct treenode* node);
static struct treenode* zparse_enum(const char* str, char** end, parser_f parser, const char c, struct treenode* node);

static int zparse_check(const char* str, char** end, const char c)
{
    struct token tok = ztoknext(str);
    if (tok.type != ZTOK_NULL && tok.len == 1 && tok.str[0] == c) {
        *end = tokend(tok);
        return 1;
    }
    return 0;
}

static struct treenode* zparse_char(const char* str, char** end, const char c)
{
    struct token tok = ztoknext(str);
    if (tok.type != ZTOK_NULL && tok.len == 1 && tok.str[0] == c) {
        *end = tokend(tok);
        return treenode_create(&tok, sizeof(struct token));
    }
    return NULL;
}

static struct treenode* zparse_token(const char* str, char** end, const char* symbol)
{
    size_t len;
    struct token tok = ztoknext(str);
    len = zstrlen(symbol);
    if (tok.type != ZTOK_NULL && len == tok.len && !zmemcmp(tok.str, symbol, len)) {
        *end = tokend(tok);
        return treenode_create(&tok, sizeof(struct token));
    }
    return NULL;
}

static struct treenode* zparse_identifier(const char* str, char** end)
{
    struct token tok = ztoknext(str);
    if (tok.type == ZTOK_ID) {
        *end = tokend(tok);
        return treenode_create(&tok, sizeof(struct token));
    }
    return NULL;
}

static struct treenode* zparse_operand(const char* str, char** end)
{
    struct token tok = ztoknext(str);
    if (tok.type == ZTOK_NUM) {
        *end = tokend(tok);
        return treenode_create(&tok, sizeof(struct token));
    }
    else if (tok.type == ZTOK_STR) {
        struct treenode* strnode = treenode_create(&tok, sizeof(struct token));
        *end = tokend(tok);
        if (tok.type == ZTOK_STR && tok.str[0] == '"') {
            tok = ztoknext(*end);
            while (tok.type == ZTOK_STR && tok.str[0] == '"') {
                struct token* t = strnode->data;
                *t = ztokappend(t, &tok);
                *end = tokend(tok);
                tok = ztoknext(*end);
            }
        }
        return strnode;
    } else if (tok.type == ZTOK_ID) {
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

static struct treenode* zparse_sizeof(const char* str, char** end)
{
    struct treenode* sizeofnode = zparse_token(str, end, "sizeof");
    if (sizeofnode) {
        struct treenode* expr = zparse_paren(*end, end, &zparse_object);
        if (!expr) {
            expr = zparse_term(*end, end);
            if (!expr) {
                zcc_log("Illegal sizeof operand.\n");
                zparse_free(sizeofnode);
                return NULL;
            }
        }
        treenode_push(sizeofnode, expr);
    }
    return sizeofnode;
}

static struct treenode* zparse_operator_unary(const char* str, char** end)
{
    char c;
    struct token tok = ztoknext(str);

    c = tok.str[0];
    if (c != '+' && c != '-' && c != '~' && c != '!' && c != '&' && c != '*') {
        return NULL;
    }

    *end = tokend(tok);
    return treenode_create(&tok, sizeof(struct token));
}

static struct treenode* zparse_operator_binary(const char* str, char** end)
{
    static const char* nonbinary = "]}),;:?~\\@#$`'\"";

    int i;
    char c;
    struct token tok = ztoknext(str);
    
    if (tok.type != ZTOK_SYM) {
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

static struct treenode* zparse_expr(const char* str, char** end);

static struct treenode* zparse_operator_ternary(const char* str, char** end)
{
    struct treenode* operator = zparse_char(str, end, '?');
    if (operator) {
        struct treenode* expr = zparse_expr(*end, end);
        if (!expr) {
            zcc_log("Expected expression after '?' ternary operator.\n");
            zparse_free(operator);
            return NULL;
        }
        treenode_push(operator, expr);

        if (!zparse_check(*end, end, ':')) {
            zcc_log("Expected ':' after first ternary expression.\n");
            zparse_free(operator);
            return NULL;
        }

        expr = zparse_expr(*end, end);
        if (!expr) {
            zcc_log("Expected expression after ':' ternary operator.\n");
            zparse_free(operator);
            return NULL;
        }
        treenode_push(operator, expr);
    }
    return operator;
}

static struct treenode* zparse_funcall(const char* str, char** end)
{
    struct treenode* args, *check;
    struct token tok = ztokstr("()");
    args = treenode_create(&tok, sizeof(struct token));
    check = zparse_enum(str, end, &zparse_expr, ',', args);
    if (!zparse_check(check ? *end : str, end, ')')) {
        zcc_log("Expected closing parenthesis after function call.\n");
        zparse_free(args);
        args = NULL;
    }
    return args;
}

static struct treenode* zparse_operator_access(const char* str, char** end)
{
    char c;
    struct treenode* node;
    struct token tok = ztoknext(str);

    c = tok.str[0];
    if (c == '(') {
        *end = tokend(tok);
        node = zparse_funcall(*end, end);
        if (!node) {
            zcc_log("Expected function call.\n");
        }
        return node;
    } else if (c == '[') {
        *end = tokend(tok);
        node = zparse_expr(*end, end);
        if (!node) {
            zcc_log("Expected expression inside [] array accessor.\n");
            zparse_free(node);
            return NULL;
        }
        if (!zparse_check(*end, end, ']')) {
            zcc_log("Expected closing ] parenthesis in array accessor.\n");
            zparse_free(node);
            return NULL;
        }
        return node;
    } else if (c == '.' || (c == '-' && tok.str[1] == '>')) {
        struct treenode* identifier;
        *end = tokend(tok);
        node = treenode_create(&tok, sizeof(struct token));
        identifier = zparse_identifier(*end, end);
        if (!identifier) {
            zcc_log("Expected identifier after %s operator.\n", ztokbuf(&tok));
            zparse_free(node);
            return NULL;
        }
        treenode_push(node, identifier);
        return node;
    } else {
        return NULL;
    }

    return treenode_create(&tok, sizeof(struct token));
}

static struct treenode* zparse_operator_postfix(const char* str, char** end)
{
    char c;
    struct token tok = ztoknext(str);

    c = tok.str[0];
    if ((c != '-' || tok.str[1] != '-') || (c != '+' || tok.str[1] != '+')) {
        return NULL;
    }

    *end = tokend(tok);
    return treenode_create(&tok, sizeof(struct token));
}

static struct treenode* zparse_term(const char* str, char** end)
{
    struct treenode *term = zparse_operator_unary(str, end);
    if (term) {
        struct treenode* realterm = zparse_term(*end, end);
        if (!realterm) {
            zcc_log("Expected term after unary operator %s in expression.\n", ztokbuf(term->data));
            zparse_free(term);
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
                zcc_log("Expected term after type cast operator %s in expression.\n", ztokbuf(term->data));
                zparse_free(term);
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

    lookahead = ztoknext(str);
    if (lookahead.str[0] == '?') {
        op = zparse_operator_ternary(*end, end);
        if (!op) {
            return NULL;
        }
        treenode_push(op, lhs);
        return op;
    }

    precedence = zsolve_precedence(lookahead.str);

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
        
        lookahead = ztoknext(*end);
        next_precedence = zsolve_precedence(lookahead.str);

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
            lookahead = ztoknext(*end);
            next_precedence = zsolve_precedence(lookahead.str);
        }
        
        treenode_push(op, lhs);
        treenode_push(op, rhs);
        lhs = op;

        precedence = zsolve_precedence(lookahead.str);
    }

    return lhs;
}

static struct treenode* zparse_expr(const char* str, char** end)
{
    struct treenode* lhs = zparse_term(str, end);
    return lhs ? zparse_expr_rhs(*end, end, lhs, 20) : NULL;
}

static struct treenode* zparse_constexpr(const char* str, char** end)
{
    struct treenode* expr = zparse_expr(str, end);
    if (expr) {
        struct token* tok;
        zparse_reduce(expr);
        tok = expr->data;
        if (tok->type != ZTOK_DEF && tok->type != ZTOK_NUM && tok->str[0] != '\'') {
            static const char sizeofstr[] = "sizeof";
            if ((tok->len == sizeof(sizeofstr) - 1 && !zmemcmp(tok->str, sizeofstr, sizeof(sizeofstr) - 1)) || 
                (tok->str[0] == '&' && tok->len == 1)) {
                return expr;
            }
            zparse_free(expr);
            expr = NULL;
        }
    }
    return expr;
}

/* COMPOSERS */

static struct treenode* zparse_keywords(const char* str, char** end, const char** keywords)
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

static struct treenode* zparse_or(const char* str, char** end, const parser_f* parsers)
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

static struct treenode* zparse_any(const char* str, char** end, parser_f parser, struct treenode* root)
{
    struct treenode* node = parser(str, end);
    while (node) {
        treenode_push(root, node);
        node = parser(*end, end);
    }
    return root;
}

static struct treenode** zparse_chain(const char* str, char** end, const parser_f* parsers)
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

static struct treenode* zparse_enum(const char* str, char** end, parser_f parser, const char c, struct treenode* node)
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

static struct treenode* zparse_paren(const char* str, char** end, parser_f parser)
{
    char* mark = *end;
    if (zparse_check(str, end, '(')) {
        struct treenode* node = parser(*end, end);
        if (!node) {
            *end = mark;
            return NULL;
        }
        
        if (!zparse_check(*end, end, ')')) {
            zcc_log("Expected closing parenthesis.\n");
            zparse_free(node);
            *end = mark;
            return NULL;
        }
        
        return node;
    }
    return NULL;
}

/* C-Parser */

static struct treenode* zparse_storage(const char* str, char** end)
{
    static const char* keywords[] = {"static", "extern", "register", "typedef", NULL};
    return zparse_keywords(str, end, keywords);
}

static struct treenode* zparse_qualifier(const char* str, char** end)
{
    static const char* keywords[] = {"const", "volatile", NULL};
    return zparse_keywords(str, end, keywords);
}

static struct treenode* zparse_signedness(const char* str, char** end)
{
    static const char* keywords[] = {"signed", "unsigned", NULL};
    return zparse_keywords(str, end, keywords);
}

static struct treenode* zparse_sizeness(const char* str, char** end)
{
    static const char* keywords[] = {"short", "long", NULL};
    return zparse_keywords(str, end, keywords);
}

static struct treenode* zparse_type(const char* str, char** end)
{
    static const char* keywords[] = {"char", "int", "float", "double", "void", "size_t", "parser_f", NULL};
    return zparse_keywords(str, end, keywords);
}

static struct treenode* zparse_array(const char* str, char** end);
static struct treenode* zparse_declstat(const char* str, char** end);

static struct treenode* zparse_objstruct(const char* str, char** end)
{
    struct treenode* structnode = zparse_token(str, end, "struct");
    if (structnode) {
        struct treenode* identifier;
        identifier = zparse_identifier(*end, end);
        if (!identifier) {
            zcc_log("Expected identifier after struct keyword.\n");
            zparse_free(structnode);
            return NULL;
        }

        if (zparse_check(*end, end, '{')) {
            zparse_any(*end, end, &zparse_declstat, structnode);
            if (!zparse_check(*end, end, '}')) {
                zcc_log("Expected } after struct definition.\n");
                zparse_free(identifier);
                zparse_free(structnode);
                return NULL;
            }
        }
        treenode_push(identifier, structnode);
        structnode = identifier;
    }
    return structnode;
}

static struct treenode* zparse_list(const char* str, char** end)
{
    if (zparse_check(str, end, '{')) {
        struct token tok;
        struct treenode* list;
        tok = ztokstr("{}");
        list = treenode_create(&tok, sizeof(struct token));
        zparse_enum(*end, end, &zparse_expr, ',', list);
        if (!zparse_check(*end, end, '}')) {
            zcc_log("Expected } after initializer list.\n");
            zparse_free(list);
            return NULL;
        }
        return list;
    }
    return NULL;
}

static struct treenode* zparse_assignment(const char* str, char** end, parser_f parser)
{
    struct treenode* assignment = zparse_char(str, end, '=');
    if (assignment) {
        struct treenode* expr = parser(*end, end);
        if (!expr) {
            zcc_log("Assignment not followed by expression when declaring variable.\n");
            zparse_free(assignment);
            return NULL;
        }

        treenode_push(assignment, expr);
    }
    return assignment;
}

static struct treenode* zparse_enumstat(const char* str, char** end)
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

static struct treenode* zparse_objenum(const char* str, char** end)
{
    struct treenode* enumnode = zparse_token(str, end, "enum");
    if (enumnode) {
        struct treenode* identifier = zparse_identifier(*end, end);
        if (!identifier) {
            zcc_log("Expected identifier after enum keyword.\n");
            zparse_free(enumnode);
            return NULL;
        }

        if (zparse_check(*end, end, '{')) {
            zparse_enum(*end, end, &zparse_enumstat, ',', enumnode);
            if (!zparse_check(*end, end, '}')) {
                zcc_log("Expected } after enum definition.\n");
                zparse_free(identifier);
                zparse_free(enumnode);
                return NULL;
            }
        }
        treenode_push(identifier, enumnode);
        enumnode = identifier;
    }
    return enumnode;
}

static struct treenode* zparse_objunion(const char* str, char** end)
{
    struct treenode* unionnode = zparse_token(str, end, "union");
    if (unionnode) {
        struct treenode* identifier = zparse_identifier(*end, end);
        if (!identifier) {
            zcc_log("Expected identifier after enum keyword.\n");
            zparse_free(unionnode);
            return NULL;
        }

        if (zparse_check(*end, end, '{')) {
            zparse_enum(*end, end, &zparse_enumstat, ',', unionnode);
            if (!zparse_check(*end, end, '}')) {
                zcc_log("Expected } after enum definition.\n");
                zparse_free(identifier);
                zparse_free(unionnode);
                return NULL;
            }
        }
        treenode_push(identifier, unionnode);
        unionnode = identifier;
    }
    return unionnode;
}

static struct treenode* zparse_varid(const char* str, char** end);
static struct treenode* zparse_declargs(const char* str, char** end);

static struct treenode* zparse_funcptr(const char* str, char** end)
{
    struct treenode* varid = zparse_paren(str, end, &zparse_varid);
    if (varid) {
        if (!zparse_check(*end, end, '(')) {
            zcc_log("Invalid syntax for function pointers.\n");
            zparse_free(varid);
            return NULL;
        }

        zparse_enum(*end, end, &zparse_declargs, ',', varid);
        if (!zparse_check(*end, end, ')')) {
            zcc_log("Expected ) after function pointer declaration.\n");
            zparse_free(varid);
            varid = NULL;
        }
    }
    return varid;
}

static struct treenode* zparse_exprdecl(const char* str, char** end)
{
    static const parser_f parsers[] = {
        &zparse_expr,
        &zparse_list,
        NULL
    };

    return zparse_or(str, end, parsers);
}

static struct treenode* zparse_indirection(const char* str, char** end)
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

static struct treenode* zparse_array(const char* str, char** end)
{
    if (zparse_check(str, end, '[')) {
        struct token tok;
        struct treenode* expr, *node;
        expr = zparse_constexpr(*end, end);
        if (!zparse_check(*end, end, ']')) {
            zcc_log("Expected ] at array declaration.\n");
            if (expr) {
                zparse_free(expr);
            }
        }

        tok = ztokstr("[]");
        node = treenode_create(&tok, sizeof(struct token));
        if (expr) {
            treenode_push(node, expr);
        }
        return node;
    }
    return NULL;
}

static struct treenode* zparse_object(const char* str, char** end)
{
    static const parser_f parsers[] = {
        &zparse_type,
        &zparse_objstruct,
        &zparse_objenum,
        &zparse_objunion,
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

static struct treenode* zparse_varid(const char* str, char** end)
{
    struct treenode* identifier, *indirection, *array;
    indirection = zparse_indirection(str, end);

    identifier = zparse_identifier(indirection ? *end : str, end);
    if (!identifier) {
        if (indirection) {
            zparse_free(indirection);
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

static struct treenode* zparse_variable(const char* str, char** end)
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

static struct treenode* zparse_decl(const char* str, char** end)
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
            struct token tok = ztokstr("int");
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

static struct treenode* zparse_declstat(const char* str, char** end)
{
    struct treenode* decl = zparse_decl(str, end);
    if (decl) {
        struct token tok;
        struct treenode *parent;

        tok = ztokstr(":=");
        parent = treenode_create(&tok, sizeof(struct token));
        treenode_push(parent, decl);
        decl = parent;
        
        zparse_enum(*end, end, &zparse_variable, ',', decl);
        if (!zparse_check(*end, end, ';')) {
            zcc_log("Expected ';' at the end of declaration.\n");
            zparse_free(decl);
            return NULL;
        }
    }
    return decl;
}

static struct treenode* zparse_nullstat(const char* str, char** end)
{
    if (zparse_check(str, end, ';')) {
        struct token tok = ztokstr("null");
        return treenode_create(&tok, sizeof(struct token));
    }
    return NULL;
}

static struct treenode* zparse_exprdef(const char* str, char** end)
{
    struct treenode* expr = zparse_expr(str, end);
    if (expr) {
        struct token tok;
        struct treenode* parent;
        
        tok = ztokstr("(!)");
        parent = treenode_create(&tok, sizeof(struct token));
        treenode_push(parent, expr);
        expr = parent;

        if (zparse_check(*end, end, ',')) {
            zparse_enum(*end, end, &zparse_expr, ',', expr);
        }
    }

    return expr ? expr : zparse_nullstat(str, end);
}

static struct treenode* zparse_exprstat(const char* str, char** end)
{
    struct treenode* expr = zparse_expr(str, end);
    if (expr) {
        struct token tok;
        struct treenode* parent;
        zcc_log("[%s]\n", ztokbuf(expr->data));
        tok = ztokstr("(!)");
        parent = treenode_create(&tok, sizeof(struct token));
        treenode_push(parent, expr);
        expr = parent;

        if (zparse_check(*end, end, ',')) {
            zparse_enum(*end, end, &zparse_expr, ',', expr);
        }

        if (!zparse_check(*end, end, ';')) {
            zcc_log("Expected ';' after expression inside scope.\n");
            zparse_tree_print(expr, 0);
            zparse_free(expr);
            return NULL;
        }
    }

    return expr ? expr : zparse_nullstat(str, end);
}

static struct treenode* zparse_return(const char* str, char** end)
{
    struct treenode* retnode = zparse_token(str, end, "return");
    if (retnode) {
        struct treenode* expr;
        expr = zparse_expr(*end, end);
        if (!expr) {
            return retnode;
        }

        if (!zparse_check(*end, end, ';')) {
            zcc_log("Expected ';' at the end of return expression.\n");
            zparse_free(retnode);
            zparse_free(expr);
            return NULL;
        }

        treenode_push(retnode, expr);
    }
    return retnode;
}

static struct treenode* zparse_while_expr(const char* str, char** end)
{
    struct treenode* whilenode = zparse_token(str, end, "while");
    if (whilenode) {
        struct treenode* expr = zparse_paren(*end, end, &zparse_expr);
        if (!expr) {
            zcc_log("Expected parenthesised expression after while kwyword.\n");
            zparse_free(whilenode);
            return NULL;
        }
        treenode_push(whilenode, expr);
    }
    return whilenode;
}

static struct treenode* zparse_statement(const char* str, char** end);
static struct treenode* zparse_loopstat(const char* str, char** end);
static struct treenode* zparse_switchstat(const char* str, char** end);

static struct treenode* zparse_while(const char* str, char** end)
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

static struct treenode* zparse_do(const char* str, char** end)
{
    struct treenode* donode = zparse_token(str, end, "do");
    if (donode) {
        struct treenode* scope, *whilenode;
        scope = zparse_loopstat(*end, end);
        if (!scope) {
            zcc_log("Expected scope to match do statement.\n");
            zparse_free(donode);
            return NULL;
        }

        treenode_push(donode, scope);
        whilenode = zparse_while_expr(*end, end);
        if (!whilenode) {
            zcc_log("Expected while to match do statement.\n");
            zparse_free(donode);
            return NULL;
        }

        treenode_push(donode, whilenode);
    }
    return donode;
}

static struct treenode* zparse_else(const char* str, char** end);

static struct treenode* zparse_if(const char* str, char** end)
{
    struct treenode* ifnode = zparse_token(str, end, "if");
    if (ifnode) {
        struct treenode* expr, *scope, *elsenode;
        expr = zparse_paren(*end, end, &zparse_expr);
        if (!expr) {
            zcc_log("Expected parenthesised expression after if keyword.\n");
            zparse_free(ifnode);
            return NULL;
        }
        treenode_push(ifnode, expr);

        scope = zparse_statement(*end, end);
        if (!scope) {
            zcc_log("Expected scope to match if statement.\n");
            zparse_free(ifnode);
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

static struct treenode* zparse_else(const char* str, char** end)
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

static struct treenode* zparse_for(const char* str, char** end)
{
    struct treenode* fornode = zparse_token(str, end, "for");
    if (fornode) {
        int i;
        struct treenode* scope, *expr;

        if (!zparse_check(*end, end, '(')) {
            zcc_log("Expected parenthesis after for keyword.\n");
            zparse_free(fornode);
            return NULL;
        }

        for (i = 0; i < 2; ++i) {
            expr = zparse_exprstat(*end, end);
            if (!expr) {
                zcc_log("For loop must contain three expressions within declaration.\n");
                zparse_free(fornode);
                return NULL;
            }
            treenode_push(fornode, expr);
        }

        expr = zparse_exprdef(*end, end);
        if (!expr) {
            zcc_log("Invalid for loop syntax declaration.\n");
            zparse_free(fornode);
            return NULL;
        }
        treenode_push(fornode, expr);

        if (!zparse_check(*end, end, ')')) {
            zcc_log("Expected closing parenthesis after for loop declaration.\n");
            zparse_free(fornode);
            return NULL;
        }

        scope = zparse_loopstat(*end, end);
        if (!scope) {
            zcc_log("Expected body after for loop declaration.\n");
            zparse_free(fornode);
            return NULL;
        }

        treenode_push(fornode, scope);
    }
    return fornode;
}

static struct treenode* zparse_goto(const char* str, char** end)
{
    struct treenode* gotonode = zparse_token(str, end, "goto");
    if (gotonode) {
        struct treenode* identifier = zparse_identifier(*end, end);
        if (!identifier) {
            zcc_log("Expected label identifier after goto keyword.\n");
            zparse_free(gotonode);
            return NULL;
        }
        
        treenode_push(gotonode, identifier);
        if (!zparse_check(*end, end, ';')) {
            zcc_log("Expected semicolon after goto statement.\n");
            zparse_free(gotonode);
            return NULL;
        }
    }
    return gotonode;
}

static struct treenode* zparse_label(const char* str, char** end)
{
    struct treenode* label = zparse_identifier(str, end);
    if (label && !zparse_check(*end, end, ':')) {
        zparse_free(label);
        return NULL;
    }
    return label;
}

static struct treenode* zparse_continue(const char* str, char** end)
{
    struct treenode* contnode = zparse_token(str, end, "continue");
    if (contnode && !zparse_check(*end, end, ';')) {
        zcc_log("Expected semicolon after continue keyword.\n");
        zparse_free(contnode);
        return NULL;
    }
    return contnode;
}

static struct treenode* zparse_break(const char* str, char** end)
{
    struct treenode* breaknode = zparse_token(str, end, "break");
    if (breaknode && !zparse_check(*end, end, ';')) {
        zcc_log("Expected semicolon after break keyword.\n");
        zparse_free(breaknode);
        return NULL;
    }
    return breaknode;
}

static struct treenode* zparse_case(const char* str, char** end)
{
    struct treenode* casenode = zparse_token(str, end, "case");
    if (casenode) {
        struct treenode* constexpr = zparse_constexpr(*end, end);
        if (!constexpr) {
            zcc_log("Expected constant expression after case keyword.\n");
            zcc_log("%s\n", *end);
            zparse_free(casenode);
            return NULL;
        }

        treenode_push(casenode, constexpr);
        if (!zparse_check(*end, end, ':')) {
            zcc_log("Expected semicolon after case statement.\n");
            zparse_free(casenode);
            return NULL;
        }
    }
    return casenode;
}

static struct treenode* zparse_default(const char* str, char** end)
{
    struct treenode* defaultnode = zparse_token(str, end, "default");
    if (defaultnode) {
        if (!zparse_check(*end, end, ':')) {
            zcc_log("Expected semicolon after default statement.\n");
            zparse_free(defaultnode);
            return NULL;
        }
    }
    return defaultnode;
}

static struct treenode* zparse_switchscope(const char* str, char** end);

static struct treenode* zparse_switch(const char* str, char** end)
{
    struct treenode* switchnode = zparse_token(str, end, "switch");
    if (switchnode) {
        struct treenode* expr;
        expr = zparse_paren(*end, end, &zparse_expr);
        if (!expr) {
            zcc_log("Expected parenthesised expression after switch keyword.\n");
            zparse_free(expr);
            return NULL;
        }
        treenode_push(switchnode, expr);

        expr = zparse_switchscope(*end, end);
        if (!expr) {
            zcc_log("Expected body after switch declaration.\n");
            zparse_free(switchnode);
            return NULL;
        }

        treenode_push(switchnode, expr);
    }
    return switchnode;
}

static struct treenode* zparse_scope(const char* str, char** end);
static struct treenode* zparse_loopscope(const char* str, char** end);

static struct treenode* zparse_stat(const char* str, char** end)
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

static struct treenode* zparse_statement(const char* str, char** end)
{
    static const parser_f parsers[] = {
        &zparse_stat,
        &zparse_scope,
        NULL
    };

    return zparse_or(str, end, parsers);
}

static struct treenode* zparse_loopstat(const char* str, char** end)
{
    static const parser_f parsers[] = {
        &zparse_break,
        &zparse_continue,
        &zparse_loopscope,
        &zparse_stat,
        NULL
    };

    return zparse_or(str, end, parsers);
}

static struct treenode* zparse_switchstat(const char* str, char** end)
{
    static const parser_f parsers[] = {
        &zparse_case,
        &zparse_default,
        &zparse_loopstat,
        NULL
    };

    return zparse_or(str, end, parsers);
}

static struct treenode* zparse_body(const char* str, char** end, parser_f declparser, parser_f statparser)
{
    struct treenode* body, *decls = NULL;
    struct token tok = ztokstr("{}");
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
        if (body && !zparse_check(*end, end, '}')) {
            zcc_log("Expected '}' at the end of scope.\n");
            zparse_free(body);
            return NULL;
        }
        return body;
    }
    return NULL;
}

static struct treenode* zparse_scope(const char* str, char** end)
{
    return zparse_brackets(str, end, &zparse_declstat, &zparse_statement);
}

static struct treenode* zparse_loopscope(const char* str, char** end)
{
    return zparse_brackets(str, end, &zparse_declstat, &zparse_loopstat);
}

static struct treenode* zparse_switchscope(const char* str, char** end)
{
    return zparse_brackets(str, end, &zparse_switchstat, NULL);
}

static struct treenode* zparse_declargs(const char* str, char** end)
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

static struct treenode* zparse_funcsign(const char* str, char** end)
{
    struct treenode* decl = zparse_decl(str, end);
    if (decl) {
        struct treenode* identifier = zparse_identifier(*end, end);
        if (!identifier) {
            zparse_free(decl);
            return NULL;
        }
        treenode_push(identifier, decl);
        decl = identifier;

        if (!zparse_check(*end, end, '(')) {
            zparse_free(decl);
            return NULL;
        }

        zparse_enum(*end, end, &zparse_declargs, ',', decl);
        if (!zparse_check(*end, end, ')')) {
            zcc_log("Expected ) after function parameter declaration.\n");
            zparse_free(decl);
            return NULL;
        }
    }
    return decl;
}

static struct treenode* zparse_func(const char* str, char** end)
{
    struct treenode* func = zparse_funcsign(str, end);
    if (func) {
        if (!zparse_check(*end, end, ';')) {
            struct treenode* scope = zparse_scope(*end, end);
            if (!scope) {
                zcc_log("Expected scope or semicolon after function declaration.\n");
                zparse_free(func);
                return NULL;
            }
            treenode_push(func, scope);
        }
    }
    return func;
}

static struct treenode* zparse_modulestat(const char* str, char** end)
{
    static const parser_f parsers[] = {
        &zparse_func,
        &zparse_declstat,
        NULL
    };

    return zparse_or(str, end, parsers);
}

/* Interface */

struct treenode* zparse_module(const char* str, char** end)
{
    struct token tok;
    struct treenode* module;
    tok = ztokstr("module");
    module = treenode_create(&tok, sizeof(struct token));
    zparse_any(str, end, &zparse_modulestat, module);
    return module;
}

struct treenode* zparse_source(const char* str)
{
    char* end = (char*)(size_t)str;
    return zparse_module(str, &end);
}

void zparse_free(struct treenode* node)
{
    if (node) {
        int i;
        struct token* tok = node->data;
        if (tok->type == ZTOK_DEF) {
            zfree((void*)(size_t)tok->str);
            zmemset(tok, 0, sizeof(struct token));
        }

        for (i = 0; node->children[i]; ++i) {
            zparse_free(node->children[i]);
        }

        zfree(node->data);
        zfree(node->children);
        zfree(node);
    }
}

void zparse_reduce(struct treenode* root)
{
    long i;
    const struct token *op;
    if (!root || !root->children[0]) {
        return;
    }

    for (i = 0; root->children[i]; ++i) {
        zparse_reduce(root->children[i]);
    }

    op = root->data;
    if (op->type == ZTOK_SYM) {
        const struct token* lhs, *rhs;
        lhs = root->children[0]->data;
        if (lhs->type != ZTOK_NUM && (lhs->type != ZTOK_DEF || !_isdigit(lhs->str[0]))) {
            return;
        }

        if (!root->children[1]) {
            struct token tok = ztoknum(zsolve_unary(zatol(ztokbuf(lhs)), op->str));
            zparse_free(root->children[0]);
            root->children[0] = NULL;
            zmemcpy(root->data, &tok, sizeof(struct token));
            return;
        } 
        
        rhs = root->children[1]->data;
        if (rhs->type == ZTOK_NUM || rhs->type == ZTOK_DEF) {
            struct token tok = ztoknum(zsolve_binary(zatol(ztokbuf(lhs)), zatol(ztokbuf(rhs)), op->str));
            zparse_free(root->children[0]);
            zparse_free(root->children[1]);
            zmemset(root->children, 0, sizeof(struct token) * 2);
            zmemcpy(root->data, &tok, sizeof(struct token));
        }
    }
}

void zparse_tree_print(const struct treenode* node, const size_t lvl)
{
    size_t i;
    if (!node) {
        zcc_log("Null Tree\n");
        return;
    }

    for (i = 0; i < lvl; ++i) {
        zcc_log("  ");
    }

    zcc_log("╚> %s\n", ztokbuf(node->data));
    for (i = 0; node->children[i]; ++i) {
        zparse_tree_print(node->children[i], lvl + 1);
    }
}
