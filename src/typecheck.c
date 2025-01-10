#include "error.h"
#include "parse.h"
#include "print.h"
#include "symbol_table.h"
#include "typecheck.h"
#include "util.h"

#include <stdarg.h>
#include <stdio.h>


static int in_loop = 0;
static int cur_line = 0;

static void report_error_cur_line(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vreport_error_line(cur_line, fmt, args);
	va_end(args);
}

static void got_but_expected(ast_type *got, ast_type *expected) {
	fprintf(stderr, " (got ");
	e_type_print(got);
	fprintf(stderr, ", expected ");
	e_type_print(expected);
	fprintf(stderr, ")\n");
}

static void cant_with_expr(const char *msg, ast_expr *e) {
	report_error_cur_line(msg);
	fprintf(stderr, " '");
	e_expr_print(e);
	fprintf(stderr, "'\n");
}

static void l_r_mismatch(const char *msg, ast_expr *l, ast_expr *r) {
	report_error_cur_line(msg);
	fprintf(stderr, " [operands: '");
	e_expr_print(l);
	fprintf(stderr, "' (");
	if (l->type != NULL)
		e_type_print(l->type);
	else
		fprintf(stderr, "couldn't be derived");
	fprintf(stderr, ") and ");
	e_expr_print(r);
	fprintf(stderr, " (");
	if (r->type != NULL)
		e_type_print(r->type);
	else
		fprintf(stderr, "couldn't be derived");
	fprintf(stderr, ")]\n");
}

void typecheck_program(ast_decl *program)
{
	ast_decl *cur = program;
	while (cur != NULL) {
		typecheck_decl(cur, 1);
		cur = cur->next;
	}
}

