#ifndef ZCC_PARSER_H
#define ZCC_PARSER_H

/*** GRAMMAR ***

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

*/

#include <utopia/tree.h>

void zparse_tree_print(const struct treenode* node, const size_t lvl);
void zparse_free(struct treenode* node);
void zparse_reduce(struct treenode* node);
struct treenode* zparse_source(const char* str);
struct treenode* zparse_module(const char* str, char** end);

#endif /* ZCC_PARSER_H */
