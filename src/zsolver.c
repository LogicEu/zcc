#include <zsolver.h>
#include <zstd.h>
#include <zlexer.h>
#include <zassert.h>

static int oppres(const ztok_t tok)
{
    switch (*tok.str) {
        case ')':
        case '(': return 0;
        case '!': return 3 + 7 * (tok.str[1] == '=');
        case '*':
        case '/':
        case '%': return 5;
        case '+':
        case '-': return 3 + 3 * (tok.str[1] != tok.str[0] && tok.kind != ZTOK_SYM_OP_UNARY);
        case '<':
        case '>': return 9 - 2 * (tok.str[1] == tok.str[0]);
        case '=': return 10;
        case '^': return 12;
        case '&': return 11 + 3 * (tok.str[1] == tok.str[0]);
        case '|': return 13 + 2 * (tok.str[1] == tok.str[0]);
    }
    return 16;
}

static long op1(const long l, const char* p)
{
    switch (*p) {
        case '!': return !l;
        case '~': return ~l;
        case '-': return -l;
        case '+': return l;
    }
    zassert(0);
    return 0;
}

static long op2(const long l, const long r, const char* p)
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
    zcc_log("SOLVER? -> '%s'\n", p);
    zassert(0);
    return 0;
}

long zcc_solve(const char* str)
{
    long out[0xff], outcount = 0, stackcount = 0, n, u = 1;
    ztok_t tok = ztok_get(str), stack[0xff];

    while (tok.str) {
        /*zcc_log("%s\n", tok.str);*/
        zassert(!chralpha(*tok.str));
        switch (*tok.str) {
            case 0:
                break;
            case '0' ... '9':
                out[outcount++] = zatol(tok.str);
                u = 0;
                break;
            case '(':
                stack[stackcount++] = tok;
                ++u;
                break;
            case ')':
                while (stackcount && *stack[stackcount - 1].str != '(') {
                    if (stack[stackcount - 1].kind == ZTOK_SYM_OP_UNARY) {
                        out[outcount - 1] = op1(out[outcount - 1], stack[--stackcount].str);
                        continue;
                    }
                    n = out[--outcount];
                    out[outcount - 1] = op2(out[outcount - 1], n, stack[--stackcount].str);
                }
                stackcount = stackcount ? stackcount - 1: stackcount;
                break;
            default:
                tok.kind = u ? ZTOK_SYM_OP_UNARY : ZTOK_SYM_OP_RIGHT;
                while (stackcount && *stack[stackcount - 1].str != '(' &&
                    oppres(tok) >= oppres(stack[stackcount - 1])) {
                    if (stack[stackcount - 1].kind == ZTOK_SYM_OP_UNARY) {
                        out[outcount - 1] = op1(out[outcount - 1], stack[--stackcount].str);
                        continue;
                    }
                    n = out[--outcount];
                    out[outcount - 1] = op2(out[outcount - 1], n, stack[--stackcount].str);
                }
                stack[stackcount++] = tok;
                ++u;
        }
        tok = ztok_next(tok);
    }

    /*long i;
    for (i = 0; i < outcount; ++i) {
        zcc_log("[%zu] ", out[i]);
    }
    zcc_log("\n");

    for (i = 0; i < stackcount; ++i) {
        zcc_log("{%s} ", stack[i].str);
    }
    zcc_log("\n");*/

    while (stackcount) {
        if (stack[stackcount - 1].kind == ZTOK_SYM_OP_UNARY) {
            out[outcount - 1] = op1(out[outcount - 1], stack[--stackcount].str);
            continue;
        }
        n = out[--outcount];
        out[outcount - 1] = op2(out[outcount - 1], n, stack[--stackcount].str);
    }
    /*zcc_log("%s\n", str);
    zcc_log("Result: %ld\n", out[outcount - 1]);*/
    return out[0];
}

#include <stdio.h>