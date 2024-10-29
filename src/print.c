#include "print.h"
#include "token.h"

#include <stdio.h>

static void fprint_op(FILE *f, ast_expr *expr)
{
	if (!expr)
		return;

	switch (expr->kind) {
	case E_ADDSUB:
	case E_MULDIV:
	case E_INEQUALITY:
	case E_EQUALITY:
	case E_ASSIGN:
		fprintf(f, " ");
		fprint_tok_t(f, expr->op);
		fprintf(f, " ");
		break;
	default:
		fprint_tok_t(f, expr->op);
	}
}

static void fprint_expr_list(FILE *f, vect *list) {
	size_t i;

	if (!list)
		return;
	for (i = 0 ; i < list->size - 1 ; ++i) {
		fexpr_print(f, list->elements[i]);
		fprintf(f, ", ");
	}
	fexpr_print(f, list->elements[i]);
}

void fprint_sub_exprs(FILE *f, ast_expr *expr)
{
	if (!expr || !expr->sub_exprs)
		return;
	fprint_expr_list(f, expr->sub_exprs);
}

void fexpr_print(FILE *f, ast_expr *expr)
{
	if (!expr)
		return;
	switch (expr->kind) {
	case E_POST_UNARY:
		if (expr->op == T_LBRACKET) {
			fexpr_print(f, expr->left);
			fprintf(f, "[");
			fexpr_print(f, expr->right);
			fprintf(f, "]");
		} else if (expr->op == T_PERIOD) {
			fexpr_print(f, expr->left);
			fprintf(f, ".");
			fexpr_print(f, expr->right);
		} else {
			fexpr_print(f, expr->left);
			fprint_op(f, expr);
		}
		break;
	case E_PRE_UNARY:
		fprint_op(f, expr);
		fexpr_print(f, expr->left);
		break;
	case E_ASSIGN:
	case E_INEQUALITY:
	case E_EQUALITY:
	case E_MULDIV:
	case E_ADDSUB:
		fexpr_print(f, expr->left);
		fprint_op(f, expr);
		fexpr_print(f, expr->right);
		break;
	case E_PAREN:
		fprintf(f, "(");
		fexpr_print(f, expr->left);
		fprintf(f, ")");
		break;
	case E_INT_LIT:
		switch (smallest_fit(expr->num)) {
		case Y_I32:
			fprintf(f, "%d", (int32_t)expr->num);
			break;
		default:
			fprintf(f, "%ld", (int64_t)expr->num);
			break;
		}
		break;
	case E_CHAR_LIT:
		fprintf(f, "'");
		fstrvec_print(f, expr->string_literal);
		fprintf(f, "'");
		break;
	case E_STR_LIT:
		fprintf(f, "\"");
		fstrvec_print(f, expr->string_literal);
		fprintf(f, "\"");
		break;
	case E_IDENTIFIER:
		fstrvec_print(f, expr->name);
		break;
	case E_FNCALL:
		fstrvec_print(f, expr->name);
		fprintf(f, "(");
		fprint_sub_exprs(f, expr);
		fprintf(f, ")");
		break;
	case E_SYSCALL:
		fprintf(f, "syscall(");
		fprint_sub_exprs(f, expr);
		fprintf(f, ")");
		break;
	case E_FALSE_LIT:
		fprintf(f, "false");
		break;
	case E_TRUE_LIT:
		fprintf(f, "true");
		break;
	default:
		fprintf(f, "(unsupported expr)");
	}
}

void ftyped_sym_print(FILE *f, ast_typed_symbol *typesym)
{
	if (!typesym)
		return;
	fstrvec_print(f, typesym->symbol);
	fprintf(f, ": ");
	ftype_print(f, typesym->type);
}

void ftype_print(FILE *f, ast_type *type)
{
	if (!type)
		return;
	vect *arglist = type->arglist;
	ssize_t i;
	switch (type->kind) {
	case Y_I32:
		fprintf(f, "i32");
		break;
	case Y_U32:
		fprintf(f, "u32");
		break;
	case Y_I64:
		fprintf(f, "i64");
		break;
	case Y_U64:
		fprintf(f, "u64");
		break;
	case Y_VOID:
		fprintf(f, "void");
		break;
	case Y_BOOL:
		fprintf(f, "bool");
		break;
	case Y_CHAR:
		fprintf(f, "char");
		break;
	case Y_FUNCTION:
		fprintf(f, "(");
		for (i = 0 ; arglist && i < ((ssize_t)arglist->size) - 1 ; ++i) {
			ftyped_sym_print(f, arglist_get(arglist, i));
			fprintf(f, ", ");
		}
		if (arglist && arglist->size != 0) {
			ftyped_sym_print(f, arglist_get(arglist, i));
		}
		fprintf(f, ")");
		fprintf(f, " -> ");
		ftype_print(f, type->subtype);
		break;
	case Y_POINTER:
		ftype_print(f, type->subtype);
		fprintf(f, "*");
		break;
	case Y_CONSTPTR:
		ftype_print(f, type->subtype);
		fprintf(f, "@");
		break;
	case Y_STRUCT:
		fprintf(f, "struct");
		if (type->name != NULL) {
			fprintf(f, " ");
			fstrvec_print(f, type->name);
		}
		break;
	default:
		fprintf(f, "UNSUPPORTED TYPE! (type %d)", type->kind);
	}
}

