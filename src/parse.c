#include "ast.h"
#include "error.h"
#include "parse.h"
#include "token.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

token_s *cur_token = NULL;
token_s *prev_token = NULL;

static inline void next(void)
{
	prev_token = cur_token;
	cur_token = cur_token->next;
}

static inline token_t cur_tok_type(void)
{
	if (cur_token == NULL) {
		return T_EOF;
	}
	return cur_token->type;
}

static inline int expect(token_t expected)
{
	return cur_tok_type() == expected;
}

static inline size_t cur_tok_line(void)
{
	return cur_token->line;
}

static void report_error_cur_tok(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	if (!cur_token)
		report_error(prev_token->line, prev_token->col, fmt, args);
	else
		report_error(cur_token->line, cur_token->col, fmt, args);
	va_end(args);
}

//static inline void put_back(token_s *t)
//{
//	t->next = cur_token;
//	cur_token = t;
//}

// Can search for a specific token to sync to. the or_newline enabled means the
// parser will consider itself synced upon reaching the specified target token
// type, OR a newline. If you just want to sync to a newline, put T_EOF as the
// target token.
//
// Parser is also considered synced at EOF, no matter the target.
static void sync_to(token_t target, int or_newline)
{
	if (cur_token == NULL)
		return;
	size_t prevline = cur_tok_line();
	while (!expect(target)) {
		if (or_newline && prevline != cur_tok_line())
			break;
		if (expect(T_EOF))
			break;
		next();
	}
}

static vect *parse_comma_separated_exprs(token_t closer) {
	vect *ret = NULL;
	next(); // consumes opening token
	ast_expr *cur;
	if (expect(closer)) {
		next();
		return ret;
	}
	ret = vect_init(4);
	do {
		if (ret->size != 0)
			next(); // we know cur_token is a comma from while condition.
		cur = parse_expr();
		if (cur != NULL)
			vect_append(ret, (void *)cur);
		else {
			destroy_expr_vect(ret);
			return NULL;
		}
	} while (expect(T_COMMA));
	if (!expect(closer)) {
		report_error_cur_tok("Expression list is missing closing token");
		destroy_expr_vect(ret);
		ret = NULL;
	}
	next();
	return ret;
}

ast_decl *parse_program(token_s *head)
{
	cur_token = head;
	if (expect(T_EOF))
		return NULL;
	ast_decl *ret = parse_decl();
	ret->next = parse_program(cur_token);
	return ret;
}

