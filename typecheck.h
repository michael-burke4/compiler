#ifndef TYPECHECK_H
#define TYPECHECK_H
#include "ast.h"

void typecheck_program(ast_decl *program);
void typecheck_decl(ast_decl *decl);
ast_type *derive_expr_type(ast_expr *expr);
void typecheck_stmt(ast_stmt *stmt);
int type_equals(ast_type *a, ast_type *b);

#endif
