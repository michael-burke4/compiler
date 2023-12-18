#include "ast.h"
#include "error.h"
#include "parse.h"
#include "token.h"
#include <stdlib.h>
#include <stdio.h>

static inline void next(token_s **cur_token)
{
	*cur_token = (*cur_token)->next;
}

static inline token_t get_type(token_s **cur_token)
{
	return (*cur_token)->type;
}

static inline size_t get_line(token_s **cur_token)
{
	return (*cur_token)->line;
}

static inline size_t get_col(token_s **cur_token)
{
	return (*cur_token)->line;
}
// Can search for a specific token to sync to. the or_newline enabled means the
// parser will consider itself synced upon reaching the specified target token
// type, OR a newline. If you just want to sync to a newline, put T_EOF as the
// target token.
//
// Parser is also considered synced at EOF, no matter the target.
static void sync_to(token_s **cur_token, token_t target, int or_newline)
{
	size_t prevline = get_line(cur_token);
	while (get_type(cur_token) != target) {
		if (or_newline && prevline != get_line(cur_token))
			break;
		if (get_type(cur_token) == T_EOF)
			break;
		next(cur_token);
	}
}

ast_decl *parse_program(token_s **cur_token)
{
	if (get_type(cur_token) == T_EOF)
		return 0;
	ast_decl *ret = parse_decl(cur_token);
	ret->next = parse_program(cur_token);
	return ret;
}

ast_decl *parse_decl(token_s **cur_token)
{
	ast_type *type = 0;
	strvec *name = 0;
	ast_expr *expr = 0;

	type = parse_type(cur_token);

	if (!type)
		goto decl_parse_err;

	if (get_type(cur_token) != T_IDENTIFIER) {
		report_error_tok("Missing identifier or valid type specifier",
				 *cur_token);
		sync_to(cur_token, T_EOF, 1);
		goto decl_parse_err;
	} else {
		name = (*cur_token)->text; // Steal the strvec from the token >:)
		(*cur_token)->text = 0;
		next(cur_token);
	}

	if (get_type(cur_token) == T_SEMICO)
		next(cur_token);
	else if (get_type(cur_token) == T_ASSIGN) {
		next(cur_token);
		expr = parse_expr(cur_token);
		if (get_type(cur_token) != T_SEMICO) {
			report_error_tok(
				"Missing semicolon (possibly missing from previous line)",
				*cur_token);
			sync_to(cur_token, T_ERROR, 1);
			goto decl_parse_err;
		} else {
			next(cur_token);
		}
	} else {
		report_error_tok(
			"Missing semicolon (possibly missing on previous line)",
			*cur_token);
		goto decl_parse_err;
	}
	return decl_init(type, name, expr, 0);

decl_parse_err:
	type_destroy(type);
	strvec_destroy(name);
	expr_destroy(expr);
	return decl_init(0, 0, 0, 0);
}

ast_type *parse_type(token_s **cur_token)
{
	ast_type *ret = 0;
	strvec *text = 0;

	switch (get_type(cur_token)) {
	case T_I64:
	case T_I32:
	case T_U64:
	case T_U32:
	case T_STRING:
	case T_CHAR:
	case T_VOID:
	case T_BOOL:
		ret = type_init(get_type(cur_token), 0);
		next(cur_token);
		break;
	case T_IDENTIFIER:
		text = (*cur_token)->text;
		(*cur_token)->text = 0;
		ret = type_init(T_IDENTIFIER, text);
		next(cur_token);
		break;
	default:
		report_error_tok("Could not use the following token as a type:",
				 *cur_token);
		printf("\t");
		tok_print(*cur_token);
		sync_to(cur_token, T_ERROR, 1);
	}
	return ret;
}

// Shoutouts to https://www.engr.mun.ca/~theo/Misc/exp_parsing.htm#classic
// for info on parsing binary expressions with recursive descent parsers :)
ast_expr *parse_expr(token_s **cur_token)
{
	return parse_expr_assign(cur_token);
}

