#include "ast.h"
#include <stdlib.h>
#include <stdio.h>

static void expr_print(ast_expr *expr);
static void type_print(ast_type *type);
static void decl_print(ast_decl *decl);

ast_decl *decl_init(ast_type *type, strvec *name, ast_expr *expr,
		    ast_decl *next)
{
	ast_decl *ret = malloc(sizeof(*ret));
	ret->type = type;
	ret->name = name;
	ret->expr = expr;
	ret->next = next;
	return ret;
}

ast_type *type_init(token_t type, strvec *name)
{
	ast_type *ret = malloc(sizeof(*ret));
	ret->type = type;
	ret->name = name;
	return ret;
}

ast_expr *expr_init(expr_t kind, ast_expr *left, ast_expr *right, token_t op,
		    strvec *name, int int_lit, strvec *str_lit)
{
	ast_expr *ret = malloc(sizeof(*ret));
	ret->kind = kind;
	ret->left = left;
	ret->right = right;
	ret->op = op;
	ret->name = name;
	ret->int_lit = int_lit;
	ret->string_literal = str_lit;
	return ret;
}

void type_destroy(ast_type *type)
{
	if (!type)
		return;
	strvec_destroy(type->name);
	free(type);
}

void expr_destroy(ast_expr *expr)
{
	if (!expr)
		return;
	expr_destroy(expr->right);
	expr_destroy(expr->left);
	strvec_destroy(expr->name);
	strvec_destroy(expr->string_literal);
	free(expr);
}

void decl_destroy(ast_decl *decl)
{
	if (!decl)
		return;
	type_destroy(decl->type);
	strvec_destroy(decl->name);
	expr_destroy(decl->expr);
	free(decl);
}

void ast_free(ast_decl *program)
{
	if (!program)
		return;
	ast_decl *next = program->next;
	decl_destroy(program);
	ast_free(next);
}

static void print_op(ast_expr *expr)
{
	switch (expr->op) {
	case T_PLUS:
		printf(" + ");
		break;
	case T_MINUS:
		printf(" - "); // todo fix printing with space even in unary minus.
		break;
	case T_FSLASH:
		printf(" / ");
		break;
	case T_STAR:
		printf(" * ");
		break;
	case T_DPLUS:
		printf("++");
		break;
	case T_DMINUS:
		printf("--");
		break;
	case T_NOT:
		printf("!");
		break;
	case T_BW_NOT:
		printf("~");
		break;
	case T_LT:
		printf(" < ");
		break;
	case T_LTE:
		printf(" <= ");
		break;
	case T_GT:
		printf(" > ");
		break;
	case T_GTE:
		printf(" >= ");
		break;

	default:
		printf(" ??? ");
	}
}
static void expr_print(ast_expr *expr)
{
	if (!expr)
		return;
	switch (expr->kind) {
	case E_PRE_UNARY:
		print_op(expr);
		expr_print(expr->left);
		break;
	case E_COMPARISON:
	case E_MULDIV:
	case E_ADDSUB:
		expr_print(expr->left);
		print_op(expr);
		expr_print(expr->right);
		break;
	case E_PAREN:
		printf("(");
		expr_print(expr->left);
		printf(")");
		break;
	case E_INT_LIT:
		printf("%d", expr->int_lit);
		break;
	case E_IDENTIFIER:
		strvec_print(expr->name);
		break;
	case E_FALSE_LIT:
		printf("false");
		break;
	case E_TRUE_LIT:
		printf("true");
		break;
	default:
		printf("(unsupported expr)");
	}
}

static void type_print(ast_type *type)
{
	switch (type->type) {
	case T_I32:
		printf("i32");
		break;
	case T_U32:
		printf("u32");
		break;
	case T_I64:
		printf("i64");
		break;
	case T_U64:
		printf("u64");
		break;
	case T_VOID:
		printf("void");
		break;
	case T_BOOL:
		printf("bool");
		break;
	default:
		printf("UNSUPPORTED TYPE!");
	}
}

static void decl_print(ast_decl *decl)
{
	type_print(decl->type);
	printf(" ");
	strvec_print(decl->name);
	if (decl->expr != 0) {
		printf(" = ");
		expr_print(decl->expr);
	}
	printf(";");
}

void program_print(ast_decl *program)
{
	if (!program)
		return;
	decl_print(program);
	printf("\n");
	program_print(program->next);
}
