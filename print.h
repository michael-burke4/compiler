#ifndef PRINT_H
#define PRINT_H
#include "ast.h"
 
void program_print(ast_decl *program);
void expr_print(ast_expr *expr);
void stmt_print(ast_stmt *stmt);
void type_print(ast_type *type);
void decl_print(ast_decl *decl);
void typed_sym_print(ast_typed_symbol *typesym);

#endif