ast_expr *parse_expr_assign(token_s **cur_token)
{
	ast_expr *this = parse_expr_comparison(cur_token);
	ast_expr *that;
	token_t typ = get_type(cur_token);
	token_t op;
	while (typ == T_ASSIGN || typ == T_ADD_ASSIGN || typ == T_SUB_ASSIGN || typ == T_MUL_ASSIGN
		|| typ == T_DIV_ASSIGN || typ == T_MOD_ASSIGN || typ == T_BW_AND_ASSIGN ||
		typ == T_BW_OR_ASSIGN) { // This is dumb
		op = typ;
		next(cur_token);
		that = parse_expr_comparison(cur_token);
		this = expr_init(E_ASSIGN, this, that, op, 0, 0, 0);
		typ = get_type(cur_token);
	}
	return this;
}

ast_expr *parse_expr_comparison(token_s **cur_token)
{
	ast_expr *this = parse_expr_addsub(cur_token);
	ast_expr *that;
	token_t typ = get_type(cur_token);
	token_t op;
	while (typ == T_LT || typ == T_LTE || typ == T_GT || typ == T_GTE) {
		op = typ;
		next(cur_token);
		that = parse_expr_addsub(cur_token);
		this = expr_init(E_COMPARISON, this, that, op, 0, 0, 0);
		typ = get_type(cur_token);
	}
	return this;
}

ast_expr *parse_expr_addsub(token_s **cur_token)
{
	ast_expr *this = parse_expr_muldiv(cur_token);
	ast_expr *that;
	token_t typ = get_type(cur_token);
	token_t op;
	while (typ == T_MINUS || typ == T_PLUS) {
		op = typ;
		next(cur_token);
		that = parse_expr_muldiv(cur_token);
		this = expr_init(E_ADDSUB, this, that, op, 0, 0, 0);
		typ = get_type(cur_token);
	}
	return this;
}

ast_expr *parse_expr_muldiv(token_s **cur_token)
{
	ast_expr *this = parse_expr_pre_unary(cur_token);
	ast_expr *that;
	token_t typ = get_type(cur_token);
	token_t op;
	while (typ == T_STAR || typ == T_FSLASH || typ == T_PERCENT) {
		op = typ;
		next(cur_token);
		that = parse_expr_unit(cur_token);
		this = expr_init(E_MULDIV, this, that, op, 0, 0, 0);
		typ = get_type(cur_token);
	}
	return this;
}

ast_expr *parse_expr_pre_unary(token_s **cur_token)
{
	ast_expr *inner = 0;
	token_t typ = get_type(cur_token);

	switch (typ) {
	case T_DPLUS:
	case T_DMINUS:
	case T_MINUS:
	case T_NOT:
	case T_BW_NOT:
	case T_STAR:
		next(cur_token);
		inner = parse_expr(cur_token);
		return expr_init(E_PRE_UNARY, inner, 0, typ, 0, 0, 0);
	default:
		return parse_expr_unit(cur_token);
	}
}

ast_expr *parse_expr_unit(token_s **cur_token)
{
	token_t typ = get_type(cur_token);
	token_s *cur = *cur_token;
	strvec *txt;
	ast_expr *inner = 0;
	switch (typ) {
	case T_LPAREN:
		next(cur_token);
		inner = parse_expr(cur_token);
		if (get_type(cur_token) != T_RPAREN) {
			report_error_tok("Expression is missing a closing paren", *cur_token);
			sync_to(cur_token, T_EOF, 1);
			expr_destroy(inner);
			return 0;
		}
		next(cur_token);
		return expr_init(E_PAREN, inner, 0, 0, 0, 0, 0);
	case T_INT_LIT:
		next(cur_token);
		return expr_init(E_INT_LIT, 0, 0, 0, 0, strvec_toi(cur->text), 0);
	case T_STR_LIT:
		txt = cur->text;
		cur->text = 0;
		next(cur_token);
		return expr_init(E_STR_LIT, 0, 0, 0, 0, 0, txt);
	case T_IDENTIFIER:
		txt = cur->text;
		cur->text = 0;
		next(cur_token);
		return expr_init(E_IDENTIFIER, 0, 0, 0, txt, 0, 0);
	case T_TRUE:
		next(cur_token);
		return expr_init(E_TRUE_LIT, 0, 0, 0, 0, 0, 0);
	case T_FALSE:
		next(cur_token);
		return expr_init(E_FALSE_LIT, 0, 0, 0, 0, 0, 0);
	default:
		report_error_tok(
			"Could not parse expr unit. The offending token in question:",
			*cur_token);
		printf("\t");
		tok_print(*cur_token);
		sync_to(cur_token, T_EOF, 1);
		return 0;
	}
}
