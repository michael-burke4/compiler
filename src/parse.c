#include "ast.h"
#include "error.h"
#include "parse.h"
#include "token.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

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

static vect *parse_comma_separated_exprs(token_s **cur_token, token_t closer) {
	vect *ret = NULL;
	next(cur_token);
	ast_expr *cur;
	if (expect(cur_token, closer)) {
		next(cur_token);
		return ret;
	}
	ret = vect_init(4);
	do {
		if (ret->size != 0)
			next(cur_token); // we know cur_token is a comma from while condition.
		cur = parse_expr(cur_token);
		if (cur != NULL)
			vect_append(ret, (void *)cur);
		else {
			destroy_expr_vect(ret);
			return NULL;
		}
	} while (expect(cur_token, T_COMMA));
	if (!expect(cur_token, closer)) {
		report_error_tok(*cur_token, "Expression list is missing closing token");
		destroy_expr_vect(ret);
		ret = NULL;
	}
	next(cur_token);
	return ret;
}

ast_decl *parse_program(token_s **cur_token)
{
	if (expect(cur_token, T_EOF))
		return NULL;
	ast_decl *ret = parse_decl(cur_token);
	ret->next = parse_program(cur_token);
	return ret;
}

static vect *parse_arglist(token_s **cur_token, int *had_arglist_error)
{
	vect *ret = NULL;
	int empty = 1;
	token_t typ;
	ast_typed_symbol *ts;
	while (1) {
		typ = get_type(cur_token);
		if (typ == T_RPAREN) {
			next(cur_token);
			return ret;
		} else if (empty) {
			ts = parse_typed_symbol(cur_token);
			if (ts == NULL) {
				*had_arglist_error = 1;
				return NULL;
			}
			empty = 0;
			ret = vect_init(2);
			vect_append(ret, (void *)ts);
		} else if (typ == T_COMMA) {
			next(cur_token);
			ts = parse_typed_symbol(cur_token);
			if (ts == NULL) {
				*had_arglist_error = 1;
				return NULL;
			}
			vect_append(ret, (void *)ts);
		} else {
			*had_arglist_error = 1;
			return NULL;
		}
	}
}

ast_typed_symbol *parse_typed_symbol(token_s **cur_token)
{
	ast_type *type = NULL;
	strvec *name = NULL;

	if (!expect(cur_token, T_IDENTIFIER))
		goto parse_typsym_err;
	name = (*cur_token)->text;
	(*cur_token)->text = NULL;
	next(cur_token);

	if (!expect(cur_token, T_COLON))
		goto parse_typsym_err;
	next(cur_token);

	type = parse_type(cur_token);
	if (type == NULL)
		goto parse_typsym_err;
	return ast_typed_symbol_init(type, name);

parse_typsym_err:
	tok_print(*cur_token);
	type_destroy(type);
	strvec_destroy(name);
	return NULL;
}

static void destroy_def_vect(vect *def_vect)
{
	if (def_vect == NULL)
		return;
	for (size_t i = 0 ; i < def_vect->size ; ++i)
		ast_typed_symbol_destroy(def_vect->elements[i]);
	vect_destroy(def_vect);
}

static vect *parse_struct_def(token_s **cur_token)
{
	ast_typed_symbol *cur;
	vect *def_vect = vect_init(3);
	if (!expect(cur_token, T_LCURLY)) {
		report_error_tok(*cur_token, "missing opening brace in struct definition");
		sync_to(cur_token, T_SEMICO, 0);
		destroy_def_vect(def_vect);
		return NULL;
	}
	next(cur_token);
	if (expect(cur_token, T_RCURLY)) {
		report_error_tok(*cur_token, "struct definition can't be empty");
		sync_to(cur_token, T_SEMICO, 0);
		next(cur_token);
		destroy_def_vect(def_vect);
		return NULL;
	}
	while (!expect(cur_token, T_RCURLY)) {
		cur = parse_typed_symbol(cur_token);
		if (cur != NULL && !expect(cur_token, T_SEMICO)) {
			report_error_tok(*cur_token, "missing semicolon in struct field definition");
			sync_to(cur_token, T_EOF, 1);
		} else if (cur == NULL) {
			report_error_tok(*cur_token, "Couldn't parse typed symbol");
			sync_to(cur_token, T_SEMICO, 1);
		} else
			next(cur_token);
		vect_append(def_vect, cur);
	}
	next(cur_token);
	if (!expect(cur_token, T_SEMICO)) {
		report_error_tok(*cur_token, "missing semicolon after struct definition");
		sync_to(cur_token, T_EOF, 1);
		vect_destroy(def_vect);
		return NULL;
	}
	next(cur_token);
	return def_vect;
}

