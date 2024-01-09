#include "ast.h"
#include <stdlib.h>
#include <stdio.h>

static void expr_print(ast_expr *expr);
static void stmt_print(ast_stmt *stmt);
static void type_print(ast_type *type);
static void decl_print(ast_decl *decl);
static void typed_sym_print(ast_typed_symbol *typesym);

ast_decl *decl_init(ast_typed_symbol *typesym, ast_expr *expr, ast_stmt *stmt, ast_decl *next)
{
	ast_decl *ret = malloc(sizeof(*ret));
	ret->typesym = typesym;
	ret->body = stmt;
	ret->expr = expr;
	ret->next = next;
	return ret;
}

ast_type *type_init(token_t type, strvec *name)
{
	ast_type *ret = malloc(sizeof(*ret));
	ret->subtype = 0;
	ret->arglist = 0;
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

ast_typed_symbol *ast_typed_symbol_init(ast_type *type, strvec *symbol)
{
	ast_typed_symbol *ret = malloc(sizeof(*ret));
	ret->type = type;
	ret->symbol = symbol;
	ret->next = 0;
	return ret;
}

ast_stmt *stmt_init(stmt_t kind, ast_decl *decl, ast_expr *expr, ast_stmt *body,
	ast_stmt *else_body)
{
	ast_stmt *ret = malloc(sizeof(*ret));
	ret->kind = kind;
	ret->decl = decl;
	ret->expr = expr;
	ret->body = body;
	ret->else_body = else_body;
	ret->next = 0;

	return ret;
}

void stmt_destroy(ast_stmt *stmt)
{
	ast_stmt *next;
	if (!stmt)
		return;
	next = stmt->next;
	decl_destroy(stmt->decl);
	expr_destroy(stmt->expr);
	stmt_destroy(stmt->body);
	stmt_destroy(stmt->else_body);
	free(stmt);
	stmt_destroy(next);
}

void type_destroy(ast_type *type)
{
	if (!type)
		return;
	ast_typed_symbol_destroy(type->arglist);
	type_destroy(type->subtype);
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
	ast_typed_symbol_destroy(decl->typesym);
	expr_destroy(decl->expr);
	stmt_destroy(decl->body);
	free(decl);
}

void ast_typed_symbol_destroy(ast_typed_symbol *typesym)
{
	if (!typesym)
		return;
	ast_typed_symbol_destroy(typesym->next);
	type_destroy(typesym->type);
	strvec_destroy(typesym->symbol);
	free(typesym);
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
static void expr_print(ast_expr *expr)
{
	if (!expr)
		return;
	switch (expr->kind) {
	case E_POST_UNARY:
		expr_print(expr->left);
		print_op(expr);
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

static void typed_sym_print(ast_typed_symbol *typesym)
{
	strvec_print(typesym->symbol);
	printf(": ");
	type_print(typesym->type);
}

static void type_print(ast_type *type)
{
	ast_typed_symbol *printhead = type->arglist;
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
	case T_ARROW:
		printf("(");
		while (printhead != 0) {
			typed_sym_print(printhead);
			printhead = printhead->next;
			if (printhead == 0)
				break;
			else
				printf(", ");
		}
		printf(")");
		printf(" -> ");
		type_print(type->subtype);
		break;
	case T_IDENTIFIER:
		strvec_print(type->name);
		break;
	default:
		printf("UNSUPPORTED TYPE! (type %d)", type->type);
	}
}

static void decl_print(ast_decl *decl)
{
	printf("let ");
	typed_sym_print(decl->typesym);
	if (decl->expr != 0) {
		printf(" = ");
		expr_print(decl->expr);
	} else if (decl->body != 0) {
		printf(" = ");
		stmt_print(decl->body);
	}
	printf(";");
}

//typedef enum {
//	S_ERROR,
//	S_BODY,
//	S_DECL,
//	S_EXPR,
//	S_IFELSE,
//	S_RETURN
//} stmt_t;
static void stmt_print(ast_stmt *stmt)
{
	if (!stmt)
		return;
	switch (stmt->kind) {
	case S_DECL:
		decl_print(stmt->decl);
		break;
	case S_BLOCK:
		stmt = stmt->next;
		printf("{\n");
		while (stmt != 0) {
			stmt_print(stmt);
			stmt = stmt->next;
			printf("\n");
		}
		printf("}");
		break;
	case S_RETURN:
		printf("return ");
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
