#include "error.h"
#include "parse.h"
#include "print.h"
#include "symbol_table.h"
#include "typecheck.h"
#include "util.h"

#include <stdio.h>

// TODO: at least line numbers in error messages.

extern int had_error;

static int in_loop = 0;

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
		fprintf(stderr, "NULL FN IN BINDING ARGS?!");
		had_error = 1;
		return;
	}
	arglist = decl->typesym->type->arglist;
	if (arglist == NULL)
		return;
	for (i = 0 ; i < arglist->size ; ++i)
		scope_bind_ts(arglist_get(arglist, i));
}

static void typecheck_return(ast_stmt *stmt) {
	ast_type *typ;
	if (stmt->next != NULL) {
		had_error = 1;
		eputs("Return statements must be at the end of statement blocks.");
	}
	// typ does not 'own' this - don't free this value.
	typ = scope_get_return_type();
	if (typ == NULL) {
		had_error = 1;
		eputs("Cannot use a return statement outside of a function");
	} else if (stmt->expr == NULL) {
		if (typ->kind != Y_VOID) {
			had_error = 1;
			eputs("Non-void function has empty return statement.");
		}
	} else {
		// Dangerous reuse of typ: now 'owns' what is points to. Must free.
		typ = derive_expr_type(stmt->expr);
		if (!type_equals(typ, scope_get_return_type())) {
			had_error = 1;
			eputs("Return value does not match function signature");
		}
		type_destroy(typ);
	}
}


// if `if` and `else` branch of a S_IFELSE end with ret and the S_IFELSE has
// no stmt->next
// This does have to be done recursively. For example:
// let fn: () -> i32 = {
//	if (condition) {
//		// arbitrary amount
//		// of code
//		if (other_condition) {
//			// more code ...
//			return 0;
//		}
//		else {
//			// more code ...
//			return 1;
//		}
//	} else {
//		// more code ...
//		if (other_condition) {
//			// more code ...
//			return 2;
//		}
//		else {
//			// more code ...
//			return 3;
//		}
//	}
// }
// PRECONDITIONS:
// 	stmt must be non-null.
// 	stmt->next must be null.
static int is_return_worthy(ast_stmt *stmt) {
	if (stmt->kind == S_RETURN) {
		typecheck_return(stmt);
		// typecheck_return can still set had_error and print an error message:
		// what matters to is_return_worthy is that this statement is return-worthy!
		return 1;
	}
	if (stmt->kind == S_IFELSE) {
		if (stmt->else_body == NULL) {
			return 0;
		}
		return is_return_worthy(last(stmt->body->body)) && is_return_worthy(last(stmt->else_body->body));
	}
	return 0;
}

static void append_retvoid_if_needed(ast_decl *fn) {
	if (fn == NULL || fn->typesym->type->subtype->kind != Y_VOID || fn->body == NULL || fn->body->body == NULL)
		return;
	ast_stmt *s = last(fn->body->body);
	if (!is_return_worthy(s))
		s->next = stmt_init(S_RETURN, 0, 0, 0, 0);
}

static void typecheck_fnbody(ast_decl *decl)
{
	if (decl == NULL || decl->body == NULL) {
		//TODO: Look at this.
		fprintf(stderr, "Currently not allowing empty fn declarations. Provide an fn body.");
		had_error = 1;
		return;
	}
	scope_enter();
	scope_bind_return_type(decl->typesym->type->subtype);
	scope_bind_args(decl);
	append_retvoid_if_needed(decl);
	typecheck_stmt(decl->body->body, 1);
	scope_exit();
}


