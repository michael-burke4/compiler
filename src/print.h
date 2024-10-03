#ifndef PRINT_H
#define PRINT_H

#include "ast.h"

void fprogram_print(FILE *f, ast_decl *program);
void fexpr_print(FILE *f, ast_expr *expr);
void fstmt_print(FILE *f, ast_stmt *stmt);
void ftype_print(FILE *f, ast_type *type);
void fdecl_print(FILE *f, ast_decl *decl);
void ftyped_sym_print(FILE *f, ast_typed_symbol *typesym);

void program_print(ast_decl *program);
void expr_print(ast_expr *expr);
void stmt_print(ast_stmt *stmt);
void type_print(ast_type *type);
void decl_print(ast_decl *decl);
void typed_sym_print(ast_typed_symbol *typesym);

void e_program_print(ast_decl *program);
void e_expr_print(ast_expr *expr);
void e_stmt_print(ast_stmt *stmt);
void e_type_print(ast_type *type);
void e_decl_print(ast_decl *decl);
void e_typed_sym_print(ast_typed_symbol *typesym);

#endif
