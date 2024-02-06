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

void expr_print(ast_expr *expr)
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
	ast_typed_symbol *printhead = type->arglist;
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
	case Y_IDENTIFIER:
		strvec_print(type->name);
		break;
	default:
		printf("UNSUPPORTED TYPE! (type %d)", type->kind);
	}
}

void decl_print(ast_decl *decl)
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

void stmt_print(ast_stmt *stmt)
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
