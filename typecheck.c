#include "typecheck.h"
#include <stdio.h>
#include "symbol_table.h"
#include "util.h"
#include "parse.h"

#include "print.h"

extern int had_error;

void typecheck_program(ast_decl *program)
{
	if (program == NULL)
		return;
	typecheck_decl(program);
	typecheck_program(program->next);
}

static void scope_bind_args(ast_decl *decl)
{
	size_t i;
	vect *arglist;
	if (decl == NULL) {
		printf("NULL FN IN BINDING ARGS?!");
		had_error = 1;
		return;
	}
	arglist = decl->typesym->type->arglist;
	if (arglist == NULL)
		return;
	for (i = 0 ; i < arglist->size ; ++i)
		scope_bind_ts(arglist_get(arglist, i));
}

static void typecheck_fnbody(ast_decl *decl)
{
	if (decl == NULL || decl->body == NULL) {
		//TODO: Look at this.
		printf("Currently not allowing empty fn declarations. Provide an fn body.");
		had_error = 1;
		return;
	}
	scope_enter();
	scope_bind_return_type(decl->typesym->type->subtype);
	scope_bind_args(decl);
	typecheck_stmt(decl->body->body, 1);
	scope_exit();
}


static void typecheck_array_initializer(ast_decl *decl)
{
	ast_type *t;
	if (decl->initializer == NULL)
		return;
	if (decl->typesym->type->kind != Y_POINTER) {
		puts("Can only use initializers with pointers and structs.");
		had_error = 1;
		return;
	}
	for (size_t i = 0 ; i < decl->initializer->size ; ++i) {
		t = derive_expr_type(decl->initializer->elements[i]);
		if (!type_equals(t, decl->typesym->type->subtype)) {
			puts("Type mismatch in array initializer");
			had_error = 1;
			type_destroy(t);
			return;
		}
		type_destroy(t);
	}
	return;
}


