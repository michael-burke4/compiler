#include "ast.h"
#include "util.h"
#include <stdlib.h>
#include <stdio.h>

ast_decl *decl_init(ast_typed_symbol *typesym, ast_expr *expr, ast_stmt *stmt,
		    ast_decl *next)
{
	ast_decl *ret = smalloc(sizeof(*ret));
	ret->typesym = typesym;
	ret->body = stmt;
	ret->expr = expr;
	ret->next = next;
	return ret;
}

ast_type *type_init(token_t type, strvec *name)
{
	ast_type *ret = smalloc(sizeof(*ret));
	ret->subtype = 0;
	ret->arglist = 0;
	ret->type = type;
	ret->name = name;
	return ret;
}

ast_expr *expr_init(expr_t kind, ast_expr *left, ast_expr *right, token_t op,
		    strvec *name, int int_lit, strvec *str_lit)
{
	ast_expr *ret = smalloc(sizeof(*ret));
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
	ast_typed_symbol *ret = smalloc(sizeof(*ret));
	ret->type = type;
	ret->symbol = symbol;
	ret->next = 0;
	return ret;
}

ast_stmt *stmt_init(stmt_t kind, ast_decl *decl, ast_expr *expr, ast_stmt *body,
		    ast_stmt *else_body)
{
	ast_stmt *ret = smalloc(sizeof(*ret));
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

ast_typed_symbol *arglist_copy(ast_typed_symbol *arglist)
{
	//ast_typed_symbol *ret;
	if (!arglist)
		return 0;
	return 0; // TODO
}

ast_type *type_copy(ast_type *t)
{
	ast_type *ret;

	if (!t)
		return 0;
	ret = type_init(t->type, strvec_copy(t->name));
	ret->subtype = type_copy(t->subtype);
	ret->arglist = arglist_copy(t->arglist);
	return 0; // TODO
}
