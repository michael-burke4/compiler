#ifndef AST_H
#define AST_H

#include "token.h"
#include "strvec.h"

typedef struct ast_decl {
	struct ast_type *type;
	strvec *name;
	struct ast_expr *expr;
	struct ast_decl *next;
} ast_decl;

typedef struct ast_type {
	token_t type;
	strvec *name;
} ast_type;

typedef enum {
	E_ADDSUB,
	E_MULDIV,
	E_PAREN,
	E_INT_LIT,
	E_STR_LIT,
	E_CHAR_LIT,
	E_FALSE_LIT,
	E_TRUE_LIT,
	E_IDENTIFIER,
	E_PRE_UNARY,
	E_COMPARISON,
	E_ASSIGN,
} expr_t;

typedef struct ast_expr {
	expr_t kind;
	struct ast_expr *left;
	struct ast_expr *right;
	token_t op;
	strvec *name;
	int int_lit;
	strvec *string_literal;
} ast_expr;

/*
 * typedef struct ast_stmt...
 *
 */

ast_decl *decl_init(ast_type *type, strvec *name, ast_expr *expr,
		    ast_decl *next);
ast_type *type_init(token_t type, strvec *name);
ast_expr *expr_init(expr_t kind, ast_expr *left, ast_expr *right, token_t op,
		    strvec *name, int int_lit, strvec *str_lit);
void program_print(ast_decl *program);
void ast_free(ast_decl *program);
void type_destroy(ast_type *type);
void expr_destroy(ast_expr *expr);
void decl_destroy(ast_decl *decl);

#endif
