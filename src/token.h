#ifndef TOKEN_H
#define TOKEN_H

#include "util.h"

#include <stddef.h>

typedef enum {
	T_ERROR = 0,
	T_EOF,

	T_IDENTIFIER,

	T_I32,
	T_I64,
	T_U32,
	T_U64,
	T_STRING,
	T_CONST,
	T_BREAK,
	T_CONTINUE,
	T_ELSE,
	T_FOR,
	T_VOID,
	T_BOOL,
	T_CHAR,
	T_IF,
	T_RETURN,
	T_WHILE,
	T_LET,
	T_SYSCALL,
	T_STRUCT,

	T_DPLUS,
	T_DMINUS,

	T_PLUS,
	T_MINUS,
	T_STAR,
	T_FSLASH,
	T_PERCENT,

	T_LT,
	T_LTE,
	T_GT,
	T_GTE,
	T_EQ,
	T_NEQ,

	T_AND,
	T_OR,
	T_NOT,

	T_AMPERSAND,
	T_BW_OR,
	T_LSHIFT,
	T_RSHIFT,
	T_BW_NOT,
	T_XOR,

	T_ASSIGN,
	T_ADD_ASSIGN,
	T_SUB_ASSIGN,
	T_MUL_ASSIGN,
	T_DIV_ASSIGN,
	T_MOD_ASSIGN,
	T_BW_AND_ASSIGN,
	T_BW_OR_ASSIGN,

	T_QMARK,
	T_COLON,

	T_ARROW,
	T_SEMICO,
	T_COMMA,
	T_PERIOD,
	T_LPAREN,
	T_RPAREN,
	T_LCURLY,
	T_RCURLY,
	T_LBRACKET,
	T_RBRACKET,

	T_TRUE,
	T_FALSE,
	T_INT_LIT,
	T_STR_LIT,
	T_CHAR_LIT,
} token_t;

typedef struct token_s {
	token_t type;
	size_t line;
	size_t col;
	struct token_s *prev;
	struct token_s *next;
	strvec *text;
} token_s;

token_s *tok_init(token_t type, size_t line, size_t col, token_s *prev, token_s *next,
			strvec *text);
token_s *tok_init_nl(token_t type, size_t line, size_t col, strvec *text);
void tok_list_destroy(token_s *head);
void tok_destroy(token_s *tok);
void tok_setnext(token_s *cur, token_s *next);
void tok_print(token_s *token);
void tok_t_print(token_t t);
#endif
