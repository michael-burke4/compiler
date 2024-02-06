#include "typecheck.h"
#include <stdio.h>
#include "symbol_table.h"

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

ast_type *derive_expr_type(ast_expr *expr)
{
	ast_typed_symbol *ts = 0;
	if (!expr)
		return 0;
	switch (expr->kind) {
	case E_ADDSUB:
	case E_MULDIV:
	case E_EQUALITY:
	case E_INEQUALITY:
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

	case E_IDENTIFIER:
		ts = scope_lookup(expr->name);
		if (ts) {
			return type_copy(ts->type);
		}
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
