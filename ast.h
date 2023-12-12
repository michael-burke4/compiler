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
} ast_type;

typedef struct ast_expr {
	token_t op;
	struct ast_expr *left;
	struct ast_expr *right;
} ast_expr;

/*
 * typedef struct ast_stmt...
 *
 */

ast_decl *decl_init(ast_type *type, strvec *name, ast_expr *expr,
		    ast_decl *next);
ast_type *type_init(token_t type);
void program_print(ast_decl *program);

#endif
