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

static inline int expect(token_s **cur_token, token_t expected)
{
	return get_type(cur_token) == expected;
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
	while (!expect(cur_token, target)) {
		if (or_newline && prevline != get_line(cur_token))
			break;
		if (expect(cur_token, T_EOF))
			break;
		next(cur_token);
	}
}

ast_decl *parse_program(token_s **cur_token)
{
	if (expect(cur_token, T_EOF))
		return 0;
	ast_decl *ret = parse_decl(cur_token);
	ret->next = parse_program(cur_token);
	return ret;
}

// Parsing a non-empty arglist will return a linked list of typed symbols
// An arglist with an error in it will return 0
// an empty arglist will return a pointer to the area in memory that is 0 + sizeof(ast_typed_symbol)
//	This is a dumb hack to get around the fact that an empty arglist should be 0 but I also
//	want to communicate that there was no issue.
//	THEORETICALLY there could be a weird error if the return value of this function
//	is actually allocated at that 0 + sizeof(...), but I will accept that risk and continue
//	living in denial and ignorance.
//
//	Could just have an `int *empty` out param but uhhhh...
static ast_typed_symbol *parse_arglist(token_s **cur_token)
{
	ast_typed_symbol *head = 0;
	ast_typed_symbol *tmp = 0;
	ast_typed_symbol *tmp2 = 0;
	token_t typ;

	head += 1;
	while (1) {
		typ = get_type(cur_token);
		if (typ == T_RPAREN) {
			next(cur_token);
			return head;
		} else if (head == ((ast_typed_symbol *)0) + 1) {
			head = parse_typed_symbol(cur_token);
			if (!head)
				return 0;
			tmp = head;
		} else if (typ == T_COMMA) {
			next(cur_token);
			tmp2 = parse_typed_symbol(cur_token);
			if (!tmp2)
				return 0;
			tmp->next = tmp2;
			tmp = tmp->next;
		} else
			return 0;
	}
}

ast_typed_symbol *parse_typed_symbol(token_s **cur_token)
{
	ast_type *type = 0;
	strvec *name = 0;

	if (!expect(cur_token, T_IDENTIFIER))
		goto parse_typsym_err;
	name = (*cur_token)->text;
	(*cur_token)->text = 0;
	next(cur_token);

	if (!expect(cur_token, T_COLON))
		goto parse_typsym_err;
	next(cur_token);

	type = parse_type(cur_token);
	if (!type)
		goto parse_typsym_err;
	return ast_typed_symbol_init(type, name);

parse_typsym_err:
	type_destroy(type);
	strvec_destroy(name);
	return 0;
}

//TODO: error reporting for function declarations is REALLY bad right now.
ast_decl *parse_decl(token_s **cur_token)
{
	ast_typed_symbol *typed_symbol = 0;
	ast_expr *expr = 0;
	ast_stmt *stmt = 0;

	if (!expect(cur_token, T_LET)) {
		report_error_tok("Missing 'let' keyword in declaration.", *cur_token);
		sync_to(cur_token, T_EOF,
			1); // maybe this should be in the goto
		goto decl_parse_err;
	}
	next(cur_token);
	typed_symbol = parse_typed_symbol(cur_token);
	if (!typed_symbol) {
		report_error_tok("Missing/invalid type specifier or name.", *cur_token);
		sync_to(cur_token, T_EOF, 1);
		goto decl_parse_err;
	}

	if (get_type(cur_token) == T_SEMICO)
		next(cur_token);
	else if (expect(cur_token, T_ASSIGN)) {
		next(cur_token);
		if (expect(cur_token, T_LCURLY))
			stmt = parse_stmt_block(cur_token);
		else
			expr = parse_expr(cur_token);
		if (!expect(cur_token, T_SEMICO)) {
			report_error_tok("Missing semicolon (possibly missing from previous line)",
					 *cur_token);
			sync_to(cur_token, T_EOF, 1);
			goto decl_parse_err;
		} else {
			next(cur_token);
		}
	} else {
		report_error_tok("Missing semicolon (maybe missing on previous line)", *cur_token);
		goto decl_parse_err;
	}
	return decl_init(typed_symbol, expr, stmt, 0);

decl_parse_err:
	ast_typed_symbol_destroy(typed_symbol);
	expr_destroy(expr);
	return decl_init(0, 0, 0, 0);
}

