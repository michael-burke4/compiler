#ifndef PARSE_H
#define PARSE_H

#include "ast.h"

//TODO: consistent noun_verb or verb_noun
ast_decl *parse_program(token_s *head_tok);
ast_decl *parse_decl(void);
ast_type *parse_type(void);
ast_expr *parse_expr(void);
ast_expr *parse_expr_addsub(void);
ast_expr *parse_expr_muldiv(void);
ast_expr *parse_expr_post_unary(void);
ast_expr *parse_expr_pre_unary(void);
ast_expr *parse_expr_unit(void);
ast_expr *parse_expr_equality(void);
ast_expr *parse_expr_shift(void);
ast_expr *parse_expr_inequality(void);
ast_expr *parse_expr_and(void);
ast_expr *parse_expr_or(void);
ast_expr *parse_expr_assign(void);
ast_expr *parse_expr_bw_xor(void);
ast_expr *parse_expr_bw_or(void);
ast_expr *parse_expr_bw_and(void);
ast_stmt *parse_stmt(void);
ast_stmt *parse_stmt_block(void);
ast_typed_symbol *parse_typed_symbol(void);
ast_expr *build_cast(ast_expr *ex, type_t kind);

#endif