static void scope_bind_args(ast_decl *decl)
{
	size_t i;
	vect *arglist;
	if (decl == NULL) {
		report_error_cur_line("NULL FN IN BINDING ARGS?!\n");
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
		report_error_cur_line("Return statements must be at the end of statement blocks.\n");
	}
	typ = scope_get_return_type();
	if (typ == NULL) {
		report_error_cur_line("Cannot use a return statement outside of a function\n");
	} else if (stmt->expr == NULL) {
		if (typ->kind != Y_VOID) {
			report_error_cur_line("Non-void function has empty return statement.\n");
		}
		return;
	}
	derive_expr_type(stmt->expr);
	if (!type_equals(stmt->expr->type, typ, 0)) {
		report_error_cur_line("Type of return expression '");
		e_expr_print(stmt->expr);
		fprintf(stderr, "' does not match the expected return type (");
		e_type_print(typ);
		fprintf(stderr, ")\n");
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
//	stmt must be non-null.
//	stmt->next must be null.
static retw_t is_return_worthy(ast_stmt *stmt) {
	if (stmt->return_worthy != RETW_UNCHECKED)
		return stmt->return_worthy;
	if (stmt->kind == S_RETURN) {
		typecheck_return(stmt);
		// typecheck_return can still set had_error and print an error message:
		// what matters to is_return_worthy is that this statement is return-worthy!
		return (stmt->return_worthy = RETW_TRUE);
	}
	if (stmt->kind == S_IFELSE) {
		if (stmt->else_body == NULL) {
			return (stmt->return_worthy = RETW_FALSE);
		}
		if (is_return_worthy(last(stmt->body->body)) == RETW_TRUE
				&& is_return_worthy(last(stmt->else_body->body)) == RETW_TRUE) {
			return (stmt->return_worthy = RETW_TRUE);
		}
	}
	return (stmt->return_worthy = RETW_FALSE);
}

static void append_retvoid_if_needed(ast_decl *fn) {
	if (fn == NULL || fn->typesym->type->subtype->kind != Y_VOID || fn->body == NULL || fn->body->body == NULL)
		return;
	ast_stmt *s = last(fn->body->body);
	if (is_return_worthy(s) == RETW_FALSE)
		s->next = stmt_init(S_RETURN, 0, 0, 0, 0, fn->line);
}

static void typecheck_fnbody(ast_decl *decl)
{
	if (decl->body == NULL) {
		report_error_cur_line("Must provide a function body to non-prototype declaration of '%s'\n", decl_name(decl));
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
	if (decl->initializer == NULL)
		return;
	for (size_t i = 0 ; i < decl->initializer->size ; ++i) {
		derive_expr_type(decl->initializer->elements[i]);
		if (!type_equals(((ast_expr *)(decl->initializer->elements[i]))->type, decl->typesym->type->subtype, 0)) {
			report_error_cur_line("Type mismatch at position %lu in array '%s' initializer\n", i, decl_name(decl));
			return;
		}
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
// TODO: think of a better name.
static int right_can_cast_implicitly(ast_type *left, ast_type *right)
{
	if (left == NULL || right == NULL)
		return 0;
	if (is_int_type(left) && is_int_type(right) &&
			TYPE_WIDTH(left->kind) > TYPE_WIDTH(right->kind))
		return 1;
	if (left->kind == Y_POINTER && right->kind == Y_POINTER
			&& right->subtype->kind == Y_VOID)
		return 1;
	return 0;
}

static void typecheck_global_decl(ast_decl *decl)
{
	if (decl->typesym->type->arglist != NULL) {
		return;
	}
	if (decl->expr == NULL) {
		report_error_cur_line("Global declaration of '%s' missing initializer with compile-time-constant expression\n", decl_name(decl));
		return;
	}

	derive_expr_type(decl->expr);
	switch (decl->typesym->type->kind) {
	case Y_CHAR:
		if (decl->expr->kind != E_CHAR_LIT) {
			goto global_wrong_type;
		}
		return;
	case Y_BOOL:
		if (decl->expr->kind != E_TRUE_LIT && decl->expr->kind != E_FALSE_LIT)
			goto global_wrong_type;
		return;
	case Y_POINTER:
	case Y_CONSTPTR:
		// TODO: allow array ininitializers at global level
		if (decl->expr->kind != E_NULL)
			goto global_wrong_type;
		return;
	case Y_U32:
	case Y_U64:
	case Y_I32:
	case Y_I64:
		if (decl->expr->kind != E_INT_LIT)
			goto global_wrong_type;
		return;
	case Y_FUNCTION:
	case Y_STRUCT:
	case Y_VOID:
		report_error_cur_line("Could not declare a declaration of type ");
		e_type_print(decl->typesym->type);
		fprintf(stderr, " at the global level.\n");
		return;
	}

global_wrong_type:
	report_error_cur_line("Global declaration of '%s' initialized with mismatched value.", decl_name(decl));
	got_but_expected(decl->expr->type, decl->typesym->type);
}

static int valid_type_for_decl(ast_type *tp, int pointing) {
	if (tp == NULL)
		return 0;

	switch (tp->kind) {
	case Y_CHAR:
	case Y_BOOL:
	case Y_U32:
	case Y_U64:
	case Y_I32:
	case Y_I64:
		return 1;

	case Y_STRUCT:
		if (tp->name == NULL) {
			return !pointing;
		}
		ast_typed_symbol *found = scope_lookup(tp->name);
		return found != NULL;

	case Y_POINTER:
	case Y_CONSTPTR:
		return valid_type_for_decl(tp->subtype, 1);

	case Y_VOID:
		return pointing;
	case Y_FUNCTION:
		return !pointing;
	// TODO: function pointers?
	// one thing is that the way function types are written is
	// (arg1: type1, arg2: type2, ..., argn : typen) -> return_type
	//
	// if we want function pointers, writing the following...
	// (arg1: type1, arg2: type2, ..., argn : typen) -> return_type*
	// just says to return a `return_type` pointer, not that we're specifying a
	// function pointer.
	}
	return 0;
}

void typecheck_decl(ast_decl *decl, int at_global_level)
{
	ast_typed_symbol *ts = NULL;

	if (decl == NULL) {
		report_error_cur_line("Typechecking empty decl!?!?!\n");
		return;
	}
	cur_line = decl->line;
	if ((ts = scope_lookup_current(decl->typesym->symbol))) {
		if (ts->type->modif != VM_PROTO) {
			report_error_cur_line("Duplicate declaration of symbol '%s'\n", decl_name(decl));
			return;
		}
		if (decl->typesym->type->modif != VM_DEFAULT) {
			report_error_cur_line("Definitions of already-prototyped function '%s' must use the 'let' keyword\n", decl_name(decl));
			return;
		}
		if (!type_equals(ts->type, decl->typesym->type, 1)) {
			report_error_cur_line("Definition of function '%s' does not match type signature of function prototype", decl_name(decl));
			got_but_expected(decl->typesym->type, ts->type);
			return;
		}
		// TODO: might want to put this somewhere else? This still works though.
		ts->type->modif = VM_PROTO_DEFINED;
	}
	if (decl->typesym->type->modif == VM_PROTO) {
		if (decl->typesym->type->kind != Y_FUNCTION) {
			report_error_cur_line("Only functions can be prototypes.\n");
			return;
		}
		if (decl->expr != NULL || decl->body != NULL) {
			report_error_cur_line("Function prototype '%s' must not be assigned a body/value.\n", decl_name(decl));
			return;
		}
		scope_bind_ts(decl->typesym);
		return;
	}

	if (!valid_type_for_decl(decl->typesym->type, 0)) {
		report_error_cur_line("Can not create declare '%s' with type ", decl_name(decl));
		e_type_print(decl->typesym->type);
		fprintf(stderr, "\n");
		return;
	}

	if (ts == NULL)
		scope_bind_ts(decl->typesym);

	if (decl->typesym->type->kind == Y_FUNCTION) {
		typecheck_fnbody(decl);
	} else if (decl->initializer) {
		if (at_global_level) {
			report_error_cur_line("Can't use array initializers at global level yet. Sorry :(\n");
		}
		if (decl->typesym->type->kind != Y_POINTER && decl->typesym->type->kind != Y_CONSTPTR) {
			report_error_cur_line("%s is being declared with non-pointer type ", decl_name(decl));
			e_type_print(decl->typesym->type);
			fprintf(stderr, ". Only pointers can be assigned array initializers.\n");
			return;
		}
		typecheck_array_initializer(decl);
	} else if (at_global_level) {
		typecheck_global_decl(decl);
		return;
	} else if (decl->expr) {
		if (decl->typesym->type->kind == Y_POINTER && decl->typesym->type->subtype->kind == Y_CHAR
				&& decl->expr->kind == E_STR_LIT) {
			ast_expr *e;
			decl->initializer = vect_init(decl->expr->string_literal->size);
			for (size_t i = 0 ; i < decl->initializer->capacity ; ++i) {
				strvec *c = strvec_init(1);
				strvec_append(c, decl->expr->string_literal->text[i]);
				// TODO: replace 0s with NULL
				e = expr_init(E_CHAR_LIT, 0, 0, 0, 0, 0, c);
				e->type = type_init(Y_CHAR, NULL);
				e->owns_type = 1;
				vect_append(decl->initializer, e);
			}
			return;
		}
		derive_expr_type(decl->expr);
		if (right_can_cast_implicitly(decl->typesym->type, decl->expr->type)) {
			decl->expr = build_cast(decl->expr, decl->typesym->type->kind);
		} else if (!(type_equals(decl->typesym->type, decl->expr->type, 0))) {
			report_error_cur_line("Tried to assign an expression type ");
			e_type_print(decl->expr->type);
			fprintf(stderr, " to a ");
			e_type_print(decl->typesym->type);
			fprintf(stderr, "\n");
			return;
		}
	}
}


/*
Assumes that expr is a function call.

In order for this to attach a type the function must find that:

	(a) a function of the same name exists in the symbol table
	(b) the provided function's list of parameters must match with the aforementioned
		symbol table function's arg list.
*/
static void typecheck_fncall(ast_expr *expr)
{
	ast_typed_symbol *fn_ts = scope_lookup(expr->name);
	if (fn_ts == NULL) {
		report_error_cur_line("Call to undeclared function '%s'\n", expr->name->text);
		return;
	} else if (fn_ts->type->kind != Y_FUNCTION) {
		report_error_cur_line("Identifier '%s' does not refer to a function\n", expr->name->text);
		return;
	}
	vect *decl_arglist = fn_ts->type->arglist;
	vect *expr_arglist = expr->sub_exprs;
	size_t i;
	if (decl_arglist == NULL && expr_arglist == NULL) {
		expr->type = fn_ts->type->subtype;
		return;
	} else if ((decl_arglist == NULL || expr_arglist == NULL) ||
			(decl_arglist->size != expr_arglist->size)) {
		report_error_cur_line("Argument count mismatch in call to %s: expected %lu, %lu\n", expr->name->text,
				decl_arglist == NULL ? 0 : decl_arglist->size,
				expr_arglist == NULL ? 0 : expr_arglist->size);
		return;
	}
	for (i = 0 ; i < decl_arglist->size ; ++i) {
		ast_type *t = arglist_get(decl_arglist, i)->type;
		ast_expr *e = expr_arglist->elements[i];
		derive_expr_type(e);
		if (right_can_cast_implicitly(t, e->type)) {
			expr_arglist->elements[i] = build_cast(e, t->kind);
		} else if (!type_equals(t, e->type, 0)) {
			report_error_cur_line("Positional argument type mismatch at position %lu in call to '%s'",
					i, expr->name->text);
			got_but_expected(e->type, t);
		}
	}
	expr->type = fn_ts->type->subtype;
}

static void derive_assign(ast_expr *expr) {
	if (!expr->left->is_lvalue) {
		cant_with_expr("Cannot assign to non-lvalue expression", expr->left);
		return;
	}
	derive_expr_type(expr->left);
	if (expr->left->type != NULL && expr->left->type->modif != VM_DEFAULT) {
		cant_with_expr("Cannot assign to const/proto value", expr->left);
	}
	derive_expr_type(expr->right);
	if (type_equals(expr->left->type, expr->right->type, 0)) {
		expr->type = expr->left->type;
		return;
	}
	if (right_can_cast_implicitly(expr->left->type, expr->right->type)) {
		expr->right = build_cast(expr->right, expr->left->type->kind);
		expr->type = expr->left->type;
		return;
	}
	report_error_cur_line("Mismatched types in assigment statement");
	got_but_expected(expr->right->type, expr->left->type);
}

static void derive_pre_unary(ast_expr *expr)
{
	derive_expr_type(expr->left);
	switch (expr->op) {
	case T_AMPERSAND:
		if (expr->left->type == NULL || !expr->left->is_lvalue) {
			cant_with_expr("Cannot find address of non-lvalue expr", expr->left);
			return;
		}
		expr->type = type_init(expr->left->type->modif == VM_CONST ? Y_CONSTPTR : Y_POINTER, NULL);
		expr->owns_type = true;
		expr->type->subtype = expr->left->type;
		expr->type->owns_subtype = false;
		break;
	case T_STAR:
		if (expr->left->type == NULL || (expr->left->type->kind != Y_POINTER && expr->left->type->kind != Y_CONSTPTR)) {
			cant_with_expr("Cannot dereference non-lvalue expr", expr->left);
			return;
		}
		expr->type = expr->left->type->subtype;
		break;
	case T_MINUS:
		if (!is_int_type(expr->left->type)) {
			cant_with_expr("Cannot use unary negative operator on non-integer expression", expr->left);
			return;
		}
		expr->type = expr->left->type;
		break;
	case T_BW_NOT:
		if (!is_int_type(expr->left->type)) {
			cant_with_expr("Cannot use unary bitwise not operator on non-integer expression", expr->left);
			return;
		}
		expr->type = expr->left->type;
		break;
	case T_NOT:
		if (expr->left->type->kind != Y_BOOL) {
			cant_with_expr("Cannot use unary not operator on non-bolean expression", expr->left);
			return;
		}
		expr->type = expr->left->type;
		break;
	case T_SIZEOF:
		if (expr->left->type == NULL) {
			cant_with_expr("Could not determine type of", expr->left);
			return;
		}
		expr->type = type_init(Y_U64, NULL);
		expr->owns_type = true;
		break;
	default:
		report_error_cur_line("unsupported expr kind while typechecking!\n");
	}
}

static ast_type *struct_field_type(ast_typed_symbol *struct_ts, strvec *name) {
	vect *field_list = struct_ts->type->arglist;
	ast_typed_symbol *cur;
	for (size_t i = 0 ; i < field_list->size ; ++i) {
		cur = arglist_get(field_list, i);
		if (strvec_equals(name, cur->symbol))
			return cur->type;
	}
	return NULL;
}
static void derive_post_unary(ast_expr *expr)
{
	ast_typed_symbol *ts;
	derive_expr_type(expr->left);
	switch (expr->op) {
	case T_LBRACKET:
		if (expr->left->type == NULL || (expr->left->type->kind != Y_POINTER &&
					expr->left->type->kind != Y_CONSTPTR)) {
			cant_with_expr("Cannot use index operator on non-pointer expression", expr->left);
			return;
		}
		derive_expr_type(expr->right);
		// TODO: make this unsigned. USIZE?
		if (expr->right->type == NULL || expr->right->type->kind != Y_I32) {
			cant_with_expr("Cannot index with non-integer expression", expr->right);
			return;
		}
		expr->type = expr->left->type->subtype;
		return;
	case T_PERIOD:
		// TODO: think long and hard about if this if statement needs to check for lvalue-ness.
		if (expr->left->type == NULL || expr->left->type->kind != Y_STRUCT) {
			cant_with_expr("Cannot use member operator on non-struct expression", expr->left);
			return;
		}
		if (expr->right->kind != E_IDENTIFIER) {
			cant_with_expr("Cannot use non-identifier as struct member", expr->left);
			return;
		}
		ts = scope_lookup(expr->left->type->name);
		expr->type = struct_field_type(ts, expr->right->name);
		if (expr->type == NULL) {
			report_error_cur_line("No such member '%s' of struct '%s'", expr->right->name->text, expr->left->type->name->text);
		}
		return;
	default:
		report_error_cur_line("unsupported post unary expr while typechecking\n");
	}
}


static void cast_up_if_necessary(ast_expr *expr)
{
	ast_expr *l = expr->left;
	ast_expr *r = expr->right;
	if (is_int_type(l->type) && is_int_type(r->type) &&
			!type_equals(expr->left->type, expr->right->type, 0)) {
		if (TYPE_WIDTH(l->type->kind) > TYPE_WIDTH(r->type->kind)) {
			type_t new_kind = TYPE_WIDTH(l->type->kind) | TYPE_SIGNEDNESS(r->type->kind);
			r = build_cast(r, new_kind);
			expr->right = r;
		} else {
			type_t new_kind = TYPE_WIDTH(r->type->kind) | TYPE_SIGNEDNESS(l->type->kind);
			l = build_cast(l, new_kind);
			expr->left = l;
		}
	}
}

void derive_expr_type(ast_expr *expr)
{
	ast_typed_symbol *ts = NULL;

	if (expr == NULL)
		return;

	switch (expr->kind) {
	case E_LOG_OR:
	case E_LOG_AND:
		derive_expr_type(expr->left);
		derive_expr_type(expr->right);
		if (expr->left->type->kind == Y_BOOL && expr->right->type->kind == Y_BOOL) {
			expr->type = expr->left->type;
			return;
		}
		l_r_mismatch("Operands must both be booleans in logical and/or expressions", expr->left, expr->right);
		return;
	case E_ADDSUB:
	case E_MULDIV:
	case E_SHIFT:
	case E_BW_XOR:
	case E_BW_OR:
	case E_BW_AND:
		derive_expr_type(expr->left);
		derive_expr_type(expr->right);
		cast_up_if_necessary(expr);
		if (type_equals(expr->left->type, expr->right->type, 0) && is_int_type(expr->left->type)) {
			expr->type = expr->left->type;
			return;
		}
		l_r_mismatch("Operands must both be integers in arithmetic expressions", expr->left, expr->right);
		return;
	case E_EQUALITY:
	case E_INEQUALITY:
		derive_expr_type(expr->left);
		derive_expr_type(expr->right);
		cast_up_if_necessary(expr);
		if ((is_int_type(expr->left->type) && is_int_type(expr->right->type)) ||
					(expr->left->type != NULL && expr->right->type != NULL &&
					expr->left->type->kind == Y_CHAR
					&& expr->right->type->kind == Y_CHAR)) {
			expr->type = type_init(Y_BOOL, NULL);
			expr->owns_type = true;
			return;
		}
		l_r_mismatch("Operands must both be integer types in (in)equality expressions", expr->left, expr->right);
		return;
	case E_ASSIGN:
		derive_assign(expr);
		return;
	case E_PAREN:
		derive_expr_type(expr->left);
		expr->type = expr->left->type;
		return;
	case E_CHAR_LIT:
		expr->type = type_init(Y_CHAR, NULL);
		expr->owns_type = true;
		return;
	case E_TRUE_LIT:
	case E_FALSE_LIT:
		expr->type = type_init(Y_BOOL, NULL);
		expr->owns_type = true;
		return;
	case E_NULL:
		expr->type = type_init(Y_POINTER, NULL);
		expr->type->subtype = type_init(Y_VOID, NULL);
		expr->owns_type = true;
		return;
	case E_INT_LIT:
		// Deriving this type is performed in parsing.
		return;
	case E_STR_LIT:
		expr->type = type_init(Y_CONSTPTR, NULL);
		expr->type->subtype = type_init(Y_CHAR, NULL);
		expr->owns_type = true;
		return;
	case E_FNCALL:
		typecheck_fncall(expr);
		return;
	case E_IDENTIFIER:
		ts = scope_lookup(expr->name);
		if (ts == NULL) {
			report_error_cur_line("Used undeclared identifier '%s'\n", expr->name->text);
			return;
		}
		if (ts->type->kind == Y_STRUCT && ts->type->name != NULL && strvec_equals(ts->type->name, expr->name)) {
			report_error_cur_line("Can't use struct type '%s' in this expression\n", ts->type->name);
			return;
		}
		expr->type = ts->type;
		return;
	case E_CAST:
		if (expr->type->kind == Y_STRUCT) {
			report_error_cur_line("Casting directly between struct types is not supported.\n");
			return;
		}
		derive_expr_type(expr->left);
		// expr->type is already set during parsing.
		return;
	case E_PRE_UNARY:
		derive_pre_unary(expr);
		return;
	case E_POST_UNARY:
		derive_post_unary(expr);
		return;
	default:
		report_error_cur_line("unsupported expr kind while typechecking!\n");
		return;
	}
}

void typecheck_asm(ast_stmt *stmt) {
	asm_struct *a = stmt->asm_obj;
	// LCOV_EXCL_START
	// I can't imagine we'll get to this point but who knows?
	if (a == NULL) {
		report_error_cur_line("Invalid asm statement\n");
		return;
	}
	// LCOV_EXCL_STOP

	// everything in this LCOV_EXCL block below this line is already checked in parsing.
	// Subject to change.
	derive_expr_type(a->code);
	if (a->code == NULL || a->code->type == NULL || a->code->kind != E_STR_LIT) {
		report_error_cur_line("asm statement's first arg must be a string literal.\n");
		return;
	}
	derive_expr_type(a->constraints);
	if (a->constraints != NULL &&
			a->constraints->type != NULL &&
			a->constraints->kind != E_STR_LIT) {
		report_error_cur_line("asm statement's second arg must be a string.\n");
		return;
	}

	for (size_t i = 0 ; a->out_operands != NULL && i < a->out_operands->size ; ++i) {
		if (((ast_expr *)vect_get(a->out_operands, i))->kind != E_IDENTIFIER) {
			report_error_cur_line("inline asm output operands must be identifiers.\n");
		}
		derive_expr_type(vect_get(a->out_operands, i));
	}

	// TODO: double check these exprs all get freed and their types get freed by stmt_free.
	for (size_t i = 0 ; a->in_operands != NULL && i < a->in_operands->size ; ++i)
		derive_expr_type(vect_get(a->in_operands, i));
}

void typecheck_stmt(ast_stmt *stmt, int at_fn_top_level)
{
	int old_in_loop = in_loop;
	if (stmt == NULL) {
		if (at_fn_top_level) {
			report_error_cur_line("Non-void functions must end in valid return statements\n");
			return;
		}
		return;
	}
	cur_line = stmt->line;
	if (is_return_worthy(stmt) == RETW_TRUE) {
		if (stmt->next != NULL) {
			report_error_cur_line("Return-worthy statements must appear at the end of fn blocks.\n"); // TODO: this is a bad error messsage
			return;
		}
		at_fn_top_level = 0;
	}
	switch (stmt->kind) {
	case S_ERROR:
		report_error_cur_line("WARNING typechecking a bad stmt. Big problem?!\n");
		typecheck_stmt(stmt->next, at_fn_top_level);
		break;
	case S_IFELSE:
		if (stmt->expr == NULL) {
			report_error_cur_line("if statement condition must be non-empty\n");
		}
		derive_expr_type(stmt->expr);
		if (stmt->expr->type == NULL || stmt->expr->type->kind != Y_BOOL)
			cant_with_expr("Could not use non-boolean if statement condition", stmt->expr);
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
			report_error_cur_line("while statement condition must be non-empty\n");
		}
		derive_expr_type(stmt->expr);
		if (stmt->expr->type == NULL || stmt->expr->type->kind != Y_BOOL)
			cant_with_expr("Could not use non-boolean while statement condition", stmt->expr);
		in_loop = 1;
		if (stmt->body != NULL) {
			typecheck_stmt(stmt->body->body, 0);
		} else {
			report_error_cur_line("Empty while loop body.\n");
		}
		in_loop = old_in_loop;
		typecheck_stmt(stmt->next, at_fn_top_level);
		break;
	case S_BREAK:
	case S_CONTINUE:
		if (!in_loop) {
			report_error_cur_line("Break/continue statement used outside of loop\n");
		}
		if (stmt->next != NULL) {
			report_error_cur_line("Break/continue statements must appear at the end of statement blocks\n");
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
		typecheck_decl(stmt->decl, 0);
		typecheck_stmt(stmt->next, at_fn_top_level);
		break;
	case S_EXPR:
		derive_expr_type(stmt->expr);
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
		if (!type_equals(at->type, bt->type, 1))
			return 0;
	}
	return 1;
}

int type_equals(ast_type *a, ast_type *b, int int_type_strict)
{
	if (a == NULL && b == NULL)
		return 1;
	if (a == NULL || b == NULL)
		return 0;
	// TODO: clean this up
	return type_equals(a->subtype, b->subtype, int_type_strict) && arglist_equals(a->arglist, b->arglist) &&
			(a->kind == b->kind ||
			(!int_type_strict && (is_int_type(a) && is_int_type(b) && TYPE_WIDTH(a->kind) == TYPE_WIDTH(b->kind))));
}