ast_stmt *parse_stmt_block(token_s **cur_token)
{
	ast_stmt *head = 0;
	ast_stmt *current = 0;
	if (!expect(cur_token, T_LCURLY)) {
		return 0;
	}
	head = stmt_init(S_BLOCK, 0, 0, 0, 0);
	head->next = 0;
	next(cur_token);
	current = head;
	while (!expect(cur_token, T_RCURLY) && !expect(cur_token, T_EOF)) {
		//if (!head) {
		//	head = parse_stmt(cur_token);
		//	current = head;
		//} else {
		//	current->next = parse_stmt(cur_token);
		//	current = current->next;
		//}
		current->next = parse_stmt(cur_token);
		current = current->next;
	}
	next(cur_token);
	return head;
}
ast_stmt *parse_stmt(token_s **cur_token)
{
	stmt_t kind;
	ast_decl *decl = 0;
	ast_expr *expr = 0;
	ast_stmt *body = 0;
	ast_stmt *else_body = 0;

	switch (get_type(cur_token)) {
	case T_LET:
		kind = S_DECL;
		decl = parse_decl(cur_token);
		break;
	case T_IF:
		kind = S_IFELSE;
		next(cur_token);
		if (!expect(cur_token, T_LPAREN)) {
			report_error_tok("Missing left paren in `if` condition.", *cur_token);
			goto stmt_err;
		}
		next(cur_token);
		expr = parse_expr(cur_token);
		if (!expect(cur_token, T_RPAREN)) {
			report_error_tok("Missing right paren in `if` condition.", *cur_token);
			goto stmt_err;
		}
		next(cur_token);
		body = parse_stmt_block(cur_token);
		if (expect(cur_token, T_ELSE)) {
			next(cur_token);
			else_body = parse_stmt_block(cur_token);
		}
		break;
	case T_RETURN:
		kind = S_RETURN;
		next(cur_token);
		if (!expect(cur_token, T_SEMICO)) {
			expr = parse_expr(cur_token);
			if (!expr) {
				report_error_tok("Could not parse expression in return statement.",
						 *cur_token);
				goto stmt_err;
			}
			if (!expect(cur_token, T_SEMICO)) {
				report_error_tok("Statement missing terminating semicolon.",
						 *cur_token);
				goto stmt_err;
			}
			next(cur_token);
		}
		break;
	default:
		kind = S_EXPR;
		expr = parse_expr(cur_token);
		if (!expr) {
			report_error_tok("could not parse statement", *cur_token);
			goto stmt_err;
		}
		if (!expect(cur_token, T_SEMICO)) {
			report_error_tok("Statement missing terminating semicolon.", *cur_token);
			goto stmt_err;
		}
		next(cur_token);
	}
	return stmt_init(kind, decl, expr, body, else_body);

stmt_err:
	sync_to(cur_token, T_EOF, 1);
	decl_destroy(decl);
	expr_destroy(expr);
	stmt_destroy(body);
	stmt_destroy(else_body);
	return stmt_init(S_ERROR, 0, 0, 0, 0);
}
//ast_stmt *stmt_init(stmt_t kind, ast_decl *decl, ast_expr *expr, ast_stmt *body,
//ast_stmt *else_body);