static vect *parse_arglist(int *had_arglist_error)
{
	vect *ret = NULL;
	int empty = 1;
	token_t typ;
	ast_typed_symbol *ts;
	while (1) {
		typ = cur_tok_type();
		if (typ == T_RPAREN) {
			next();
			return ret;
		} else if (empty) {
			ts = parse_typed_symbol();
			if (ts == NULL) {
				*had_arglist_error = 1;
				return NULL;
			}
			empty = 0;
			ret = vect_init(2);
			vect_append(ret, (void *)ts);
		} else if (typ == T_COMMA) {
			next();
			ts = parse_typed_symbol();
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

ast_typed_symbol *parse_typed_symbol(void)
{
	ast_type *type = NULL;
	strvec *name = NULL;

	if (!expect(T_IDENTIFIER))
		goto parse_typsym_err;
	name = cur_token->text;
	cur_token->text = NULL;
	next();

	if (!expect(T_COLON))
		goto parse_typsym_err;
	next();

	type = parse_type();
	if (type == NULL)
		goto parse_typsym_err;
	return ast_typed_symbol_init(type, name);

parse_typsym_err:
	fprint_tok(stderr, cur_token);
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

static vect *parse_struct_def(void)
{
	ast_typed_symbol *cur;
	vect *def_vect = vect_init(3);
	if (!expect(T_LCURLY)) {
		report_error_cur_tok("missing opening brace in struct definition");
		sync_to(T_SEMICO, 0);
		destroy_def_vect(def_vect);
		return NULL;
	}
	next();
	if (expect(T_RCURLY)) {
		report_error_cur_tok("struct definition can't be empty");
		sync_to(T_SEMICO, 0);
		next();
		destroy_def_vect(def_vect);
		return NULL;
	}
	while (!expect(T_RCURLY)) {
		cur = parse_typed_symbol();
		if (cur != NULL && !expect(T_SEMICO)) {
			report_error_cur_tok("missing semicolon in struct field definition");
			sync_to(T_EOF, 1);
		} else if (cur == NULL) {
			report_error_cur_tok("Couldn't parse typed symbol");
			sync_to(T_SEMICO, 1);
		} else
			next();
		vect_append(def_vect, cur);
	}
	next();
	if (!expect(T_SEMICO)) {
		report_error_cur_tok("missing semicolon after struct definition");
		sync_to(T_EOF, 1);
		vect_destroy(def_vect);
		return NULL;
	}
	next();
	return def_vect;
}

//TODO: error reporting for function declarations is REALLY bad right now.
ast_decl *parse_decl(void)
{
	ast_typed_symbol *typed_symbol = NULL;
	ast_expr *expr = NULL;
	ast_stmt *stmt = NULL;
	vect *initializer = NULL;
	ast_decl *ret = NULL;
	strvec *name;
	ast_type *type;
	if (expect(T_STRUCT)) {
		next();
		if (!expect(T_IDENTIFIER)) {
			report_error_cur_tok("Expected struct name after struct keyword");
			sync_to(T_LCURLY, 0);
		} else {
			type = type_init(Y_STRUCT, NULL);
			name = cur_token->text;
			cur_token->text = NULL;
			typed_symbol = ast_typed_symbol_init(type, name);
			next();
		}

		initializer = parse_struct_def();
		typed_symbol->type->arglist = initializer;
		ret = decl_init(typed_symbol, NULL, NULL, NULL);
		return ret;
	} else if (!expect(T_LET)) {
		report_error_cur_tok("Missing 'let' keyword in declaration.");
		sync_to(T_EOF, 1); // maybe this should be in the goto
		goto decl_parse_err;
	}
	next();
	typed_symbol = parse_typed_symbol();
	if (typed_symbol == NULL) {
		report_error_cur_tok("Missing/invalid type specifier or name.");
		sync_to(T_EOF, 1);
		goto decl_parse_err;
	}

	if (cur_tok_type() == T_SEMICO)
		next();
	else if (expect(T_ASSIGN)) {
		next();
		if (expect(T_LCURLY)) {
			stmt = parse_stmt_block();
		} else if (expect(T_LBRACKET)) {
			initializer = parse_comma_separated_exprs(T_RBRACKET);
		} else {
			expr = parse_expr();
		}
		if (!expect(T_SEMICO)) {
			report_error_cur_tok("Missing semicolon (possibly missing from previous line)");
			sync_to(T_EOF, 1);
			goto decl_parse_err;
		} else {
			next();
		}
	} else {
		report_error_cur_tok("Missing semicolon (maybe missing on previous line)");
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

ast_stmt *parse_stmt_block(void)
{
	ast_stmt *block = NULL;
	ast_stmt *current = NULL;
	if (!expect(T_LCURLY)) {
		return NULL;
	}
	block = stmt_init(S_BLOCK, NULL, NULL, NULL, NULL);
	block->next = NULL;
	next();
	while (!expect(T_RCURLY) && !expect(T_EOF)) {
		if (current == NULL) {
			block->body = parse_stmt();
			current = block->body;
		}
		else {
			current->next = parse_stmt();
			current = current->next;
		}
	}
	next();
	return block;
}
ast_stmt *parse_stmt(void)
{
	stmt_t kind;
	ast_decl *decl = NULL;
	ast_expr *expr = NULL;
	ast_stmt *body = NULL;
	ast_stmt *else_body = NULL;

	switch (cur_tok_type()) {
	case T_LET:
		kind = S_DECL;
		decl = parse_decl();
		break;
	case T_IF:
		kind = S_IFELSE;
		next();
		if (!expect(T_LPAREN)) {
			report_error_cur_tok("Missing left paren in `if` condition.");
			goto stmt_err;
		}
		next();
		expr = parse_expr();
		if (!expect(T_RPAREN)) {
			report_error_cur_tok("Missing right paren in `if` condition.");
			goto stmt_err;
		}
		next();
		body = parse_stmt_block();
		if (expect(T_ELSE)) {
			next();
			else_body = parse_stmt_block();
		}
		break;
	case T_WHILE:
		kind = S_WHILE;
		next();
		if (!expect(T_LPAREN)) {
			report_error_cur_tok("Missing left paren in `while` condition.");
			goto stmt_err;
		}
		next();
		expr = parse_expr();
		if (!expect(T_RPAREN)) {
			report_error_cur_tok("Missing right paren in `while` condition.");
			goto stmt_err;
		}
		next();
		body = parse_stmt_block();
		break;
	case T_RETURN:
		kind = S_RETURN;
		next();
		if (!expect(T_SEMICO)) {
			expr = parse_expr();
			if (expr == NULL) {
				report_error_cur_tok("Could not parse expression in return statement.");
				goto stmt_err;
			}
			if (!expect(T_SEMICO)) {
				report_error_cur_tok("Statement missing terminating semicolon a.");
				goto stmt_err;
			}
			next();
		}
		else
			next();
		break;
	default:
		kind = S_EXPR;
		expr = parse_expr();
		if (expr == NULL) {
			report_error_cur_tok("could not parse statement");
			goto stmt_err;
		}
		if (!expect(T_SEMICO)) {
			report_error_cur_tok("Statement missing terminating semicolon b.");
			goto stmt_err;
		}
		next();
	}
	return stmt_init(kind, decl, expr, body, else_body);
stmt_err:
	sync_to(T_EOF, 1);
	decl_destroy(decl);
	expr_destroy(expr);
	stmt_destroy(body);
	stmt_destroy(else_body);
	return stmt_init(S_ERROR, NULL, NULL, NULL, NULL);
}
//ast_stmt *stmt_init(stmt_t kind, ast_decl *decl, ast_expr *expr, ast_stmt *body,
//ast_stmt *else_body);

ast_type *parse_type(void)
{
	ast_type *ret = NULL;
	ast_type *subtype = NULL;
	vect *arglist = NULL;
	int had_arglist_err = 0;

	switch (cur_tok_type()) {
	case T_I64:
		ret = type_init(Y_I64, NULL);
		next();
		break;
	case T_I32:
		ret = type_init(Y_I32, NULL);
		next();
		break;
	case T_U64:
		ret = type_init(Y_U64, NULL);
		next();
		break;
	case T_U32:
		ret = type_init(Y_U32, NULL);
		next();
		break;
	case T_CHAR:
		ret = type_init(Y_CHAR, NULL);
		next();
		break;
	case T_VOID:
		ret = type_init(Y_VOID, NULL);
		next();
		break;
	case T_BOOL:
		ret = type_init(Y_BOOL, NULL);
		next();
		break;
	case T_STRUCT:
		ret = type_init(Y_STRUCT, NULL);
		next();
		if (!expect(T_IDENTIFIER)) {
			report_error_cur_tok("struct instances require names after the `struct` keyword");
			sync_to(T_ASSIGN, 1);
			type_destroy(ret);
			return NULL;
		}
		ret->name = cur_token->text;
		cur_token->text = NULL;
		next();
		break;
	case T_LPAREN:
		next();
		arglist = parse_arglist(&had_arglist_err);
		if (had_arglist_err) {
			report_error_cur_tok("Failed parsing argument list in function declaration.");
			sync_to(T_ASSIGN, 1);
		}
		if (!expect(T_ARROW)) {
			report_error_cur_tok("Missing arrow in function declaration.");
			sync_to(T_EOF, 1);
			break;
		}
		next();
		subtype = parse_type();
		if (subtype == NULL) {
			report_error_cur_tok("Missing/invalid return type in function declaration.");
			sync_to(T_ASSIGN, 1);
		}
		ret = type_init(Y_FUNCTION, NULL);
		ret->subtype = subtype;
		ret->arglist = arglist;
		break;
	default:
		report_error_cur_tok("Could not use the following token as a type:");
		fprintf(stderr, "\t");
		fprint_tok(stderr, cur_token);
		sync_to(T_EOF, 1);
	}
	while (cur_tok_type() == T_STAR) {
		subtype = ret;
		ret = type_init(Y_POINTER, NULL);
		ret->subtype = subtype;
		next();
	}
	return ret;
}

// Shoutouts to https://www.engr.mun.ca/~theo/Misc/exp_parsing.htm#classic
// for info on parsing binary expressions with recursive descent parsers :)
ast_expr *parse_expr(void)
{
	return parse_expr_assign();
}

ast_expr *parse_expr_assign(void)
{
	ast_expr *this = parse_expr_equality();
	ast_expr *that;
	token_t typ = cur_tok_type();
	token_t op;
	while (typ == T_ASSIGN || typ == T_ADD_ASSIGN || typ == T_SUB_ASSIGN ||
			typ == T_MUL_ASSIGN || typ == T_DIV_ASSIGN || typ == T_MOD_ASSIGN ||
			typ == T_BW_AND_ASSIGN || typ == T_BW_OR_ASSIGN) {
		op = typ;
		next();
		that = parse_expr_inequality();
		this = expr_init(E_ASSIGN, this, that, op, NULL, 0, NULL);
		typ = cur_tok_type();
	}
	return this;
}

ast_expr *parse_expr_equality(void)
{
	ast_expr *this = parse_expr_inequality();
	ast_expr *that;
	token_t typ = cur_tok_type();
	token_t op;
	while (typ == T_NEQ || typ == T_EQ) {
		op = typ;
		next();
		that = parse_expr_addsub();
		this = expr_init(E_EQUALITY, this, that, op, NULL, 0, NULL);
		typ = cur_tok_type();
	}
	return this;
}

ast_expr *parse_expr_inequality(void)
{
	ast_expr *this = parse_expr_addsub();
	ast_expr *that;
	token_t typ = cur_tok_type();
	token_t op;
	while (typ == T_LT || typ == T_LTE || typ == T_GT || typ == T_GTE) {
		op = typ;
		next();
		that = parse_expr_addsub();
		this = expr_init(E_INEQUALITY, this, that, op, NULL, 0, NULL);
		typ = cur_tok_type();
	}
	return this;
}

ast_expr *parse_expr_addsub(void)
{
	ast_expr *this = parse_expr_muldiv();
	ast_expr *that;
	token_t typ = cur_tok_type();
	token_t op;
	while (typ == T_MINUS || typ == T_PLUS) {
		op = typ;
		next();
		that = parse_expr_muldiv();
		this = expr_init(E_ADDSUB, this, that, op, NULL, 0, NULL);
		typ = cur_tok_type();
	}
	return this;
}

ast_expr *parse_expr_muldiv(void)
{
	ast_expr *this = parse_expr_pre_unary();
	ast_expr *that;
	token_t typ = cur_tok_type();
	token_t op;
	while (typ == T_STAR || typ == T_FSLASH || typ == T_PERCENT) {
		op = typ;
		next();
		that = parse_expr_pre_unary();
		this = expr_init(E_MULDIV, this, that, op, NULL, 0, NULL);
		typ = cur_tok_type();
	}
	return this;
}

ast_expr *parse_expr_pre_unary(void)
{
	ast_expr *inner = NULL;
	token_t typ = cur_tok_type();
	ast_expr *ret = NULL;

	switch (typ) {
	case T_DPLUS:
	case T_DMINUS:
	case T_MINUS:
	case T_NOT:
	case T_BW_NOT:
	case T_AMPERSAND:
		next();
		inner = parse_expr_pre_unary(); // TODO: is this correct? Do a lot of thinking.
		return expr_init(E_PRE_UNARY, inner, NULL, typ, NULL, 0, NULL);
	case T_STAR:
		next();
		inner = parse_expr_pre_unary();
		ret = expr_init(E_PRE_UNARY, inner, NULL, typ, NULL, 0, NULL);
		ret->is_lvalue = 1;
		return ret;
	default:
		return parse_expr_post_unary();
	}
}

// Now with member operator this isn't actually unary but w/e.
ast_expr *parse_expr_post_unary(void)
{
	ast_expr *inner = parse_expr_unit();
	ast_expr *e = NULL;
	token_t typ = cur_tok_type();
	// something like x++++; should parse but fail at typechecking. Could fail it now but w/e.
	// TODO: Make this look just like parse_expr_pre_unary
	while (typ == T_DPLUS || typ == T_DMINUS || typ == T_LBRACKET || typ == T_PERIOD) {
		next();
		if (typ == T_LBRACKET) {
			e = parse_expr();
			if (!expect(T_RBRACKET)) {
				report_error_cur_tok("Missing closing bracket");
				expr_destroy(e);
				return NULL;
			}
			next();
		} else if (typ == T_PERIOD) {
			if (!expect(T_IDENTIFIER)) {
				report_error_cur_tok("Member operator right side must be an identifier");
				return NULL;
			}
			e = parse_expr_unit(); // This can only be an expr unit: we know it's an identifier.
		}
		inner = expr_init(E_POST_UNARY, inner, NULL, typ, NULL, 0, NULL);
		inner->right = e;
		inner->is_lvalue = 1;
		typ = cur_tok_type();
	}
	return inner;
}


ast_expr *parse_expr_unit(void)
{
	token_t typ = cur_tok_type();
	token_s *cur = cur_token;
	strvec *txt;
	ast_expr *ex = NULL;
	ast_expr *ret;
	//token_t op;
	switch (typ) {
	case T_LPAREN:
		next();
		ex = parse_expr();
		if (cur_tok_type() != T_RPAREN) {
			// TODO leave error handling to fns like parse_stmt and parse_decl.
			// Just bubble the error up by returning zero.
			report_error_cur_tok("Expression is missing a closing paren");
			sync_to(T_EOF, 1);
			expr_destroy(ex);
			return NULL;
		}
		next();
		ret = expr_init(E_PAREN, ex, NULL, 0, NULL, 0, NULL);
		ret->is_lvalue = ex->is_lvalue;
		return ret;
	case T_INT_LIT:
		next();
		int64_t n;
		n = strvec_tol(cur->text);
		if (errno != 0) {
			report_error_cur_tok("Could not parse int literal \"");
			strvec_print(cur->text);
			eputs("\"");
		}
		ex = expr_init(E_INT_LIT, NULL, NULL, 0, NULL, n, NULL);
		ex->int_size = smallest_fit(n);
		return ex;
	case T_STR_LIT:
		txt = cur->text;
		cur->text = NULL;
		next();
		return expr_init(E_STR_LIT, NULL, NULL, 0, NULL, 0, txt);
	case T_SYSCALL:
		next();
		if (!expect(T_LPAREN)) {
			report_error_cur_tok("Missing open paren after `syscall`");
			sync_to(T_EOF, 1);
			return expr_init(E_SYSCALL, NULL, NULL, 0, NULL, 0, NULL);
		}
		ex = expr_init(E_SYSCALL, NULL, NULL, 0, NULL, 0, NULL);
		ex->sub_exprs = parse_comma_separated_exprs(T_RPAREN);
		return ex;
	case T_IDENTIFIER:
		txt = cur->text;
		cur->text = NULL;
		next();
		if (expect(T_LPAREN)) {
			ex = expr_init(E_FNCALL, NULL, NULL, 0, txt, 0, NULL);
			ex->sub_exprs = parse_comma_separated_exprs(T_RPAREN);
			return ex;
		} else {
			ex = expr_init(E_IDENTIFIER, NULL, NULL, 0, txt, 0, NULL);
			ex->is_lvalue = 1;
			return ex;
		}
	case T_TRUE:
		next();
		return expr_init(E_TRUE_LIT, NULL, NULL, 0, NULL, 0, NULL);
	case T_FALSE:
		next();
		return expr_init(E_FALSE_LIT, NULL, NULL, 0, NULL, 0, NULL);
	case T_CHAR_LIT:
		txt = cur->text;
		cur->text = NULL;
		next();
		return expr_init(E_CHAR_LIT, NULL, NULL, 0, NULL, 0, txt);
	default:
		report_error_cur_tok("Could not parse expr unit. The offending token in question:");
		fprintf(stderr, "\t");
		fprint_tok(stderr, cur_token);
		sync_to(T_EOF, 1);
		return NULL;
	}
}

ast_expr *build_cast(ast_expr *ex, type_t kind) {
	ast_expr *ret = expr_init(E_CAST, ex, NULL, 0, NULL, 0, NULL);
	ret->cast_to = kind;
	return ret;
}
