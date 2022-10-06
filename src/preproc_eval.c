#include <preproc.h>

static int oppres(const char* op)
{
    switch (op[0]) {
        case '!': return 3 + 7 * (op[1] == '=');
        case '*':
        case '/':
        case '%': return 5;
        case '+':
        case '-': return 6;
        case '<':
        case '>': return 9 - 2 * (op[1] == op[0]);
        case '=': return 10;
        case '^': return 12;
        case '&': return 11 + 3 * (op[1] == op[0]);
        case '|': return 13 + 2 * (op[1] == op[0]);
        case ',': return 16;
    }
    return 16;
}

static bnode_t* tree_parse_expression(const char* str, range_t* args, size_t argcount, size_t* iter, const int p)
{
    char c = str[args[*iter].start];
    /*ppc_log_range(str, args[*iter]);*/
    const size_t bytes = sizeof(range_t);
    range_t *token = args + *iter;
    bnode_t* root;

    if (chrparen(c)) {
        (*iter)++;
        range_t paren = tokenrange(args, argcount, parenrange(str, token->start));
        root = tree_parse_expression(str, args, paren.end - 1, iter, 16);
        *iter = paren.end - 1;
    }
    else root = bnode_create(args + *iter, bytes);
    bnode_t* subroot = root;

    for (++(*iter); *iter < argcount; ++(*iter)) {
        
        range_t* token = args + *iter;
        bnode_t* node = bnode_create(token, bytes);
        c = str[token->start];
        
        /*ppc_log("...");
        ppc_log_range(str, *token);*/
        
        if (chralpha(c) || chrdigit(c) || c == '.') {
            bnode_connect(subroot, node);
            continue;
        }
        
        if (chrparen(c)) {
            (*iter)++;
            range_t paren = tokenrange(args, argcount, parenrange(str, token->start));
            bnode_free(node);
            node = tree_parse_expression(str, args, paren.end - 1, iter, 16);
            bnode_connect(subroot, node);
            *iter = paren.end - 1;
            continue;
        }
            
        int precedence = oppres(str + token->start);
        if (precedence < p) {
            ++(*iter);
            bnode_t* rec = tree_parse_expression(str, args, argcount, iter, precedence);
            bnode_connect(node, subroot);
            bnode_connect(node, rec);
            root = node;
            subroot = node;
            continue;
        }

        --(*iter);
        break;
    }

    return root;
}

static long tree_eval_unary(const char* str, const long l)
{
    switch (str[0]) {
        case '+': return l;
        case '-': return -l;
        case '!': return !l;
    }
    return 0;
}

static long tree_eval_binary(const char* str, const long l, const long r)
{
    switch (str[0]) {
        case '*': return l * r;
        case '/': return l / r;
        case '%': return l % r;
        case '+': return l + r;
        case '-': return l - r;
        case '=': return l == r;
        case '^': return l ^ r;
        case '<': return str[1] == str[0] ? l << r : l < r;
        case '>': return str[1] == str[0] ? l >> r : l > r;
        case '&': return str[1] == str[0] ? l && r : l & r;
        case '|': return str[1] == str[0] ? l || r : l | r;
    }
    return 0;
}

long tree_eval(const bnode_t* root, const char* str)
{
    if (!root->data) {
        ppc_log("Preprocessor expression is not valid.\n");
        return 0;
    }

    const range_t r = *(range_t*)root->data;
    const char* c = str + r.start;

    if (chrdigit(*c) || *c == '.') {
        return atol(strrange(str, r));
    }

    if (chralpha(*c)) {
        ppc_log("Preprocesor macro identifier '%s' is not defined.\n", strrange(str, r));
        ppc_log_range(str, (range_t){r.start - 32, r.start + 32});
        return 0;
    }

    if (!root->left) {
        ppc_log("Preprocesor macro operator '%s' has no factor.\n", strrange(str, r));
        return 0;
    }

    const long left = tree_eval(root->left, str);
    /*ppc_log("L-%ld\n", left);*/
    
    if (!root->right) {
        return tree_eval_unary(strrange(str, r), left);
    }

    if (c[0] == '&' && c[1] == '&' && !left) {
        return 0;
    }
    else if (c[0] == '|' && c[1] == '|' && left) {
        return 1;
    }

    const long right = tree_eval(root->right, str);
    /*ppc_log("R-%ld\n", right);*/

    return tree_eval_binary(c, left, right);
}

bnode_t* tree_parse(const char* str, range_t* args, const size_t argcount)
{
    size_t iter = 0;
    return tree_parse_expression(str, args, argcount, &iter, 18);
}