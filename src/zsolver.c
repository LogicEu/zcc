#include <zsolver.h>
#include <zlexer.h>
#include <ztoken.h>
#include <zassert.h>

long zsolve_unary(const long l, const char* p)
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

long zsolve_binary(const long l, const long r, const char* p)
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

int zsolve_tree(const struct treenode* root, long* val)
{
    if (root) {
        const struct token* op = root->data;
        if (op->type == ZTOK_ID || op->type == ZTOK_STR) {
            return 0;
        }

        if (op->type == ZTOK_NUM) {
            *val = zatol(ztokbuf(op));
            return 1;
        }

        if (op->type == ZTOK_SYM && *root->children) {
            long l, r;
            if (!zsolve_tree(root->children[0], &l)) {
                return 0;
            }

            if (root->children[1]) {
                if (!zsolve_tree(root->children[1], &r)) {
                    return 0;
                }
                *val = zsolve_binary(l, r, op->str);
            } else {
                *val = zsolve_unary(l, op->str);
            }
            return 1;
        }
    }
    return 0;
}

int zsolve_precedence(const char* str)
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

long zsolve_stack(const char* str)
{
    long out[0xff], outcount = 0, stackcount = 0, n, u = 1;
    struct token stack[0xff], tok = ztok_get(str);

    while (tok.str) {
        zassert(!_isalpha(*tok.str));
        switch (*tok.str) {
            case 0:
                break;
            case '0': case '1': case '2': case '3': case '4':
            case '5': case '6': case '7': case '8': case '9':
                out[outcount++] = zatol(tok.str);
                u = 0;
                break;
            case '(':
                stack[stackcount++] = tok;
                ++u;
                break;
            case ')':
                while (stackcount && *stack[stackcount - 1].str != '(') {
                    if (stack[stackcount - 1].type == ZTOK_SYM_OP_UNARY) {
                        out[outcount - 1] = zsolve_unary(out[outcount - 1], stack[--stackcount].str);
                        continue;
                    }
                    n = out[--outcount];
                    out[outcount - 1] = zsolve_binary(out[outcount - 1], n, stack[--stackcount].str);
                }
                stackcount = stackcount ? stackcount - 1: stackcount;
                break;
            default:
                tok.type = u ? ZTOK_SYM_OP_UNARY : ZTOK_SYM_OP_RIGHT;
                while (stackcount && *stack[stackcount - 1].str != '(' &&
                    zsolve_precedence(tok.str) >= zsolve_precedence(stack[stackcount - 1].str)) {
                    if (stack[stackcount - 1].type == ZTOK_SYM_OP_UNARY) {
                        out[outcount - 1] = zsolve_unary(out[outcount - 1], stack[--stackcount].str);
                        continue;
                    }
                    n = out[--outcount];
                    out[outcount - 1] = zsolve_binary(out[outcount - 1], n, stack[--stackcount].str);
                }
                stack[stackcount++] = tok;
                ++u;
        }
        tok = ztok_nextl(tok);
    }

    while (stackcount) {
        if (stack[stackcount - 1].type == ZTOK_SYM_OP_UNARY) {
            out[outcount - 1] = zsolve_unary(out[outcount - 1], stack[--stackcount].str);
            continue;
        }
        n = out[--outcount];
        out[outcount - 1] = zsolve_binary(out[outcount - 1], n, stack[--stackcount].str);
    }

    return out[0];
}
