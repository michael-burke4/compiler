#include "typecheck.h"
#include <stdio.h>
#include "symbol_table.h"

#include "print.h"

extern int had_error;

void typecheck_program(ast_decl *program)
{
	if (!program)
		return;
	typecheck_decl(program);
	typecheck_program(program->next);
}

void typecheck_decl(ast_decl *decl)
{
	// TODO: prohibit duplicate declarations within same scope!
	ast_type *expr_type;
	if (decl->expr) {
		expr_type = derive_expr_type(decl->expr);
		if (!(type_equals(decl->typesym->type, expr_type))) {
			had_error = 1;
			puts("Failed typecheck TODO: good error messages.");
		}
		type_destroy(expr_type);

	}
	scope_bind(decl->typesym);
}

/*
Assumes that expr is a function call.

In order for this to return a type the function must find that:

	(a) a function of the same name exists in the symbol table
	(b) the provided function's list of parameters must match with the aforementioned
		symbol table function's arg list.
assuming these conditions are met, return the return type found in the symbol table.

should these conditions fail, return zero.
*/
static ast_type *typecheck_fncall(ast_expr *expr)
{
	ast_expr *e_current;
	ast_typed_symbol *st_current;
	ast_typed_symbol *st_fn = scope_lookup(expr->name);
	ast_type *derived;
	int flag = 0;

	if (!st_fn) {
		printf("call to undeclared function \"");
		strvec_print(expr->name);
		puts("\"");
		return 0;
	}
	st_current = st_fn->type->arglist;
	e_current = expr->left;
	if (!e_current && !st_current)
		return type_copy(st_fn->type->subtype);
	if (!e_current || !st_current) {
		puts("Argument count mismatch");
		return 0;
	}

	while (e_current->left && st_current) {
		derived = derive_expr_type(e_current->left);
		if (!type_equals(st_current->type, derived)) {
			printf("Type mismatch in call to function ");
			strvec_print(expr->name);
			printf(": expression ");
			expr_print(e_current->left);
			printf(" does not resolve to positional argument's expected type (");
			type_print(st_current->type);
			puts(")");
			had_error = 1;
			flag = 1;
		}
		type_destroy(derived);
		st_current = st_current->next;
		e_current = e_current->right;
	}
	if (flag)
		return 0;
	if (e_current->left || st_current) {
		puts("Argument count mismatch");
		return 0;
	}
	return type_copy(st_fn->type->subtype);
}

static int is_numeric_type(ast_type *t)
{
	if (!t)
		return 0;
	switch (t->kind) {
	case Y_I32:
	case Y_I64:
	case Y_U32:
	case Y_U64:
		return 1;
	default:
		return 0;
	}
}
ast_type *derive_expr_type(ast_expr *expr)
{
	ast_typed_symbol *ts = 0;
	ast_type *left;
	ast_type *right;
	if (!expr)
		return 0;
	switch (expr->kind) {
	case E_ADDSUB:
	case E_MULDIV:
		left = derive_expr_type(expr->left);
		right = derive_expr_type(expr->right);
		if (type_equals(left, right) && is_numeric_type(left)) {
			type_destroy(right);
			return left;
		}
		had_error = 1;
		type_destroy(left);
		type_destroy(right);
		return 0;
	case E_EQUALITY:
	case E_INEQUALITY:
		left = derive_expr_type(expr->left);
		right = derive_expr_type(expr->right);
		if (type_equals(left, right) && is_numeric_type(left)) {
			type_destroy(left);
			type_destroy(right);
			return type_init(Y_BOOL, 0);
		}
		had_error = 1;
		type_destroy(left);
		type_destroy(right);
		return 0;
	case E_ASSIGN:
	case E_PAREN:
	case E_PRE_UNARY:
	case E_POST_UNARY:
		return 0;
	case E_STR_LIT:
		return type_init(Y_STRING, 0);
	case E_CHAR_LIT:
		return type_init(Y_CHAR, 0);
	case E_TRUE_LIT:
	case E_FALSE_LIT:
		return type_init(Y_BOOL, 0);
	case E_INT_LIT: // TODO: worry about i32, i64, u32, u64
		return type_init(Y_I32, 0);
	case E_FNCALL:
		return typecheck_fncall(expr);
	case E_IDENTIFIER:
		ts = scope_lookup(expr->name);
		if (ts) {
			return type_copy(ts->type);
		}
		printf("Use of undeclared identifier '");
		strvec_print(expr->name);
		puts("'");
		return 0;
	default:
		puts("unsupported expr kind while typechecking!");
		had_error = 1;
		return 0;
	}
}

void typecheck_stmt(ast_stmt *stmt)
{
	if (!stmt)
		puts("Typechecking empty statement?");
}

static int arglist_equals(ast_typed_symbol *a, ast_typed_symbol *b)
{
	if (!a && !b)
		return 1;
	if (!a || !b)
		return 0;
	return type_equals(a->type, b->type) && arglist_equals(a->next, b->next);
}
int type_equals(ast_type *a, ast_type *b)
{
	if (!a && !b)
		return 1;
	if (!a || !b)
		return 0;
	return type_equals(a->subtype, b->subtype) && arglist_equals(a->arglist, b->arglist) &&
	       a->kind == b->kind;
}
