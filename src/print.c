#include <stdio.h>
#include "print.h"
#include "token.h"

void print_op(ast_expr *expr)
{
	if (!expr)
		return;

	switch (expr->kind) {
	case E_ADDSUB:
	case E_MULDIV:
	case E_INEQUALITY:
	case E_EQUALITY:
	case E_ASSIGN:
		printf(" ");
		tok_t_print(expr->op);
		printf(" ");
		break;
	default:
		tok_t_print(expr->op);
	}
}

static void print_expr_list(vect *list) {
	size_t i;

	if (!list)
		return;
	for (i = 0 ; i < list->size - 1 ; ++i) {
		expr_print(list->elements[i]);
		printf(", ");
	}
	expr_print(list->elements[i]);
}

static void print_sub_exprs(ast_expr *expr)
{
	if (!expr || !expr->sub_exprs)
		return;
	print_expr_list(expr->sub_exprs);
}

void expr_print(ast_expr *expr)
{
	if (!expr)
		return;
	switch (expr->kind) {
	case E_POST_UNARY:
		if (expr->op == T_LBRACKET) {
			expr_print(expr->left);
			printf("[");
			expr_print(expr->right);
			printf("]");
		} else if (expr->op == T_PERIOD) {
			expr_print(expr->left);
			printf(".");
			expr_print(expr->right);
		} else {
			expr_print(expr->left);
			print_op(expr);
		}
		break;
	case E_PRE_UNARY:
		print_op(expr);
		expr_print(expr->left);
		break;
	case E_ASSIGN:
	case E_INEQUALITY:
	case E_EQUALITY:
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
		switch (smallest_fit(expr->num)) {
		case Y_I32:
			printf("%d", (int32_t)expr->num);
			break;
		default:
			printf("%ld", (int64_t)expr->num);
			break;
		}
		break;
	case E_CHAR_LIT:
		printf("'");
		strvec_print(expr->string_literal);
		printf("'");
		break;
	case E_STR_LIT:
		printf("\"");
		strvec_print(expr->string_literal);
		printf("\"");
		break;
	case E_IDENTIFIER:
		strvec_print(expr->name);
		break;
	case E_FNCALL:
		strvec_print(expr->name);
		printf("(");
		print_sub_exprs(expr);
		printf(")");
		break;
	case E_SYSCALL:
		printf("syscall(");
		print_sub_exprs(expr);
		printf(")");
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

void typed_sym_print(ast_typed_symbol *typesym)
{
	if (!typesym)
		return;
	strvec_print(typesym->symbol);
	printf(": ");
	type_print(typesym->type);
}

void type_print(ast_type *type)
{
	if (!type)
		return;
	vect *arglist = type->arglist;
	ssize_t i;
	switch (type->kind) {
	case Y_I32:
		printf("i32");
		break;
	case Y_U32:
		printf("u32");
		break;
	case Y_I64:
		printf("i64");
		break;
	case Y_U64:
		printf("u64");
		break;
	case Y_VOID:
		printf("void");
		break;
	case Y_BOOL:
		printf("bool");
		break;
	case Y_STRING:
		printf("string");
		break;
	case Y_CHAR:
		printf("char");
		break;
	case Y_FUNCTION:
		printf("(");
		for (i = 0 ; arglist && i < ((ssize_t)arglist->size) - 1 ; ++i) {
			typed_sym_print(arglist_get(arglist, i));
			printf(", ");
		}
		if (arglist && arglist->size != 0) {
			typed_sym_print(arglist_get(arglist, i));
		}
		printf(")");
		printf(" -> ");
		type_print(type->subtype);
		break;
	case Y_POINTER:
		type_print(type->subtype);
		printf("*");
		break;
	case Y_STRUCT:
		printf("struct");
		if (type->name != NULL) {
			printf(" ");
			strvec_print(type->name);
		}
		break;
	default:
		printf("UNSUPPORTED TYPE! (type %d)", type->kind);
	}
}

void decl_print(ast_decl *decl)
{
	// Struct definitions don't have type names in their typesyms:
	// the type of a struct definition is just `struct`
	// while it is `struct (struct_name_here)` in instantiation.
	if (decl->typesym->type->kind == Y_STRUCT && decl->typesym->type->name == NULL) {
		printf("struct ");
		strvec_print(decl->typesym->symbol);
		puts(" {");
		for (size_t i = 0 ; i < decl->typesym->type->arglist->size; ++i) {
			typed_sym_print(decl->typesym->type->arglist->elements[i]);
			puts(";");
		}
		puts("};");
		return;
	}
	printf("let ");
	typed_sym_print(decl->typesym);
	if (decl->expr != NULL) {
		printf(" = ");
		expr_print(decl->expr);
	} else if (decl->body != NULL) {
		printf(" = ");
		stmt_print(decl->body);
	} else if (decl->initializer != NULL) {
		printf(" = ");
		printf("[");
		print_expr_list(decl->initializer);
		printf("]");
	}
	printf(";");
}

void stmt_print(ast_stmt *stmt)
{
	if (!stmt)
		return;
	switch (stmt->kind) {
	case S_DECL:
		decl_print(stmt->decl);
		break;
	case S_BLOCK:
		stmt = stmt->body;
		printf("{\n");
		while (stmt != NULL) {
			stmt_print(stmt);
			stmt = stmt->next;
			printf("\n");
		}
		printf("}");
		break;
	case S_RETURN:
		printf("return");
		if (!stmt->expr) {
			printf(";");
			break;
		}
		printf(" ");
		// fall through
	case S_EXPR:
		expr_print(stmt->expr);
		printf(";");
		break;
	case S_IFELSE:
		printf("if (");
		expr_print(stmt->expr);
		printf(") ");
		stmt_print(stmt->body);
		if (stmt->else_body) {
			printf(" else ");
			stmt_print(stmt->else_body);
		}
		break;
	case S_WHILE:
		printf("while (");
		expr_print(stmt->expr);
		printf(") ");
		stmt_print(stmt->body);
		break;
	default:
		printf("Somehow reached error/unkown statement printing case?\n");
	}
}

void program_print(ast_decl *program)
{
	if (!program)
		return;
	decl_print(program);
	printf("\n");
	program_print(program->next);
}
