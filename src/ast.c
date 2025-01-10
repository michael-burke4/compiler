#include "ast.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>

ast_decl *decl_init(ast_typed_symbol *typesym, ast_expr *expr, ast_stmt *stmt, ast_decl *next, size_t line)
{
	ast_decl *ret = smalloc(sizeof(*ret));
	ret->typesym = typesym;
	ret->body = stmt;
	ret->expr = expr;
	ret->next = next;
	ret->initializer = NULL;
	ret->line = line;
	return ret;
}

ast_type *type_init(type_t kind, strvec *name)
{
	ast_type *ret = smalloc(sizeof(*ret));
	ret->subtype = NULL;
	ret->owns_subtype = 1;
	ret->arglist = NULL;
	ret->kind = kind;
	ret->name = name;
	ret->modif = VM_DEFAULT;
	return ret;
}

ast_expr *expr_init(expr_t kind, ast_expr *left, ast_expr *right, token_t op, strvec *name,
			int64_t num, strvec *str_lit)
{
	ast_expr *ret = smalloc(sizeof(*ret));
	ret->kind = kind;
	ret->left = left;
	ret->right = right;
	ret->op = op;
	ret->name = name;
	ret->num = num;
	ret->sub_exprs = NULL;
	ret->is_lvalue = 0;
	ret->string_literal = str_lit;
	ret->type = NULL;
	ret->owns_type = false;
	return ret;
}

ast_typed_symbol *ast_typed_symbol_init(ast_type *type, strvec *symbol)
{
	ast_typed_symbol *ret = smalloc(sizeof(*ret));
	ret->type = type;
	ret->symbol = symbol;
	return ret;
}

ast_stmt *stmt_init(stmt_t kind, ast_decl *decl, ast_expr *expr, ast_stmt *body,
			ast_stmt *else_body, size_t line)
{
	ast_stmt *ret = smalloc(sizeof(*ret));
	ret->kind = kind;
	ret->decl = decl;
	ret->expr = expr;
	ret->body = body;
	ret->else_body = else_body;
	ret->next = NULL;
	ret->asm_obj = NULL;
	ret->line = line;
	ret->return_worthy = RETW_UNCHECKED;

	return ret;
}

static void asm_struct_destroy(asm_struct *a)
{
	if (a == NULL)
		return;
	expr_destroy(a->code);
	expr_destroy(a->constraints);
	for (size_t i = 0 ; a->in_operands != NULL && i < a->in_operands->size ; ++i) {
		expr_destroy(vect_get(a->in_operands, i));
	}
	for (size_t i = 0 ; a->out_operands != NULL && i < a->out_operands->size ; ++i) {
		expr_destroy(vect_get(a->out_operands, i));
	}
	vect_destroy(a->in_operands);
	vect_destroy(a->out_operands);
	free(a);
}

void stmt_destroy(ast_stmt *stmt)
{
	ast_stmt *next;
	if (stmt == NULL)
		return;
	next = stmt->next;
	asm_struct_destroy(stmt->asm_obj);
	decl_destroy(stmt->decl);
	expr_destroy(stmt->expr);
	stmt_destroy(stmt->body);
	stmt_destroy(stmt->else_body);
	free(stmt);
	stmt_destroy(next);
}

void type_destroy(ast_type *type)
{
	if (type == NULL)
		return;
	arglist_destroy(type->arglist);
	if (type->owns_subtype)
		type_destroy(type->subtype);
	strvec_destroy(type->name);
	free(type);
}

void destroy_expr_vect(vect *expr_vect)
{
	if (expr_vect == NULL)
		return;
	for (size_t i = 0 ; i < expr_vect->size ; ++i)
		expr_destroy((ast_expr *)expr_vect->elements[i]);
	vect_destroy(expr_vect);
}

void expr_destroy(ast_expr *expr)
{
	if (expr == NULL)
		return;
	expr_destroy(expr->right);
	expr_destroy(expr->left);
	strvec_destroy(expr->name);
	strvec_destroy(expr->string_literal);
	destroy_expr_vect(expr->sub_exprs);

	// See the note in the ast.h ast_expr struct definition for details.
	if (expr->owns_type)
		type_destroy(expr->type);

	free(expr);
}


void decl_destroy(ast_decl *decl)
{
	if (decl == NULL)
		return;
	if (decl->initializer != NULL) {
		for (size_t i = 0 ; i < decl->initializer->size ; ++i)
			expr_destroy(decl->initializer->elements[i]);
		free(decl->initializer->elements);
		free(decl->initializer);
	}
	ast_typed_symbol_destroy(decl->typesym);
	expr_destroy(decl->expr);
	stmt_destroy(decl->body);
	free(decl);
}

void ast_typed_symbol_destroy(ast_typed_symbol *typesym)
{
	if (typesym == NULL)
		return;
	type_destroy(typesym->type);
	strvec_destroy(typesym->symbol);
	free(typesym);
}

void ast_free(ast_decl *program)
{
	if (program == NULL)
		return;
	ast_decl *next = program->next;
	decl_destroy(program);
	ast_free(next);
}

ast_type *type_copy(ast_type *t)
{
	ast_type *ret;
	if (t == NULL)
		return NULL;
	ret = type_init(t->kind, strvec_copy(t->name));
	ret->subtype = type_copy(t->subtype);
	ret->arglist = arglist_copy(t->arglist);
	ret->modif = t->modif;
	return ret;
}

vect *arglist_copy(vect *arglist)
{
	vect *ret = NULL;
	if (arglist == NULL)
		return ret;
	ret = vect_init(arglist->size);
	for (size_t i = 0 ; i < arglist->size ; ++i) {
		ast_typed_symbol *cur = arglist_get(arglist, i);
		vect_append(ret, (void *)ast_typed_symbol_init(type_copy(cur->type), strvec_copy(cur->symbol)));
	}
	return ret;
}

void arglist_destroy(vect *arglist)
{
	if (arglist == NULL)
		return;
	for (size_t i = 0 ; i < arglist->size ; ++i)
		ast_typed_symbol_destroy(arglist_get(arglist, i));
	vect_destroy(arglist);
}

type_t smallest_fit(int64_t num)
{
	int64_t max_32 = 2147483647;
	int64_t min_32 = -2147483648;
	if (num <= max_32 && num >= min_32)
		return Y_I32;
	return Y_I64;
}

ast_stmt *last(ast_stmt *block)
{
	if (block == NULL)
		return NULL;
	for (; block->next != NULL ; block = block->next) {}
	return block;
}

bool is_integer(ast_type *t)
{
	if (t == NULL)
		return false;

	switch (t->kind) {
	case Y_U32:
	case Y_U64:
	case Y_I32:
	case Y_I64:
		return true;
	default:
		return false;
	}
}

char *decl_name(ast_decl *d)
{
	if (d == NULL)
		return NULL;
	return d->typesym->symbol->text;
}
