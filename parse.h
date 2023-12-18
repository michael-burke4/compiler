#ifndef PARSE_H
#define PARSE_H
#include "ast.h"

//TODO: consistent noun_verb or verb_noun
ast_decl *parse_program(token_s **cur_token);
ast_decl *parse_decl(token_s **cur_token);
ast_type *parse_type(token_s **cur_token);
ast_expr *parse_expr(token_s **cur_token);
ast_expr *parse_expr_addsub(token_s **cur_token);
ast_expr *parse_expr_muldiv(token_s **cur_token);
ast_expr *parse_expr_unit(token_s **cur_token);
ast_expr *parse_expr_comparison(token_s **cur_token);

#endif
