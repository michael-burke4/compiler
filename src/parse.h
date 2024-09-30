#ifndef PARSE_H
#define PARSE_H

#include "ast.h"

//TODO: consistent noun_verb or verb_noun
ast_decl *parse_program(token_s *head_tok);
ast_decl *parse_decl();
ast_type *parse_type();
ast_expr *parse_expr();
ast_expr *parse_expr_addsub();
ast_expr *parse_expr_muldiv();
ast_expr *parse_expr_post_unary();
ast_expr *parse_expr_pre_unary();
ast_expr *parse_expr_unit();
ast_expr *parse_expr_equality();
ast_expr *parse_expr_inequality();
ast_expr *parse_expr_assign();
ast_stmt *parse_stmt();
ast_stmt *parse_stmt_block();
ast_typed_symbol *parse_typed_symbol();
ast_expr *build_cast(ast_expr *ex, type_t kind);

#endif