ast_type *parse_type(token_s **cur_token)
{
	ast_type *ret = 0;
	ast_type *subtype = 0;
	ast_typed_symbol *arglist = 0;
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
	case T_LPAREN:
		next(cur_token);
		arglist = parse_arglist(cur_token);
		if (!arglist) {
			report_error_tok("Failed parsing argument list in function declaration.",
					 *cur_token);
			sync_to(cur_token, T_ASSIGN, 1);
		}
		// This is kind of genius but also 100% a dumb hack
		if (arglist == (((ast_typed_symbol *)0) + 1)) {
			arglist = 0;
		}
		if (!expect(cur_token, T_ARROW)) {
			report_error_tok("Missing arrow in function declaration.", *cur_token);
			sync_to(cur_token, T_EOF, 1);
			break;
		}
		next(cur_token);
		subtype = parse_type(cur_token);
		if (!subtype) {
			report_error_tok("Missing/invalid return type in function declaration.",
					 *cur_token);
			sync_to(cur_token, T_ASSIGN, 1);
		}
		ret = type_init(T_ARROW, 0);
		ret->subtype = subtype;
		ret->arglist = arglist;
		break;
	default:
		report_error_tok("Could not use the following token as a type:", *cur_token);
		printf("\t");
		tok_print(*cur_token);
		sync_to(cur_token, T_EOF, 1);
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
	ast_expr *this = parse_expr_equality(cur_token);
	ast_expr *that;
	token_t typ = get_type(cur_token);
	token_t op;
	while (typ == T_ASSIGN || typ == T_ADD_ASSIGN || typ == T_SUB_ASSIGN ||
	       typ == T_MUL_ASSIGN || typ == T_DIV_ASSIGN || typ == T_MOD_ASSIGN ||
	       typ == T_BW_AND_ASSIGN || typ == T_BW_OR_ASSIGN) { // This is dumb
		op = typ;
		next(cur_token);
		that = parse_expr_inequality(cur_token);
		this = expr_init(E_ASSIGN, this, that, op, 0, 0, 0);
		typ = get_type(cur_token);
	}
	return this;
}

ast_expr *parse_expr_equality(token_s **cur_token)
{
	ast_expr *this = parse_expr_inequality(cur_token);
	ast_expr *that;
	token_t typ = get_type(cur_token);
	token_t op;
	while (typ == T_NEQ || typ == T_EQ) {
		op = typ;
		next(cur_token);
		that = parse_expr_addsub(cur_token);
		this = expr_init(E_EQUALITY, this, that, op, 0, 0, 0);
		typ = get_type(cur_token);
	}
	return this;
}

ast_expr *parse_expr_inequality(token_s **cur_token)
{
	ast_expr *this = parse_expr_addsub(cur_token);
	ast_expr *that;
	token_t typ = get_type(cur_token);
	token_t op;
	while (typ == T_LT || typ == T_LTE || typ == T_GT || typ == T_GTE) {
		op = typ;
		next(cur_token);
		that = parse_expr_addsub(cur_token);
		this = expr_init(E_INEQUALITY, this, that, op, 0, 0, 0);
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
		return parse_expr_post_unary(cur_token);
	}
}

ast_expr *parse_expr_post_unary(token_s **cur_token)
{
	ast_expr *inner = parse_expr_unit(cur_token);
	token_t typ = get_type(cur_token);
	// something like x++++; should parse but fail at typechecking. Could fail it now but w/e.
	while (typ == T_DPLUS || typ == T_DMINUS) {
		next(cur_token);
		inner = expr_init(E_POST_UNARY, inner, 0, typ, 0, 0, 0);
		typ = get_type(cur_token);
	}
	return inner;
}

ast_expr *parse_expr_unit(token_s **cur_token)
{
	token_t typ = get_type(cur_token);
	token_s *cur = *cur_token;
	strvec *txt;
	ast_expr *inner = 0;
	token_t op;
	switch (typ) {
	case T_DPLUS:
	case T_DMINUS:
		while ((op = get_type(cur_token)) == T_DPLUS || op == T_DMINUS)
		case T_LPAREN:
			next(cur_token);
		inner = parse_expr(cur_token);
		if (get_type(cur_token) != T_RPAREN) {
			report_error_tok(
				"Expression is missing a closing paren", // TODO leave error handling to fns like parse_stmt and parse_decl. Just bubble the error up by returning zero.
				*cur_token);
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
		report_error_tok("Could not parse expr unit. The offending token in question:",
				 *cur_token);
		printf("\t");
		tok_print(*cur_token);
		sync_to(cur_token, T_EOF, 1);
		return 0;
	}
}
