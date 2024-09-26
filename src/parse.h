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
ast_expr *parse_expr_post_unary(token_s **cur_token);
ast_expr *parse_expr_pre_unary(token_s **cur_token);
ast_expr *parse_expr_unit(token_s **cur_token);
ast_expr *parse_expr_equality(token_s **cur_token);
ast_expr *parse_expr_inequality(token_s **cur_token);
ast_expr *parse_expr_assign(token_s **cur_token);
ast_stmt *parse_stmt(token_s **cur_token);
ast_stmt *parse_stmt_block(token_s **cur_token);
ast_typed_symbol *parse_typed_symbol(token_s **cur_token);
ast_expr *build_cast(ast_expr *ex, type_t kind);

#endif
