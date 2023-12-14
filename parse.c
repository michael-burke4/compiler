#include "ast.h"
#include "parse.h"
#include "token.h"
#include <stdlib.h>
#include <stdio.h>

static void next(token_s **cur_token)
{
	*cur_token = (*cur_token)->next;
}

static token_t get_type(token_s **cur_token)
{
	return (*cur_token)->type;
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
	ast_type *type;
	strvec *name;
	ast_expr *expr;

	type = parse_type(cur_token);

	if (get_type(cur_token) != T_IDENTIFIER) {
		printf("didn't get identifier");
		exit(1); // TODO: smart error recovering stuff.
	}
	name = (*cur_token)->text; // Steal the strvec from the token >:)
	(*cur_token)->text = 0;
	next(cur_token);
	

	if (get_type(cur_token) == T_SEMICO) {
		next(cur_token);
		expr = 0;
	} else if (get_type(cur_token) == T_ASSIGN) {
		next(cur_token);
		expr = parse_expr(cur_token);
		if (get_type(cur_token) != T_SEMICO) {
			printf("Missing semicolon after declaration");
			exit(1);
		}
		next(cur_token);
	} else {
		printf("Got an unrecognized token while trying to parse a decl: ");
		tok_print(*cur_token);
		printf("\n");

	}
	return decl_init(type, name, expr, 0);
}


ast_expr *parse_expr(token_s **cur_token)
{
	ast_expr *ret = 0;
	switch (get_type(cur_token)) {
	case T_LPAREN:
		next(cur_token);
		ret = parse_expr_paren(cur_token);
		break;
	case T_INT_LIT:
		// This could feasibly crash and break everything.
		ret = expr_init(E_INT_LIT, 0, 0, 0, 0, strvec_toi((*cur_token)->text), 0);
		next(cur_token);
		break;
	default:
		printf("Currently unsupported token when trying to parse an expr : ");
		tok_print(*cur_token);
		printf("\n");
		exit(1);
	}
	return ret;
}

ast_expr *parse_expr_paren(token_s **cur_token)
{
	ast_expr *left = parse_expr(cur_token);
	if (get_type(cur_token) != T_RPAREN) {
		printf("Missing closing paren?\n");
		exit(1);
	}
	next(cur_token);
	return expr_init(E_PAREN, left, 0, 0, 0, 0, 0);
}


ast_type *parse_type(token_s **cur_token)
{
	ast_type *ret;
	switch (get_type(cur_token)) {
	case T_I32:
		ret = type_init(get_type(cur_token));
		next(cur_token);
		break;
	default:
		printf("could not parse unrecognized type");
		tok_print(*cur_token);
		printf("\n");
		exit(1);
	}
	return ret;
}
