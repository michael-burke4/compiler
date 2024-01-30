#ifndef AST_H
#define AST_H

#include "token.h"
#include "util.h"

typedef struct ast_decl {
	struct ast_typed_symbol *typesym;
	struct ast_expr *expr;
	struct ast_stmt *body;
	struct ast_decl *next;
} ast_decl;

typedef struct ast_type {
	struct ast_type *subtype;
	struct ast_typed_symbol *arglist;
	token_t type;
	strvec *name;
} ast_type;

typedef struct ast_typed_symbol {
	struct ast_type *type;
	strvec *symbol;
	struct ast_typed_symbol *next; // Used for declaring functions.
} ast_typed_symbol;

typedef enum {
	E_ADDSUB,
	E_MULDIV,
	E_EQUALITY,
	E_INEQUALITY,
	E_ASSIGN,
	E_PAREN,
	E_INT_LIT,
	E_STR_LIT,
	E_CHAR_LIT,
	E_FALSE_LIT,
	E_TRUE_LIT,
	E_IDENTIFIER,
	E_PRE_UNARY,
	E_POST_UNARY,
} expr_t;

typedef struct ast_expr {
	expr_t kind;
	struct ast_expr *left;
	struct ast_expr *right;
	token_t op;
	strvec *name;
	int int_lit;
	strvec *string_literal;
	ast_type type;
} ast_expr;

typedef enum {
	S_ERROR,
	S_BLOCK,
	S_DECL,
	S_EXPR,
	S_IFELSE,
	S_RETURN
} stmt_t;

typedef struct ast_stmt {
	stmt_t kind;
	struct ast_decl *decl;
	struct ast_expr *expr;
	struct ast_stmt *body;
	struct ast_stmt *else_body;
	struct ast_stmt *next;
} ast_stmt;

ast_decl *decl_init(ast_typed_symbol *typesym, ast_expr *expr, ast_stmt *stmt, ast_decl *next);
ast_type *type_init(token_t type, strvec *name);
ast_typed_symbol *ast_typed_symbol_init(ast_type *type, strvec *symbol);
ast_expr *expr_init(expr_t kind, ast_expr *left, ast_expr *right, token_t op,
		    strvec *name, int int_lit, strvec *str_lit);
ast_stmt *stmt_init(stmt_t kind, ast_decl *decl, ast_expr *expr, ast_stmt *body,
		    ast_stmt *else_body);
void ast_free(ast_decl *program);
void type_destroy(ast_type *type);
void expr_destroy(ast_expr *expr);
void decl_destroy(ast_decl *decl);
void stmt_destroy(ast_stmt *stmt);
void ast_typed_symbol_destroy(ast_typed_symbol *typesym);
ast_type *type_copy(ast_type *t);

#endif
