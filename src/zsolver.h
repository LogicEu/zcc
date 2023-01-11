#ifndef ZCC_SOLVER_H
#define ZCC_SOLVER_H

#include <utopia/tree.h>

long zsolve_stack(const char* str);
int zsolve_precedence(const char* str);
int zsolve_tree(const struct treenode* root, long* val);
long zsolve_binary(const long l, const long r, const char* p);
long zsolve_unary(const long l, const char* p);

#endif /* ZCC_SOLVER_H */
