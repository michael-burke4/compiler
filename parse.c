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

	if (get_type(cur_token) != T_SEMICO)
		exit(1);
	next(cur_token);
	expr = 0;
	return decl_init(type, name, expr, 0);
}

ast_type *parse_type(token_s **cur_token)
{
	ast_type *ret;
	if (get_type(cur_token) != T_I32) {
		printf("didn't get i32\n");
		exit(1);
	}
	ret = type_init(get_type(cur_token));
	next(cur_token);
	return ret;
}