static int is_int_type(ast_type *t)
{
	if (t == NULL)
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

// Returns zero if lhs and rhs are the same bit width!
static int assignment_rhs_promotable(ast_type *lhs, ast_type *rhs)
{
	if (is_int_type(lhs) && is_int_type(rhs) &&
			(lhs->kind & 0x0F) > (rhs->kind & 0x0F))
		return 1;
	return 0;
}


void typecheck_decl(ast_decl *decl)
{
	ast_type *typ;
	if (decl == NULL) {
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
		scope_bind_ts(decl->typesym);
		typecheck_fnbody(decl);
		//typ = scope_get_return_type();
		//scope_bind_return_type(decl->typesym->type->subtype);
		//typecheck_stmt(decl->body);
		//scope_bind_return_type(typ);
	} else if (decl->typesym->type->kind == Y_STRUCT && decl->typesym->type->name == NULL) {
		scope_bind_ts(decl->typesym);
	} else if (decl->expr) {
		typ = derive_expr_type(decl->expr);
		if (assignment_rhs_promotable(decl->typesym->type, typ)) {
			decl->expr = build_cast(decl->expr, decl->typesym->type->kind);
		} else if (!(type_equals(decl->typesym->type, typ))) {
			had_error = 1;
			puts("Failed typecheck TODO: good error messages.");
		}
		type_destroy(typ);
		scope_bind_ts(decl->typesym);
	} else if (decl->initializer) {
		if (decl->typesym->type->kind != Y_POINTER) {
			had_error = 1;
			puts("Only pointers can use array initializers");
			return;
		}
		typecheck_array_initializer(decl);
		scope_bind_ts(decl->typesym);
	} else
		scope_bind_ts(decl->typesym);
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
	ast_typed_symbol *fn_ts = scope_lookup(expr->name);
	vect *decl_arglist = fn_ts->type->arglist;
	vect *expr_arglist = expr->sub_exprs;
	ast_type *derived;
	size_t i;
	int flag = 0;
	if (fn_ts == NULL) {
		printf("Call to undeclared function \"");
		strvec_print(expr->name);
		puts("\"");
		had_error = 1;
		return NULL;
	}
	if (decl_arglist == NULL && expr_arglist == NULL)
		return type_copy(fn_ts->type->subtype);
	if ((decl_arglist == NULL || expr_arglist == NULL) || (decl_arglist->size != expr_arglist->size)) {
		printf("Argument count mismatch");
		return NULL;
	}

	for (i = 0 ; i < decl_arglist->size ; ++i) {
		derived = derive_expr_type(expr_arglist->elements[i]);
		if (!type_equals(arglist_get(decl_arglist, i)->type, derived)) {
			printf("Type mismatch in call to function ");
			strvec_print(expr->name);
			printf(": expression ");
			expr_print(expr_arglist->elements[i]);
			printf(" does not resolve to positional argument's expected type (");
			type_print(arglist_get(decl_arglist, i)->type);
			puts(")");
			had_error = 1;
			flag = 1;
		}
		type_destroy(derived);
		derived = NULL;
	}
	if (flag)
		return NULL;
	return type_copy(fn_ts->type->subtype);
}

static ast_type *typecheck_syscall(ast_expr *expr)
{
	vect *expr_arglist = expr->sub_exprs;
	ast_type *derived;
	if (expr_arglist->size != 4) {
		puts("CURRENTLY CAN ONLY SYSCALL IF THERE ARE 4 ARGS. SORRY");
		return NULL;
	}
	for (size_t i = 0 ; i < expr_arglist->size ; ++i) {
		derived = derive_expr_type(expr_arglist->elements[i]);
		if (derived->kind != Y_I32 && derived->kind != Y_STRING) {
			puts("syscall args can only be i32s and strings right now!");
			type_destroy(derived);
			return NULL;
		}
		type_destroy(derived);
	}
	return type_init(Y_VOID, NULL);
}

static ast_type *derive_assign(ast_expr *expr) {
	ast_typed_symbol *ts = NULL;
	ast_type *left = NULL;
	ast_type *right = NULL;
	int derived = 0;
	if (!expr->left->is_lvalue) {
		puts("Assignment expression's left side must be an lvalue!");
		had_error = 1;
		return NULL;
	}
	if (expr->left->kind == E_IDENTIFIER) {
		ts = scope_lookup(expr->left->name);
		if (ts == NULL) {
			printf("Use of undeclared identifier ");
			strvec_print(expr->left->name);
			puts("");
			had_error = 1;
			return NULL;
		}
		left = ts->type;
	} else {
		left = derive_expr_type(expr->left);
		derived = 1;
	}
	right = derive_expr_type(expr->right);
	if (type_equals(left, right)) {
		if (derived)
			type_destroy(left);
		return right;
	}
	if (is_int_type(left) && is_int_type(right)) {
		int size_mask = 0x0F;
		if ((left->kind & size_mask) > (right->kind & size_mask)) {
			expr->right = build_cast(expr->right, left->kind);
			if (derived) {
				type_destroy(right);
				return left;
			}
			type_destroy(right);
			return type_copy(left);
		}
	}
	printf("Attempted to assign expression of type ");
	type_print(right);
	printf(" to a variable of type ");
	had_error = 1;
	type_print(left);
	puts("");
	type_destroy(right);
	return NULL;
}

static ast_type *derive_pre_unary(ast_expr *expr)
{
	ast_type *left;
	ast_type *right;
	switch (expr->op) {
	case T_AMPERSAND:
		left = derive_expr_type(expr->left);
		if (left == NULL || !expr->left->is_lvalue) {
			puts("Cannot find address of non-lvalue expr");
			type_destroy(left);
			return NULL;
		}
		right = type_init(Y_POINTER, NULL);
		right->subtype = left;
		return right;
	case T_STAR:
		left = derive_expr_type(expr->left);
		if (left == NULL || left->kind != Y_POINTER) {
			puts("Cannot dereference non-pointer expression");
			type_destroy(left);
			return NULL;
		}
		right = type_copy(left->subtype);
		type_destroy(left);
		return right;
	default:
		puts("unsupported expr kind while typechecking!");
		had_error = 1;
		return NULL;
	}
}

static ast_type *struct_field_type(ast_typed_symbol *struct_ts, strvec *name) {
	vect *field_list = struct_ts->type->arglist;
	ast_typed_symbol *cur;
	for (size_t i = 0 ; i < field_list->size ; ++i) {
		cur = arglist_get(field_list, i);
		if (strvec_equals(name, cur->symbol))
			return type_copy(cur->type);
	}
	return NULL;
}
static ast_type *derive_post_unary(ast_expr *expr)
{
	ast_type *left;
	ast_type *right;
	ast_type *ret;
	ast_typed_symbol *ts;
	switch (expr->op) {
	case T_LBRACKET:
		left = derive_expr_type(expr->left);
		if (left == NULL || left->kind != Y_POINTER) {
			puts("can only use index operator on pointers");
			type_destroy(left);
			had_error = 1;
			return NULL;
		}
		right = derive_expr_type(expr->right);
		if (right == NULL || right->kind != Y_I32) {
			puts("can only index pointers with integers");
			type_destroy(left);
			type_destroy(right);
			had_error = 1;
			return NULL;
		}
		ret = type_copy(left->subtype);
		type_destroy(left);
		type_destroy(right);
		return ret;
	case T_PERIOD:
		left = derive_expr_type(expr->left);
		if (left == NULL || left->kind != Y_STRUCT || !expr->left->is_lvalue) {
			puts("Member operator left side must be a struct");
			type_destroy(left);
			had_error = 1;
			return NULL;
		}
		if (expr->right->kind != E_IDENTIFIER) {
			puts("Member operator right side must be an identifier");
			type_destroy(left);
			had_error = 1;
			return NULL;
		}
		ts = scope_lookup(left->name);
		ret = struct_field_type(ts, expr->right->name);
		if (ret == NULL) {
			printf("struct `");
			strvec_print(left->name);
			printf("` has no member `");
			strvec_print(expr->right->name);
			puts("`");
			type_destroy(left);
			had_error = 1;
			return NULL;
		}
		type_destroy(left);
		return ret;
	default:
		puts("unsupported post unary expr while typechecking");
		had_error = 1;
		return NULL;
	}
}

ast_type *derive_expr_type(ast_expr *expr)
{
	ast_typed_symbol *ts = NULL;
	ast_type *left;
	ast_type *right;
	if (expr == NULL)
		return NULL;
	switch (expr->kind) {
	case E_ADDSUB:
	case E_MULDIV:
		left = derive_expr_type(expr->left);
		right = derive_expr_type(expr->right);
		if (type_equals(left, right) && is_int_type(left)) {
			type_destroy(right);
			return left;
		}
		had_error = 1;
		printf("Typecheck failed at expression \"");
		expr_print(expr);
		puts("\"");
		type_destroy(left);
		type_destroy(right);
		return NULL;
	case E_EQUALITY:
	case E_INEQUALITY:
		left = derive_expr_type(expr->left);
		right = derive_expr_type(expr->right);
		if (type_equals(left, right) && is_int_type(left)) {
			type_destroy(left);
			type_destroy(right);
			return type_init(Y_BOOL, NULL);
		}
		had_error = 1;
		printf("Typecheck failed at expression \"");
		expr_print(expr);
		puts("\"");
		type_destroy(left);
		type_destroy(right);
		return NULL;
	case E_ASSIGN:
		return derive_assign(expr);
	case E_PAREN:
		return derive_expr_type(expr->left);
	case E_STR_LIT:
		return type_init(Y_STRING, NULL);
	case E_CHAR_LIT:
		return type_init(Y_CHAR, NULL);
	case E_TRUE_LIT:
	case E_FALSE_LIT:
		return type_init(Y_BOOL, NULL);
	case E_INT_LIT:
		return type_init(expr->int_size, NULL);
	case E_FNCALL:
		return typecheck_fncall(expr);
	case E_SYSCALL:
		return typecheck_syscall(expr);
	case E_IDENTIFIER:
		ts = scope_lookup(expr->name);
		if (ts->type->kind == Y_STRUCT && strvec_equals(ts->symbol, expr->name)) {
			printf("Can't use struct type \"");
			strvec_print(ts->symbol);
			puts("\" in this expression");
			had_error = 1;
			return NULL;
		}
		if (ts) {
			return type_copy(ts->type);
		}
		printf("Use of undeclared identifier \"");
		strvec_print(expr->name);
		puts("\"");
		had_error = 1;
		return NULL;
	case E_PRE_UNARY:
		return derive_pre_unary(expr);
	case E_POST_UNARY:
		return derive_post_unary(expr);
	default:
		puts("unsupported expr kind while typechecking!");
		had_error = 1;
		return NULL;
	}
}

void typecheck_stmt(ast_stmt *stmt, int at_fn_top_level)
{
	ast_type *typ;
	if (stmt == NULL && at_fn_top_level && 
			scope_get_return_type()->kind != Y_VOID) {
		had_error = 1;
		puts("Non-void functions must end in valid return statements");
		return;
	}
	if (stmt == NULL)
		return;
	switch (stmt->kind) {
	case S_ERROR:
		had_error = 1;
		puts("WARNING typechecking a bad stmt. Big problem?!");
		typecheck_stmt(stmt->next, at_fn_top_level);
		break;
	case S_IFELSE:
		if (stmt->expr != NULL) {
			had_error = 1;
			puts("if statement condition must be non-empty");
		}
		typ = derive_expr_type(stmt->expr);
		if (typ == NULL || typ->kind != Y_BOOL) {
			had_error = 1;
			puts("if statement condition must be a boolean");
		}
		type_destroy(typ);
		scope_enter();
		if (stmt->body != NULL)
			typecheck_stmt(stmt->body, 0);
		scope_exit();
		scope_enter();
		if (stmt->else_body != NULL)
			typecheck_stmt(stmt->else_body, 0);
		scope_exit();
		typecheck_stmt(stmt->next, at_fn_top_level);
		break;
	case S_WHILE:
		if (stmt->expr == NULL) {
			had_error = 1;
			puts("if statement condition must be non-empty");
		}
		typ = derive_expr_type(stmt->expr);
		if (typ == NULL || typ->kind != Y_BOOL) {
			had_error = 1;
			puts("if statement condition must be a boolean");
		}
		type_destroy(typ);
		if (stmt->body != NULL)
			typecheck_stmt(stmt->body->body, 0);
		typecheck_stmt(stmt->next, at_fn_top_level);
		break;
	case S_BLOCK:
		// DO NOT DESTROY TYP HERE!
		// YOU DO NOT OWN TYP!!
		typ = scope_get_return_type();
		scope_enter();
		scope_bind_return_type(typ);
		typecheck_stmt(stmt->next, 0);
		scope_exit();
		break;
	case S_DECL:
		typecheck_decl(stmt->decl);
		typecheck_stmt(stmt->next, at_fn_top_level);
		break;
	case S_EXPR:
		type_destroy(derive_expr_type(stmt->expr));
		typecheck_stmt(stmt->next, at_fn_top_level);
		break;
	case S_RETURN:
		// See above warning about typ ownership
		if (stmt->next != NULL) {
			had_error = 1;
			puts("Return statements must be at the end of statement blocks.");
		}
		typ = scope_get_return_type();
		if (typ == NULL) {
			had_error = 1;
			puts("Cannot use a return statement outside of a function");
		} else if (stmt->expr == NULL) {
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
		break;
	}
}

static int arglist_equals(vect *a, vect *b)
{
	size_t i;
	ast_typed_symbol *at;
	ast_typed_symbol *bt;
	if (a == NULL && b == NULL)
		return 1;
	if (a == NULL || b == NULL)
		return 0;
	if (a->size != b->size)
		return 0;
	for (i = 0 ; i < a->size ; ++i) {
		at = arglist_get(a, i);
		bt = arglist_get(b, i);
		if (!type_equals(at->type, bt->type))
			return 0;
	}
	return 1;
}
int type_equals(ast_type *a, ast_type *b)
{
	if (a == NULL && b == NULL)
		return 1;
	if (a == NULL || b == NULL)
		return 0;
	return type_equals(a->subtype, b->subtype) && arglist_equals(a->arglist, b->arglist) &&
	       a->kind == b->kind;
}
