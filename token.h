#ifndef TOKEN_H
#define TOKEN_H
#include "strvec.h"
typedef enum {
	T_ERROR = 0,
	T_WORD,
	T_CHAR,
	T_SQUOTE,
	T_DQUOTE,
	T_STAR,
	T_EOF
} token_t;

typedef struct token_s {
	token_t type;
	size_t line;
	size_t col;
	struct token_s *prev;
	struct token_s *next;
	strvec *text;
} token_s;

token_s *tok_init(token_t type, size_t line, size_t col, token_s *prev,
	token_s *next, strvec *text);
void tok_list_destroy(token_s *head);
void tok_destroy(token_s *tok);
void tok_setnext(token_s *cur, token_s *next);
token_s *tokenize(FILE *f);
void tok_print(token_s *token);
token_s *get_next_token(FILE *f, size_t *line, size_t *col);

#endif