void fdecl_print(FILE *f, ast_decl *decl)
{
	if (decl->typesym->type->isconst)
		fprintf(f, "const ");
	else
		fprintf(f, "let ");
	ftyped_sym_print(f, decl->typesym);
	if (decl->expr != NULL) {
		fprintf(f, " = ");
		fexpr_print(f, decl->expr);
	} else if (decl->typesym->type->kind == Y_STRUCT && decl->typesym->type->name == NULL) {
		fprintf(f, " {\n");
		for (size_t i = 0 ; i < decl->typesym->type->arglist->size; ++i) {
			ftyped_sym_print(f, decl->typesym->type->arglist->elements[i]);
			fprintf(f, ";\n");
		}
		fprintf(f, "}");

	} else if (decl->body != NULL) {
		fprintf(f, " = ");
		fstmt_print(f, decl->body);
	} else if (decl->initializer != NULL) {
		fprintf(f, " = ");
		fprintf(f, "[");
		fprint_expr_list(f, decl->initializer);
		fprintf(f, "]");
	}
	fprintf(f, ";");
}

void fstmt_print(FILE *f, ast_stmt *stmt)
{
	if (!stmt)
		return;
	switch (stmt->kind) {
	case S_DECL:
		fdecl_print(f, stmt->decl);
		break;
	case S_BLOCK:
		stmt = stmt->body;
		fprintf(f, "{\n");
		while (stmt != NULL) {
			fstmt_print(f, stmt);
			stmt = stmt->next;
			fprintf(f, "\n");
		}
		fprintf(f, "}");
		break;
	case S_RETURN:
		fprintf(f, "return");
		if (!stmt->expr) {
			fprintf(f, ";");
			break;
		}
		fprintf(f, " ");
		// fall through
	case S_EXPR:
		fexpr_print(f, stmt->expr);
		fprintf(f, ";");
		break;
	case S_IFELSE:
		fprintf(f, "if (");
		fexpr_print(f, stmt->expr);
		fprintf(f, ") ");
		fstmt_print(f, stmt->body);
		if (stmt->else_body) {
			fprintf(f, " else ");
			fstmt_print(f, stmt->else_body);
		}
		break;
	case S_WHILE:
		fprintf(f, "while (");
		fexpr_print(f, stmt->expr);
		fprintf(f, ") ");
		fstmt_print(f, stmt->body);
		break;
	default:
		fprintf(f, "Somehow reached error/unkown statement printing case?\n");
	}
}

void fprogram_print(FILE *f, ast_decl *program)
{
	if (!program)
		return;
	fdecl_print(f, program);
	fprintf(f, "\n");
	program_print(program->next);
}

void program_print(ast_decl *program)
{
	fprogram_print(stdout, program);
}
void expr_print(ast_expr *expr)
{
	fexpr_print(stdout, expr);
}
void stmt_print(ast_stmt *stmt)
{
	fstmt_print(stdout, stmt);
}
void type_print(ast_type *type)
{
	ftype_print(stdout, type);
}
void decl_print(ast_decl *decl)
{
	fdecl_print(stdout, decl);
}
void typed_sym_print(ast_typed_symbol *typesym)
{
	ftyped_sym_print(stdout, typesym);
}
void e_program_print(ast_decl *program)
{
	fprogram_print(stderr, program);
}
void e_expr_print(ast_expr *expr)
{
	fexpr_print(stderr, expr);
}
void e_stmt_print(ast_stmt *stmt)
{
	fstmt_print(stderr, stmt);
}
void e_type_print(ast_type *type)
{
	ftype_print(stderr, type);
}
void e_decl_print(ast_decl *decl)
{
	fdecl_print(stderr, decl);
}
void e_typed_sym_print(ast_typed_symbol *typesym)
{
	ftyped_sym_print(stderr, typesym);
}
