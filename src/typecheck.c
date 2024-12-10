#include "error.h"
#include "parse.h"
#include "symbol_table.h"
#include "typecheck.h"
#include "util.h"

#include <stdarg.h>
#include <stdio.h>

extern int had_error;

static int in_loop = 0;
static int cur_line = 0;

static void report_error_cur_line(const char *fmt, ...)
{
	va_list args;
	had_error = 1;
	va_start(args, fmt);
	report_error_line(cur_line, fmt, args);
	va_end(args);
}

void typecheck_program(ast_decl *program)
{
	ast_decl *cur = program;
	while (cur != NULL) {
		typecheck_decl(cur);
		cur = cur->next;
	}
}

static void scope_bind_args(ast_decl *decl)
{
	size_t i;
	vect *arglist;
	if (decl == NULL) {
		report_error_cur_line("NULL FN IN BINDING ARGS?!");
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
	cur_line = stmt->line;
	if (stmt->next != NULL) {
		report_error_cur_line("Return statements must be at the end of statement blocks.");
	}
	// typ does not 'own' this - don't free this value.
	typ = scope_get_return_type();
	if (typ == NULL) {
		report_error_cur_line("Cannot use a return statement outside of a function");
	} else if (stmt->expr == NULL) {
		if (typ->kind != Y_VOID) {
			report_error_cur_line("Non-void function has empty return statement.");
		}
	} else {
		// Dangerous reuse of typ: now 'owns' what is points to. Must free.
		typ = derive_expr_type(stmt->expr);
		if (!type_equals(typ, scope_get_return_type())) {
			report_error_cur_line("Return value does not match function signature");
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
		s->next = stmt_init(S_RETURN, 0, 0, 0, 0, fn->line);
}

static void typecheck_fnbody(ast_decl *decl)
{
	if (decl == NULL || decl->body == NULL) {
		//TODO: Look at this.
		report_error_cur_line("Currently not allowing empty fn declarations. Provide an fn body.");
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
	for (size_t i = 0 ; i < decl->initializer->size ; ++i) {
		t = derive_expr_type(decl->initializer->elements[i]);
		if (!type_equals(t, decl->typesym->type->subtype)) {
			report_error_cur_line("Type mismatch in array initializer");
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
			TYPE_WIDTH(lhs->kind) > TYPE_WIDTH(rhs->kind))
		return 1;
	return 0;
}


void typecheck_decl(ast_decl *decl)
{
	ast_type *typ;
	if (decl == NULL) {
		report_error_cur_line("Typechecking empty decl!?!?!");
		return;
	}
	cur_line = decl->line;
	if (scope_lookup_current(decl->typesym->symbol)) {
		report_error_cur_line("Duplicate symbol declaration");
		return;
	}
	if (decl->typesym->type->kind == Y_STRUCT) {
		if (decl->typesym->type->name == NULL) {
			scope_bind_ts(decl->typesym);
		}
		else {
			ast_typed_symbol *found = scope_lookup(decl->typesym->type->name);
			if (found == NULL) {
				report_error_cur_line("Can't use undeclared struct type");
			}
			scope_bind_ts(decl->typesym);
		}
	}
	if (decl->typesym->type->kind == Y_VOID) {
		report_error_cur_line("Can't declare variable with type void");
	} else if (decl->typesym->type->kind == Y_FUNCTION) {
		scope_bind_ts(decl->typesym);
		typecheck_fnbody(decl);
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
			report_error_cur_line("Assignment expression type mismatch");
		}
		type_destroy(typ);
		scope_bind_ts(decl->typesym);
	} else if (decl->initializer) {
		if (decl->typesym->type->kind != Y_POINTER && decl->typesym->type->kind != Y_CONSTPTR) {
			report_error_cur_line("Only pointers can use array initializers");
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
		report_error_cur_line("Call to undeclared function");
		return NULL;
	} else if (fn_ts->type->kind != Y_FUNCTION) {
		report_error_cur_line("Identifier does not refer to a function");
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
		report_error_cur_line("Argument count mismatch");
		return NULL;
	}

	for (i = 0 ; i < decl_arglist->size ; ++i) {
		derived = derive_expr_type(expr_arglist->elements[i]);
		if (!type_equals(arglist_get(decl_arglist, i)->type, derived)) {
			report_error_cur_line("Positional argument type mismatch in function call");
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
		report_error_cur_line("Assignment expression's left side must be an lvalue!");
		return NULL;
	}
	if (expr->left->kind == E_IDENTIFIER) {
		ts = scope_lookup(expr->left->name);
		if (ts == NULL) {
			report_error_cur_line("Use of undeclared identifier");
			return NULL;
		}
		left = ts->type;
	} else {
		left = derive_expr_type(expr->left);
		derived = 1;
	}
	if (left != NULL && left->isconst) {
		report_error_cur_line("Cannot assign to const expression");
		return NULL;
	}
	right = derive_expr_type(expr->right);
	if (type_equals(left, right)) {
		if (derived)
			type_destroy(left);
		return right;
	}
	if (is_int_type(left) && is_int_type(right)) {
		if (TYPE_WIDTH(left->kind) > TYPE_WIDTH(right->kind)) {
			expr->right = build_cast(expr->right, left->kind);
			if (derived) {
				type_destroy(right);
				return left;
			}
			type_destroy(right);
			return type_copy(left);
		}
	}
	report_error_cur_line("Mismatched types in assigment statement");
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
			report_error_cur_line("Cannot find address of non-lvalue expr");
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
			report_error_cur_line("Cannot dereference non-pointer expression");
			type_destroy(left);
			return NULL;
		}
		right = type_copy(left->subtype);
		type_destroy(left);
		return right;
	case T_MINUS:
		left = derive_expr_type(expr->left);
		if (!is_int_type(left)) {
			report_error_cur_line("Cannot use unary negative operator on non-integer type.");
			type_destroy(left);
			return NULL;
		}
		return left;
	case T_BW_NOT:
		left = derive_expr_type(expr->left);
		if (!is_int_type(left)) {
			report_error_cur_line("Cannot use unary bitwise not operator on non-integer type.");
			type_destroy(left);
			return NULL;
		}
		return left;
	case T_NOT:
		left = derive_expr_type(expr->left);
		if (left->kind != Y_BOOL) {
			report_error_cur_line("Cannot use unary not operator on non-bolean type.");
			type_destroy(left);
			return NULL;
		}
		return left;
	default:
		report_error_cur_line("unsupported expr kind while typechecking!");
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
			report_error_cur_line("can only use index operator on pointers");
			type_destroy(left);
			return NULL;
		}
		right = derive_expr_type(expr->right);
		if (right == NULL || right->kind != Y_I32) {
			report_error_cur_line("can only index pointers with integers");
			type_destroy(left);
			type_destroy(right);
			return NULL;
		}
		ret = type_copy(left->subtype);
		type_destroy(left);
		type_destroy(right);
		return ret;
	case T_PERIOD:
		left = derive_expr_type(expr->left);
		if (left == NULL || left->kind != Y_STRUCT || !expr->left->is_lvalue) {
			report_error_cur_line("Member operator left side must be a struct");
			type_destroy(left);
			return NULL;
		}
		if (expr->right->kind != E_IDENTIFIER) {
			report_error_cur_line("Member operator right side must be an identifier");
			type_destroy(left);
			return NULL;
		}
		ts = scope_lookup(left->name);
		ret = struct_field_type(ts, expr->right->name);
		if (ret == NULL) {
			report_error_cur_line("Attempted to access non-existent struct member.");
			type_destroy(left);
			return NULL;
		}
		type_destroy(left);
		return ret;
	default:
		report_error_cur_line("unsupported post unary expr while typechecking");
		return NULL;
	}
}


static void cast_up_if_necessary(ast_expr *expr, ast_type **left, ast_type **right)
{
	ast_type *l = *left;
	ast_type *r = *right;
	if (is_int_type(l) && is_int_type(r) && !type_equals(l, r)) {
		if (TYPE_WIDTH(l->kind) > TYPE_WIDTH(r->kind)) {
			// Extract L's width and R's signedness, combine into new type for R
			type_t new_kind = TYPE_WIDTH(l->kind) | TYPE_SIGNEDNESS(r->kind);
			expr->right = build_cast(expr->right, new_kind);
			r->kind = new_kind;
			expr->right->is_unsigned = UNSIGNED(new_kind);
		} else {
			// and vice versa
			type_t new_kind = TYPE_WIDTH(r->kind) | TYPE_SIGNEDNESS(l->kind);
			expr->left = build_cast(expr->left, new_kind);
			l->kind = new_kind;
			expr->left->is_unsigned = UNSIGNED(new_kind);
		}
	}
}

// TODO: use goto error pattern
ast_type *derive_expr_type(ast_expr *expr)
{
	ast_typed_symbol *ts = NULL;
	ast_type *left;
	ast_type *right;
	ast_type *ret = NULL;
	if (expr == NULL)
		return NULL;
	switch (expr->kind) {
	case E_LOG_OR:
	case E_LOG_AND:
		left = derive_expr_type(expr->left);
		right = derive_expr_type(expr->right);
		if (left->kind == Y_BOOL && right->kind == Y_BOOL) {
			type_destroy(right);
			ret = left;
			goto derive_expr_done;
		}
		report_error_cur_line("Operands must both be booleans in logical and/or expressions.");
		type_destroy(left);
		type_destroy(right);
		ret = NULL;
		goto derive_expr_done;
	case E_ADDSUB:
	case E_MULDIV:
	case E_SHIFT:
	case E_BW_XOR:
	case E_BW_OR:
	case E_BW_AND:
		left = derive_expr_type(expr->left);
		right = derive_expr_type(expr->right);
		cast_up_if_necessary(expr, &left, &right);
		if (type_equals(left, right) && is_int_type(left)) {
			type_destroy(right);
			ret = left;
			goto derive_expr_done;
		}
		report_error_cur_line("Operands must both be integers in arithmetic expressions.");
		type_destroy(left);
		type_destroy(right);
		ret = NULL;
		goto derive_expr_done;
	case E_EQUALITY:
	case E_INEQUALITY:
		left = derive_expr_type(expr->left);
		right = derive_expr_type(expr->right);
		cast_up_if_necessary(expr, &left, &right);
		if (type_equals(left, right) && (is_int_type(left) || left->kind == Y_CHAR)) {
			type_destroy(left);
			type_destroy(right);
			ret = type_init(Y_BOOL, NULL);
			goto derive_expr_done;
		}
		report_error_cur_line("Operands must both be integer types in (in)equality expressions.");
		type_destroy(left);
		type_destroy(right);
		ret = NULL;
		goto derive_expr_done;
	case E_ASSIGN:
		ret = derive_assign(expr);
		goto derive_expr_done;
	case E_PAREN:
		ret = derive_expr_type(expr->left);
		goto derive_expr_done;
	case E_CHAR_LIT:
		ret = type_init(Y_CHAR, NULL);
		goto derive_expr_done;
	case E_TRUE_LIT:
	case E_FALSE_LIT:
		ret = type_init(Y_BOOL, NULL);
		goto derive_expr_done;
	case E_INT_LIT:
		ret = type_init(expr->int_size, NULL);
		goto derive_expr_done;
	case E_STR_LIT:
		left = type_init(Y_CONSTPTR, NULL);
		left->subtype = type_init(Y_CHAR, NULL);
		ret = left;
		goto derive_expr_done;
	case E_FNCALL:
		ret = typecheck_fncall(expr);
		goto derive_expr_done;
	case E_IDENTIFIER:
		ts = scope_lookup(expr->name);
		if (ts == NULL) {
			report_error_cur_line("Used undeclared identifier");
			ret = NULL;
			goto derive_expr_done;
		}
		if (ts->type->kind == Y_STRUCT && ts->type->name != NULL && strvec_equals(ts->type->name, expr->name)) {
			report_error_cur_line("Can't use struct type in this expression");
			ret = NULL;
			goto derive_expr_done;
		}
		expr->is_unsigned = UNSIGNED(ts->type->kind);
		ret = type_copy(ts->type);
		goto derive_expr_done;
	case E_PRE_UNARY:
		ret = derive_pre_unary(expr);
		goto derive_expr_done;
	case E_POST_UNARY:
		ret = derive_post_unary(expr);
		goto derive_expr_done;
	default:
		report_error_cur_line("unsupported expr kind while typechecking!");
		ret = NULL;
		goto derive_expr_done;
	}
derive_expr_done:
	expr->is_unsigned = expr->is_unsigned ||
				(expr->left != NULL && expr->left->is_unsigned)
				|| (expr->right != NULL && expr->right->is_unsigned);
	return ret;
}

void typecheck_asm(ast_stmt *stmt) {
	asm_struct *a = stmt->asm_obj;
	ast_type *t = NULL;
	// LCOV_EXCL_START
	// I can't imagine we'll get to this point but who knows?
	if (a == NULL) {
		report_error_cur_line("Invalid asm statement");
		return;
	}

	// everything in this LCOV_EXCL block below this line is already checked in parsing.
	// Subject to change.
	t = derive_expr_type(a->code);
	if (t == NULL || a->code->kind != E_STR_LIT) {
		report_error_cur_line("asm statement's first arg must be a string literal.");
		type_destroy(t);
		return;
	}
	type_destroy(t);

	t = derive_expr_type(a->constraints);
	if (t != NULL && a->constraints->kind != E_STR_LIT) {
		report_error_cur_line("asm statement's second arg must be a string.");
		type_destroy(t);
		return;
	}
	type_destroy(t);
	// LCOV_EXCL_STOP

	for (size_t i = 0 ; a->out_operands != NULL && i < a->out_operands->size ; ++i)
		if (((ast_expr *)vect_get(a->out_operands, i))->kind != E_IDENTIFIER)
			report_error_cur_line("inline asm output operands must be identifiers.");

	for (size_t i = 0 ; a->in_operands != NULL && i < a->in_operands->size ; ++i)
		type_destroy(derive_expr_type(vect_get(a->in_operands, i)));
}

void typecheck_stmt(ast_stmt *stmt, int at_fn_top_level)
{
	ast_type *typ;
	int old_in_loop = in_loop;
	if (stmt == NULL) {
		if (at_fn_top_level) {
			report_error_cur_line("Non-void functions must end in valid return statements");
			return;
		}
		return;
	}
	cur_line = stmt->line;
	if (is_return_worthy(stmt)) {
		if (stmt->next != NULL) {
			report_error_cur_line("Return-worthy statements must appear at the end of fn blocks."); // TODO: this is a bad error messsage
			return;
		}
		at_fn_top_level = 0;
	}
	switch (stmt->kind) {
	case S_ERROR:
		report_error_cur_line("WARNING typechecking a bad stmt. Big problem?!");
		typecheck_stmt(stmt->next, at_fn_top_level);
		break;
	case S_IFELSE:
		if (stmt->expr == NULL) {
			report_error_cur_line("if statement condition must be non-empty");
		}
		typ = derive_expr_type(stmt->expr);
		if (typ == NULL || typ->kind != Y_BOOL) {
			report_error_cur_line("if statement condition must be a boolean");
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
			report_error_cur_line("while statement condition must be non-empty");
		}
		typ = derive_expr_type(stmt->expr);
		if (typ == NULL || typ->kind != Y_BOOL) {
			report_error_cur_line("while statement condition must be a boolean");
		}
		type_destroy(typ);
		in_loop = 1;
		if (stmt->body != NULL) {
			typecheck_stmt(stmt->body->body, 0);
		} else {
			report_error_cur_line("Empty while loop body.");
		}
		in_loop = old_in_loop;
		typecheck_stmt(stmt->next, at_fn_top_level);
		break;
	case S_BREAK:
	case S_CONTINUE:
		if (!in_loop) {
			report_error_cur_line("Break/continue statement used outside of loop");
		}
		if (stmt->next != NULL) {
			report_error_cur_line("Break/continue statements must appear at the end of statement blocks");
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
		// typechecking returns is done when checking for return-worthyness.
		break;
	case S_ASM:
		typecheck_asm(stmt);
		typecheck_stmt(stmt->next, at_fn_top_level);
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
	// TODO: clean this up
	return type_equals(a->subtype, b->subtype) && arglist_equals(a->arglist, b->arglist) &&
			(a->kind == b->kind ||
			(is_int_type(a) && is_int_type(b) && TYPE_WIDTH(a->kind) == TYPE_WIDTH(b->kind)));
}