//TODO: error reporting for function declarations is REALLY bad right now.
ast_decl *parse_decl(token_s **cur_token)
{
	ast_typed_symbol *typed_symbol = NULL;
	ast_expr *expr = NULL;
	ast_stmt *stmt = NULL;
	vect *initializer = NULL;
	ast_decl *ret = NULL;
	strvec *name;
	ast_type *type;
	if (expect(cur_token, T_STRUCT)) {
		next(cur_token);
		if (!expect(cur_token, T_IDENTIFIER)) {
			report_error_tok(*cur_token, "Expected struct name after struct keyword");
			sync_to(cur_token, T_LCURLY, 0);
		} else {
			type = type_init(Y_STRUCT, NULL);
			name = (*cur_token)->text;
			(*cur_token)->text = NULL;
			typed_symbol = ast_typed_symbol_init(type, name);
			next(cur_token);
		}

		initializer = parse_struct_def(cur_token);
		typed_symbol->type->arglist = initializer;
		ret = decl_init(typed_symbol, NULL, NULL, NULL);
		return ret;
	} else if (!expect(cur_token, T_LET)) {
		report_error_tok(*cur_token, "Missing 'let' keyword in declaration.");
		sync_to(cur_token, T_EOF,
			1); // maybe this should be in the goto
		goto decl_parse_err;
	}
	next(cur_token);
	typed_symbol = parse_typed_symbol(cur_token);
	if (typed_symbol == NULL) {
		report_error_tok(*cur_token, "Missing/invalid type specifier or name.");
		sync_to(cur_token, T_EOF, 1);
		goto decl_parse_err;
	}

	if (get_type(cur_token) == T_SEMICO)
		next(cur_token);
	else if (expect(cur_token, T_ASSIGN)) {
		next(cur_token);
		if (expect(cur_token, T_LCURLY))
			stmt = parse_stmt_block(cur_token);
		else if (expect(cur_token, T_LBRACKET))
			initializer = parse_comma_separated_exprs(cur_token, T_RBRACKET);
		else
			expr = parse_expr(cur_token);
		if (!expect(cur_token, T_SEMICO)) {
			report_error_tok(*cur_token, "Missing semicolon (possibly missing from previous line)");
			sync_to(cur_token, T_EOF, 1);
			goto decl_parse_err;
		} else {
			next(cur_token);
		}
	} else {
		report_error_tok(*cur_token, "Missing semicolon (maybe missing on previous line)");
		goto decl_parse_err;
	}
	ret = decl_init(typed_symbol, expr, stmt, NULL);
	ret->initializer = initializer;
	return ret;

decl_parse_err:
	ast_typed_symbol_destroy(typed_symbol);
	expr_destroy(expr);
	stmt_destroy(stmt);
	return decl_init(NULL, NULL, NULL, NULL);
}