static void typecheck_array_initializer(ast_decl *decl)
{
	ast_type *t;
	if (decl->initializer == NULL)
		return;
	if (decl->typesym->type->kind != Y_POINTER && decl->typesym->type->kind != Y_CONSTPTR) {
		eputs("Can only use initializers with pointers.");
		had_error = 1;
		return;
	}
	for (size_t i = 0 ; i < decl->initializer->size ; ++i) {
		t = derive_expr_type(decl->initializer->elements[i]);
		if (!type_equals(t, decl->typesym->type->subtype)) {
			eputs("Type mismatch in array initializer");
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
		fprintf(stderr, "Typechecking empty decl!?!?!");
		had_error = 1;
		return;
	}
	if (scope_lookup_current(decl->typesym->symbol)) {
		fprintf(stderr, "Duplicate declaration of symbol \"");
		fstrvec_print(stderr, decl->typesym->symbol);
		fprintf(stderr, "\"\n");
		had_error = 1;
		return;
	}
	if (decl->typesym->type->kind == Y_VOID) {
		had_error = 1;
		eputs("Can't declare variable with type void");
	} else if (decl->typesym->type->kind == Y_FUNCTION) {
		scope_bind_ts(decl->typesym);
		typecheck_fnbody(decl);
	} else if (decl->typesym->type->kind == Y_STRUCT && decl->typesym->type->name == NULL) {
		scope_bind_ts(decl->typesym);
	} else if (decl->expr) {
		if (decl->typesym->type->kind == Y_POINTER && decl->typesym->type->subtype->kind == Y_CHAR
				&& decl->expr->kind == E_STR_LIT) {
			ast_expr *e;
			decl->initializer = vect_init(decl->expr->string_literal->size);
			for (size_t i = 0 ; i < decl->initializer->capacity ; ++i) {
				strvec *c = strvec_init(1);
				strvec_append(c, decl->expr->string_literal->text[i]);
				e = expr_init(E_CHAR_LIT, 0, 0, 0, 0, 0, c);
				vect_append(decl->initializer, e);
			}
			scope_bind_ts(decl->typesym);
			return;
		}
		typ = derive_expr_type(decl->expr);
		if (assignment_rhs_promotable(decl->typesym->type, typ)) {
			decl->expr = build_cast(decl->expr, decl->typesym->type->kind);
		} else if (!(type_equals(decl->typesym->type, typ))) {
			had_error = 1;
			fprintf(stderr, "Cannot assign type ");
			ftype_print(stderr, typ);
			fprintf(stderr, " to type ");
			ftype_print(stderr, decl->typesym->type);
			eputs("");
		}
		type_destroy(typ);
		scope_bind_ts(decl->typesym);
	} else if (decl->initializer) {
		if (decl->typesym->type->kind != Y_POINTER && decl->typesym->type->kind != Y_CONSTPTR) {
			had_error = 1;
			eputs("Only pointers can use array initializers");
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
	if (fn_ts == NULL) {
		fprintf(stderr, "Call to undeclared function \"");
		fstrvec_print(stderr, expr->name);
		eputs("\"");
		had_error = 1;
		return NULL;
	} else if (fn_ts->type->kind != Y_FUNCTION) {
		fprintf(stderr, "Identifier \"");
		fstrvec_print(stderr, expr->name);
		eputs("\" does not refer to a function.");
		had_error = 1;
		return NULL;
	}
	vect *decl_arglist = fn_ts->type->arglist;
	vect *expr_arglist = expr->sub_exprs;
	ast_type *derived;
	size_t i;
	int flag = 0;
	if (decl_arglist == NULL && expr_arglist == NULL)
		return type_copy(fn_ts->type->subtype);
	if ((decl_arglist == NULL || expr_arglist == NULL) || (decl_arglist->size != expr_arglist->size)) {
		fprintf(stderr, "Argument count mismatch");
		return NULL;
	}

	for (i = 0 ; i < decl_arglist->size ; ++i) {
		derived = derive_expr_type(expr_arglist->elements[i]);
		if (!type_equals(arglist_get(decl_arglist, i)->type, derived)) {
			fprintf(stderr, "Type mismatch in call to function ");
			fstrvec_print(stderr, expr->name);
			fprintf(stderr, ": expression ");
			e_expr_print(expr_arglist->elements[i]);
			fprintf(stderr, " does not resolve to positional argument's expected type (");
			e_type_print(arglist_get(decl_arglist, i)->type);
			eputs(")");
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

static ast_type *derive_assign(ast_expr *expr) {
	ast_typed_symbol *ts = NULL;
	ast_type *left = NULL;
	ast_type *right = NULL;
	int derived = 0;
	if (!expr->left->is_lvalue) {
		eputs("Assignment expression's left side must be an lvalue!");
		had_error = 1;
		return NULL;
	}
	if (expr->left->kind == E_IDENTIFIER) {
		ts = scope_lookup(expr->left->name);
		if (ts == NULL) {
			fprintf(stderr, "Use of undeclared identifier ");
			fstrvec_print(stderr, expr->left->name);
			eputs("");
			had_error = 1;
			return NULL;
		}
		left = ts->type;
	} else {
		left = derive_expr_type(expr->left);
		derived = 1;
	}
	if (left->isconst) {
		eputs("Cannot assign to const expression");
		had_error = 1;
		return NULL;
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
	fprintf(stderr, "Attempted to assign expression of type ");
	e_type_print(right);
	fprintf(stderr, " to a variable of type ");
	had_error = 1;
	e_type_print(left);
	eputs("");
	type_destroy(right);
	return NULL;
}

static ast_type *derive_pre_unary(ast_expr *expr)
{
	ast_type *left;
	ast_type *right;
	int isconst;
	switch (expr->op) {
	case T_AMPERSAND:
		left = derive_expr_type(expr->left);
		if (left == NULL || !expr->left->is_lvalue) {
			eputs("Cannot find address of non-lvalue expr");
			type_destroy(left);
			return NULL;
		}
		isconst = left->isconst;
		right = type_init(isconst ? Y_CONSTPTR : Y_POINTER, NULL);
		right->subtype = left;
		return right;
	case T_STAR:
		left = derive_expr_type(expr->left);
		if (left == NULL || (left->kind != Y_POINTER && left->kind != Y_CONSTPTR)) {
			eputs("Cannot dereference non-pointer expression");
			type_destroy(left);
			return NULL;
		}
		right = type_copy(left->subtype);
		type_destroy(left);
		return right;
	default:
		eputs("unsupported expr kind while typechecking!");
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
		if (left == NULL || (left->kind != Y_POINTER && left->kind != Y_CONSTPTR)) {
			eputs("can only use index operator on pointers");
			type_destroy(left);
			had_error = 1;
			return NULL;
		}
		right = derive_expr_type(expr->right);
		if (right == NULL || right->kind != Y_I32) {
			eputs("can only index pointers with integers");
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
			eputs("Member operator left side must be a struct");
			type_destroy(left);
			had_error = 1;
			return NULL;
		}
		if (expr->right->kind != E_IDENTIFIER) {
			eputs("Member operator right side must be an identifier");
			type_destroy(left);
			had_error = 1;
			return NULL;
		}
		ts = scope_lookup(left->name);
		ret = struct_field_type(ts, expr->right->name);
		if (ret == NULL) {
			fprintf(stderr, "struct `");
			fstrvec_print(stderr, left->name);
			fprintf(stderr, "` has no member `");
			fstrvec_print(stderr, expr->right->name);
			eputs("`");
			type_destroy(left);
			had_error = 1;
			return NULL;
		}
		type_destroy(left);
		return ret;
	default:
		eputs("unsupported post unary expr while typechecking");
		had_error = 1;
		return NULL;
	}
}


static void cast_up_if_necessary(ast_expr *expr, ast_type **left, ast_type **right)
{
	ast_type *l = *left;
	ast_type *r = *right;
	// TODO: worry about signed vs unsigned
	if (is_int_type(l) && is_int_type(r) && !type_equals(l, r)) {
		if (l->kind > r->kind) {
			expr->right = build_cast(expr->right, l->kind);
			r->kind = l->kind;
			return;
		}
		expr->right = build_cast(expr->right, l->kind);
		r->kind = l->kind;
	}
}

// TODO: use goto error pattern
ast_type *derive_expr_type(ast_expr *expr)
{
	ast_typed_symbol *ts = NULL;
	ast_type *left;
	ast_type *right;
	if (expr == NULL)
		return NULL;
	switch (expr->kind) {
	case E_LOG_OR:
	case E_LOG_AND:
		left = derive_expr_type(expr->left);
		right = derive_expr_type(expr->right);
		if (left->kind == Y_BOOL && right->kind == Y_BOOL) {
			type_destroy(right);
			return left;
		}
		had_error = 1;
		fprintf(stderr, "Typecheck failed at expression \"");
		e_expr_print(expr);
		eputs("\"");
		type_destroy(left);
		type_destroy(right);
		return NULL;
	case E_ADDSUB:
	case E_MULDIV:
		left = derive_expr_type(expr->left);
		right = derive_expr_type(expr->right);
		cast_up_if_necessary(expr, &left, &right);
		if (type_equals(left, right) && is_int_type(left)) {
			type_destroy(right);
			return left;
		}
		had_error = 1;
		fprintf(stderr, "Typecheck failed at expression \"");
		e_expr_print(expr);
		eputs("\"");
		type_destroy(left);
		type_destroy(right);
		return NULL;
	case E_EQUALITY:
	case E_INEQUALITY:
		left = derive_expr_type(expr->left);
		right = derive_expr_type(expr->right);
		cast_up_if_necessary(expr, &left, &right);
		if (type_equals(left, right) && (is_int_type(left) || left->kind == Y_CHAR)) {
			type_destroy(left);
			type_destroy(right);
			return type_init(Y_BOOL, NULL);
		}
		had_error = 1;
		fprintf(stderr, "Typecheck failed at expression \"");
		e_expr_print(expr);
		eputs("\"");
		type_destroy(left);
		type_destroy(right);
		return NULL;
	case E_ASSIGN:
		return derive_assign(expr);
	case E_PAREN:
		return derive_expr_type(expr->left);
	case E_CHAR_LIT:
		return type_init(Y_CHAR, NULL);
	case E_TRUE_LIT:
	case E_FALSE_LIT:
		return type_init(Y_BOOL, NULL);
	case E_INT_LIT:
		return type_init(expr->int_size, NULL);
	case E_STR_LIT:
		left = type_init(Y_CONSTPTR, NULL);
		left->subtype = type_init(Y_CHAR, NULL);
		return left;
	case E_FNCALL:
		return typecheck_fncall(expr);
	case E_IDENTIFIER:
		ts = scope_lookup(expr->name);
		if (ts == NULL) {
			fprintf(stderr, "Use of undeclared identifier \"");
			fstrvec_print(stderr, expr->name);
			eputs("\"");
			had_error = 1;
			return NULL;
		}
		if (ts->type->kind == Y_STRUCT && strvec_equals(ts->type->name, expr->name)) {
			fprintf(stderr, "Can't use struct type \"");
			fstrvec_print(stderr, ts->symbol);
			eputs("\" in this expression");
			had_error = 1;
			return NULL;
		}
		if (ts) {
			return type_copy(ts->type);
		}
		fprintf(stderr, "Use of undeclared identifier \"");
		fstrvec_print(stderr, expr->name);
		eputs("\"");
		had_error = 1;
		return NULL;
	case E_PRE_UNARY:
		return derive_pre_unary(expr);
	case E_POST_UNARY:
		return derive_post_unary(expr);
	default:
		eputs("unsupported expr kind while typechecking!");
		had_error = 1;
		return NULL;
	}
}

void typecheck_stmt(ast_stmt *stmt, int at_fn_top_level)
{
	ast_type *typ;
	int old_in_loop = in_loop;
	if (stmt == NULL) {
		if (at_fn_top_level) {
			had_error = 1;
			eputs("Non-void functions must end in valid return statements");
			return;
		}
		return;
	}
	if (is_return_worthy(stmt)) {
		if (stmt->next != NULL) {
			had_error = 1;
			eputs("Return-worthy statements must appear at the end of fn blocks."); // TODO: this is a bad error messsage
			return;
		}
		typecheck_stmt(stmt->body, 0);
		typecheck_stmt(stmt->else_body, 0);
		return;
	}
	switch (stmt->kind) {
	case S_ERROR:
		had_error = 1;
		eputs("WARNING typechecking a bad stmt. Big problem?!");
		typecheck_stmt(stmt->next, at_fn_top_level);
		break;
	case S_IFELSE:
		if (stmt->expr == NULL) {
			had_error = 1;
			eputs("if statement condition must be non-empty");
		}
		typ = derive_expr_type(stmt->expr);
		if (typ == NULL || typ->kind != Y_BOOL) {
			had_error = 1;
			eputs("if statement condition must be a boolean");
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
			eputs("if statement condition must be non-empty");
		}
		typ = derive_expr_type(stmt->expr);
		if (typ == NULL || typ->kind != Y_BOOL) {
			had_error = 1;
			eputs("if statement condition must be a boolean");
		}
		type_destroy(typ);
		in_loop = 1;
		if (stmt->body != NULL) {
			typecheck_stmt(stmt->body->body, 0);
		} else {
			had_error = 1;
			eputs("Empty while loop body.");
		}
		in_loop = old_in_loop;
		typecheck_stmt(stmt->next, at_fn_top_level);
		break;
	case S_BREAK:
	case S_CONTINUE:
		if (!in_loop) {
			had_error = 1;
			eputs("Break/continue statement used outside of loop");
		}
		if (stmt->next != NULL) {
			had_error = 1;
			eputs("Break/continue statements must appear at the end of statement blocks");
		}
		break;
	case S_BLOCK:
		// DO NOT DESTROY TYP HERE!
		// YOU DO NOT OWN TYP!!
		scope_enter();
		typecheck_stmt(stmt->body, 0);
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
		// If we get here, it means that we are at a return statement and stmt->next is not null.
		// That is not allowed!
		had_error = 1;
		eputs("Return statements must be at the end of statement blocks.");
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
