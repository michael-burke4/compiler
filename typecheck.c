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

static void scope_bind_args(ast_decl *decl)
{
	ast_typed_symbol *sym;
	if (!decl) {
		printf("NULL FN IN BINDING ARGS?!");
		had_error = 1;
		return;
	}
	sym = decl->typesym->type->arglist;
	while (sym) {
		//typed_sym_print(sym);
		scope_bind(sym);
		sym = sym->next;
	}
}

static void typecheck_fnbody(ast_decl *decl)
{
	if (!decl || !decl->body) {
		//TODO: Look at this.
		printf("Currently not allowing empty fn declarations. Provide an fn body.");
		had_error = 1;
		return;
	}
	scope_enter();
	scope_bind_return_type(decl->typesym->type->subtype);
	scope_bind_args(decl);
	typecheck_stmt(decl->body->body);
	scope_exit();
}

void typecheck_decl(ast_decl *decl)
{
	ast_type *typ;
	if (!decl) {
		printf("Typechecking empty decl!?!?!");
		had_error = 1;
		return;
	}
	if (scope_lookup_current(decl->typesym->symbol)) {
		printf("Duplicate declaration of symbol \"");
		strvec_print(decl->typesym->symbol);
		printf("\"\n");
		had_error = 1;
		return;
	}
	if (decl->typesym->type->kind == Y_FUNCTION) {
		scope_bind(decl->typesym);
		typecheck_fnbody(decl);
		//typ = scope_get_return_type();
		//scope_bind_return_type(decl->typesym->type->subtype);
		//typecheck_stmt(decl->body);
		//scope_bind_return_type(typ);
	} else if (decl->expr) {
		typ = derive_expr_type(decl->expr);
		if (!(type_equals(decl->typesym->type, typ))) {
			had_error = 1;
			puts("Failed typecheck TODO: good error messages.");
		}
		type_destroy(typ);
		scope_bind(decl->typesym);
	}
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
		printf("Call to undeclared function \"");
		strvec_print(expr->name);
		puts("\"");
		had_error = 1;
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
		printf("Typecheck failed at expresion \"");
		expr_print(expr);
		puts("\"");
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
		printf("Typecheck failed at expresion \"");
		expr_print(expr);
		puts("\"");
		type_destroy(left);
		type_destroy(right);
		return 0;
	case E_ASSIGN:
		if (expr->left->kind != E_IDENTIFIER || !expr->left->name) {
			puts("Assignment expression's left side must be an identifier"); // TODO arrays!!!
			had_error = 1;
			return 0;
		}
		ts = scope_lookup(expr->left->name);
		if (!ts) {
			printf("Use of undeclared identifier ");
			strvec_print(expr->left->name);
			puts("");
			had_error = 1;
			return 0;
		}
		left = ts->type;
		right = derive_expr_type(expr->right);
		if (type_equals(left, right))
			return right;
		printf("Attempted to assign expression of type ");
		type_print(right);
		printf(" to a variable of type ");
		type_print(left);
		type_destroy(right);
		puts("");
		had_error = 1;
		return 0;
	case E_PAREN:
		return derive_expr_type(expr->left);
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
		printf("Use of undeclared identifier \"");
		strvec_print(expr->name);
		puts("\"");
		had_error = 1;
		return 0;
	default:
		puts("unsupported expr kind while typechecking!");
		had_error = 1;
		return 0;
	}
}

void typecheck_stmt(ast_stmt *stmt)
{
	ast_type *typ;
	if (!stmt)
		return;
	switch (stmt->kind) {
	case S_ERROR:
		had_error = 1;
		puts("WARNING typechecking a bad stmt. Big problem?!");
		typecheck_stmt(stmt->next);
		break;
	case S_IFELSE:
		if (!stmt->expr) {
			had_error = 1;
			puts("if statement condition must be non-empty");
		}
		typ = derive_expr_type(stmt->expr);
		if (!typ || typ->kind != Y_BOOL) {
			had_error = 1;
			puts("if statement condition must be a boolean");
		}
		type_destroy(typ);
		typecheck_stmt(stmt->body->body); // TODO: don't put body in body (parser's problem)
		typecheck_stmt(stmt->else_body->body);
		typecheck_stmt(stmt->next);
		break;
	case S_WHILE:
		if (!stmt->expr) {
			had_error = 1;
			puts("if statement condition must be non-empty");
		}
		typ = derive_expr_type(stmt->expr);
		if (!typ || typ->kind != Y_BOOL) {
			had_error = 1;
			puts("if statement condition must be a boolean");
		}
		type_destroy(typ);
		typecheck_stmt(stmt->body->body);
		typecheck_stmt(stmt->next);
		break;
	case S_BLOCK:
		// DO NOT DESTROY TYP HERE!
		// YOU DO NOT OWN TYP!!
		typ = scope_get_return_type();
		scope_enter();
		scope_bind_return_type(typ);
		typecheck_stmt(stmt->next);
		scope_exit();
		break;
	case S_DECL:
		typecheck_decl(stmt->decl);
		typecheck_stmt(stmt->next);
		break;
	case S_EXPR:
		type_destroy(derive_expr_type(stmt->expr));
		typecheck_stmt(stmt->next);
		break;
	case S_PRINT:
		typ = derive_expr_type(stmt->expr);
		if (typ->kind != Y_STRING) {
			had_error = 1;
			puts("Cannot print non-string expression");
		}
		type_destroy(typ);

		typecheck_stmt(stmt->next);
		break;
	case S_RETURN:
		// See above warning about typ ownership
		if (stmt->next != 0) {
			had_error = 1;
			puts("Return statements must be at the end of statement blocks.");
		}
		typ = scope_get_return_type();
		if (!typ) {
			had_error = 1;
			puts("Cannot use a return statement outside of a function");
		} else if (!stmt->expr) {
			if (typ->kind != Y_VOID) {
				had_error = 1;
				puts("Non-void function has empty return statement.");
			}
		} else {
			// Dangerous reuse of typ: now 'owns' what is points to.
			typ = derive_expr_type(stmt->expr);
			if (!type_equals(typ, scope_get_return_type())) {
				had_error = 1;
				puts("Return value does not match function signature");
			}
			type_destroy(typ);
		}
		typecheck_stmt(stmt->next);
		break;
	}
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