ast_stmt *parse_stmt_block(token_s **cur_token)
{
	ast_stmt *block = NULL;
	ast_stmt *current = NULL;
	if (!expect(cur_token, T_LCURLY)) {
		return NULL;
	}
	block = stmt_init(S_BLOCK, NULL, NULL, NULL, NULL);
	block->next = NULL;
	next(cur_token);
	while (!expect(cur_token, T_RCURLY) && !expect(cur_token, T_EOF)) {
		if (current == NULL) {
			block->body = parse_stmt(cur_token);
			current = block->body;
		}
		else {
			current->next = parse_stmt(cur_token);
			current = current->next;
		}
	}
	next(cur_token);
	return block;
}
ast_stmt *parse_stmt(token_s **cur_token)
{
	stmt_t kind;
	ast_decl *decl = NULL;
	ast_expr *expr = NULL;
	ast_stmt *body = NULL;
	ast_stmt *else_body = NULL;

	switch (get_type(cur_token)) {
	case T_LET:
		kind = S_DECL;
		decl = parse_decl(cur_token);
		break;
	case T_IF:
		kind = S_IFELSE;
		next(cur_token);
		if (!expect(cur_token, T_LPAREN)) {
			report_error_tok(*cur_token, "Missing left paren in `if` condition.");
			goto stmt_err;
		}
		next(cur_token);
		expr = parse_expr(cur_token);
		if (!expect(cur_token, T_RPAREN)) {
			report_error_tok(*cur_token, "Missing right paren in `if` condition.");
			goto stmt_err;
		}
		next(cur_token);
		body = parse_stmt_block(cur_token);
		if (expect(cur_token, T_ELSE)) {
			next(cur_token);
			else_body = parse_stmt_block(cur_token);
		}
		break;
	case T_WHILE:
		kind = S_WHILE;
		next(cur_token);
		if (!expect(cur_token, T_LPAREN)) {
			report_error_tok(*cur_token, "Missing left paren in `while` condition.");
			goto stmt_err;
		}
		next(cur_token);
		expr = parse_expr(cur_token);
		if (!expect(cur_token, T_RPAREN)) {
			report_error_tok(*cur_token, "Missing right paren in `while` condition.");
			goto stmt_err;
		}
		next(cur_token);
		body = parse_stmt_block(cur_token);
		break;
	case T_RETURN:
		kind = S_RETURN;
		next(cur_token);
		if (!expect(cur_token, T_SEMICO)) {
			expr = parse_expr(cur_token);
			if (expr == NULL) {
				report_error_tok(*cur_token, "Could not parse expression in return statement.");
				goto stmt_err;
			}
			if (!expect(cur_token, T_SEMICO)) {
				report_error_tok(*cur_token, "Statement missing terminating semicolon a.");
				goto stmt_err;
			}
			next(cur_token);
		}
		else
			next(cur_token);
		break;
	default:
		kind = S_EXPR;
		expr = parse_expr(cur_token);
		if (expr == NULL) {
			report_error_tok(*cur_token, "could not parse statement");
			goto stmt_err;
		}
		if (!expect(cur_token, T_SEMICO)) {
			report_error_tok(*cur_token, "Statement missing terminating semicolon b.");
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
	return stmt_init(S_ERROR, NULL, NULL, NULL, NULL);
}
//ast_stmt *stmt_init(stmt_t kind, ast_decl *decl, ast_expr *expr, ast_stmt *body,
//ast_stmt *else_body);

ast_type *parse_type(token_s **cur_token)
{
	ast_type *ret = NULL;
	ast_type *subtype = NULL;
	vect *arglist = NULL;
	int had_arglist_err = 0;

	switch (get_type(cur_token)) {
	case T_I64:
		ret = type_init(Y_I64, NULL);
		next(cur_token);
		break;
	case T_I32:
		ret = type_init(Y_I32, NULL);
		next(cur_token);
		break;
	case T_U64:
		ret = type_init(Y_U64, NULL);
		next(cur_token);
		break;
	case T_U32:
		ret = type_init(Y_U32, NULL);
		next(cur_token);
		break;
	case T_STRING:
		ret = type_init(Y_STRING, NULL);
		next(cur_token);
		break;
	case T_CHAR:
		ret = type_init(Y_CHAR, NULL);
		next(cur_token);
		break;
	case T_VOID:
		ret = type_init(Y_VOID, NULL);
		next(cur_token);
		break;
	case T_BOOL:
		ret = type_init(Y_BOOL, NULL);
		next(cur_token);
		break;
	case T_STRUCT:
		ret = type_init(Y_STRUCT, NULL);
		next(cur_token);
		if (!expect(cur_token, T_IDENTIFIER)) {
			report_error_tok(*cur_token, "struct instances require names after the `struct` keyword");
			sync_to(cur_token, T_ASSIGN, 1);
			type_destroy(ret);
			return NULL;
		}
		ret->name = (*cur_token)->text;
		(*cur_token)->text = NULL;
		next(cur_token);
		break;
	case T_LPAREN:
		next(cur_token);
		arglist = parse_arglist(cur_token, &had_arglist_err);
		if (had_arglist_err) {
			report_error_tok(*cur_token, "Failed parsing argument list in function declaration.");
			sync_to(cur_token, T_ASSIGN, 1);
		}
		if (!expect(cur_token, T_ARROW)) {
			report_error_tok(*cur_token, "Missing arrow in function declaration.");
			sync_to(cur_token, T_EOF, 1);
			break;
		}
		next(cur_token);
		subtype = parse_type(cur_token);
		if (subtype == NULL) {
			report_error_tok(*cur_token, "Missing/invalid return type in function declaration.");
			sync_to(cur_token, T_ASSIGN, 1);
		}
		ret = type_init(Y_FUNCTION, NULL);
		ret->subtype = subtype;
		ret->arglist = arglist;
		break;
	default:
		report_error_tok(*cur_token, "Could not use the following token as a type:");
		printf("\t");
		tok_print(*cur_token);
		sync_to(cur_token, T_EOF, 1);
	}
	while (get_type(cur_token) == T_STAR) {
		subtype = ret;
		ret = type_init(Y_POINTER, NULL);
		ret->subtype = subtype;
		next(cur_token);
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
	       typ == T_BW_AND_ASSIGN || typ == T_BW_OR_ASSIGN) {
		op = typ;
		next(cur_token);
		that = parse_expr_inequality(cur_token);
		this = expr_init(E_ASSIGN, this, that, op, NULL, 0, NULL);
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
		this = expr_init(E_EQUALITY, this, that, op, NULL, 0, NULL);
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
		this = expr_init(E_INEQUALITY, this, that, op, NULL, 0, NULL);
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
		this = expr_init(E_ADDSUB, this, that, op, NULL, 0, NULL);
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
		that = parse_expr_pre_unary(cur_token);
		this = expr_init(E_MULDIV, this, that, op, NULL, 0, NULL);
		typ = get_type(cur_token);
	}
	return this;
}

ast_expr *parse_expr_pre_unary(token_s **cur_token)
{
	ast_expr *inner = NULL;
	token_t typ = get_type(cur_token);
	ast_expr *ret = NULL;

	switch (typ) {
	case T_DPLUS:
	case T_DMINUS:
	case T_MINUS:
	case T_NOT:
	case T_BW_NOT:
	case T_AMPERSAND:
		next(cur_token);
		inner = parse_expr_pre_unary(cur_token); // TODO: is this correct? Do a lot of thinking.
		return expr_init(E_PRE_UNARY, inner, NULL, typ, NULL, 0, NULL);
	case T_STAR:
		next(cur_token);
		inner = parse_expr_pre_unary(cur_token);
		ret = expr_init(E_PRE_UNARY, inner, NULL, typ, NULL, 0, NULL);
		ret->is_lvalue = 1;
		return ret;
	default:
		return parse_expr_post_unary(cur_token);
	}
}

// Now with member operator this isn't actually unary but w/e.
ast_expr *parse_expr_post_unary(token_s **cur_token)
{
	ast_expr *inner = parse_expr_unit(cur_token);
	ast_expr *e = NULL;
	token_t typ = get_type(cur_token);
	// something like x++++; should parse but fail at typechecking. Could fail it now but w/e.
	// TODO: Make this look just like parse_expr_pre_unary
	while (typ == T_DPLUS || typ == T_DMINUS || typ == T_LBRACKET || typ == T_PERIOD) {
		next(cur_token);
		if (typ == T_LBRACKET) {
			e = parse_expr(cur_token);
			if (!expect(cur_token, T_RBRACKET)) {
				report_error_tok(*cur_token, "Missing closing bracket");
				expr_destroy(e);
				return NULL;
			}
			next(cur_token);
		} else if (typ == T_PERIOD) {
			if (!expect(cur_token, T_IDENTIFIER)) {
				report_error_tok(*cur_token, "Member operator right side must be an identifier");
				return NULL;
			}
			e = parse_expr_unit(cur_token); // This can only be an expr unit: we know it's an identifier.
		}
		inner = expr_init(E_POST_UNARY, inner, NULL, typ, NULL, 0, NULL);
		inner->right = e;
		inner->is_lvalue = 1;
		typ = get_type(cur_token);
	}
	return inner;
}


ast_expr *parse_expr_unit(token_s **cur_token)
{
	token_t typ = get_type(cur_token);
	token_s *cur = *cur_token;
	strvec *txt;
	ast_expr *ex = NULL;
	ast_expr *ret;
	//token_t op;
	switch (typ) {
	case T_LPAREN:
		next(cur_token);
		ex = parse_expr(cur_token);
		if (get_type(cur_token) != T_RPAREN) {
			// TODO leave error handling to fns like parse_stmt and parse_decl.
			// Just bubble the error up by returning zero.
			report_error_tok(*cur_token, "Expression is missing a closing paren");
			sync_to(cur_token, T_EOF, 1);
			expr_destroy(ex);
			return NULL;
		}
		next(cur_token);
		ret = expr_init(E_PAREN, ex, NULL, 0, NULL, 0, NULL);
		ret->is_lvalue = ex->is_lvalue;
		return ret;
	case T_INT_LIT:
		next(cur_token);
		int64_t n;
		n = strvec_tol(cur->text);
		if (errno != 0) {
			report_error_tok(*cur_token, "Could not parse int literal \"");
			strvec_print(cur->text);
			puts("\"");
		}
		ex = expr_init(E_INT_LIT, NULL, NULL, 0, NULL, n, NULL);
		ex->int_size = smallest_fit(n);
		return ex;
	case T_STR_LIT:
		txt = cur->text;
		cur->text = NULL;
		next(cur_token);
		return expr_init(E_STR_LIT, NULL, NULL, 0, NULL, 0, txt);
	case T_SYSCALL:
		next(cur_token);
		if (!expect(cur_token, T_LPAREN)) {
			report_error_tok(*cur_token, "Missing open paren after `syscall`");
			sync_to(cur_token, T_EOF, 1);
			return expr_init(E_SYSCALL, NULL, NULL, 0, NULL, 0, NULL);
		}
		ex = expr_init(E_SYSCALL, NULL, NULL, 0, NULL, 0, NULL);
		ex->sub_exprs = parse_comma_separated_exprs(cur_token, T_RPAREN);
		return ex;
	case T_IDENTIFIER:
		txt = cur->text;
		cur->text = NULL;
		next(cur_token);
		if (expect(cur_token, T_LPAREN)) {
			ex = expr_init(E_FNCALL, NULL, NULL, 0, txt, 0, NULL);
			ex->sub_exprs = parse_comma_separated_exprs(cur_token, T_RPAREN);
			return ex;
		} else {
			ex = expr_init(E_IDENTIFIER, NULL, NULL, 0, txt, 0, NULL);
			ex->is_lvalue = 1;
			return ex;
		}
	case T_TRUE:
		next(cur_token);
		return expr_init(E_TRUE_LIT, NULL, NULL, 0, NULL, 0, NULL);
	case T_FALSE:
		next(cur_token);
		return expr_init(E_FALSE_LIT, NULL, NULL, 0, NULL, 0, NULL);
	case T_CHAR_LIT:
		txt = cur->text;
		cur->text = NULL;
		next(cur_token);
		return expr_init(E_CHAR_LIT, NULL, NULL, 0, NULL, 0, txt);
	default:
		report_error_tok(*cur_token, "Could not parse expr unit. The offending token in question:");
		printf("\t");
		tok_print(*cur_token);
		sync_to(cur_token, T_EOF, 1);
		return NULL;
	}
}

ast_expr *build_cast(ast_expr *ex, type_t kind) {
	ast_expr *ret = expr_init(E_CAST, ex, NULL, 0, NULL, 0, NULL);
	ret->cast_to = kind;
	return ret;
}
