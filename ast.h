#ifndef AST_H
#define AST_H

#include "token.h"
#include "util.h"

#define arglist_get(list, i) ((ast_typed_symbol *)vect_get(list, i))

typedef struct ast_decl {
	struct ast_typed_symbol *typesym;
	struct ast_expr *expr;
	struct ast_stmt *body;
	struct ast_decl *next;
	struct vect *initializer;
} ast_decl;

typedef enum {
	Y_U32 = 0x08,
	Y_U64 = 0x09,
	Y_I32 = 0x18,
	Y_I64 = 0x19,
	Y_CHAR,
	Y_STRING,
	Y_BOOL,
	Y_VOID,
	Y_POINTER,
	Y_FUNCTION,
	Y_STRUCT,
} type_t;

typedef struct ast_type {
	struct ast_type *subtype;
	vect *arglist;
	type_t kind;
	strvec *name;
} ast_type;

typedef struct ast_typed_symbol {
	struct ast_type *type;
	strvec *symbol;
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
	E_FNCALL,
	E_SYSCALL,
	E_MEMBER,
	E_CAST,
} expr_t;

union num_lit {
	int8_t i8;
	int16_t i16;
	int32_t i32;
	int64_t i64;
	int8_t u8;
	int16_t u16;
	uint32_t u32;
	uint64_t u64;
	float f32;
	double f64;
};

type_t smallest_fit(union num_lit);

typedef struct ast_expr {
	expr_t kind;
	struct ast_expr *left;
	struct ast_expr *right;
	token_t op;
	strvec *name;
	union num_lit num;
	strvec *string_literal;
	vect *sub_exprs;
	int is_lvalue;
	type_t cast_to;
	type_t int_size;
} ast_expr;

void expr_add_sub_expr(ast_expr *e, ast_expr *sub);
vect *sub_exprs_init(size_t size);
void destroy_expr_vect(vect *expr_vect);

typedef enum { S_ERROR, S_BLOCK, S_DECL, S_EXPR, S_IFELSE, S_RETURN, S_WHILE } stmt_t;

typedef struct ast_stmt {
	stmt_t kind;
	struct ast_decl *decl;
	struct ast_expr *expr;
	struct ast_stmt *body;
	struct ast_stmt *else_body;
	struct ast_stmt *next;
} ast_stmt;

ast_decl *decl_init(ast_typed_symbol *typesym, ast_expr *expr, ast_stmt *stmt, ast_decl *next);
ast_type *type_init(type_t kind, strvec *name);
ast_typed_symbol *ast_typed_symbol_init(ast_type *type, strvec *symbol);
ast_expr *expr_init(expr_t kind, ast_expr *left, ast_expr *right, token_t op, strvec *name,
		    union num_lit num, strvec *str_lit);
ast_stmt *stmt_init(stmt_t kind, ast_decl *decl, ast_expr *expr, ast_stmt *body,
		    ast_stmt *else_body);
void ast_free(ast_decl *program);
void type_destroy(ast_type *type);
void expr_destroy(ast_expr *expr);
void decl_destroy(ast_decl *decl);
void stmt_destroy(ast_stmt *stmt);
void ast_typed_symbol_destroy(ast_typed_symbol *typesym);
ast_type *type_copy(ast_type *t);
vect *arglist_copy(vect *arglist);

void arglist_destroy(vect *arglist);
#endif
