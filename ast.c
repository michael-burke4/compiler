#include "ast.h"
#include <stdlib.h>

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

ast_type *type_init(token_t type)
{
	ast_type *ret = malloc(sizeof(*ret));
	ret->type = type;
	return ret;
}

static void expr_print(ast_expr *expr)
{
	if (expr == 0)
		return; // dumb stub
	return;
}

static void type_print(ast_type *type)
{
	switch (type->type) {
	case T_I32:
		printf("i32");
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
