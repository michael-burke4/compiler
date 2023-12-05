#include <stdio.h>
#include <stdlib.h>
#include "strvec.h"
#include "token.h"

/**
 *	Like tok_init, but with no provided links to next/prev tokens.
 *	(links can still be set later)
 */
token_s *tok_init_nl(token_t type, size_t line, size_t col, strvec *text)
{
	return tok_init(type, line, col, 0, 0, text);
}

token_s *tok_init(token_t type, size_t line, size_t col, token_s *prev,
	token_s *next, strvec *text)
{
	token_s *ret = malloc(sizeof(*ret));
	ret->type = type;
	ret->line = line;
	ret->col = col;
	ret->prev = prev;
	ret->next = next;
	ret->text = text;
	return ret;

}
void tok_list_destroy(token_s *head)
{
	token_s *tmp;
	while (head) {
		tmp = head->next;
		tok_destroy(head);
		head = tmp;
	}
}

void tok_destroy(token_s *tok)
{
	strvec_destroy(tok->text);
	free(tok);
}

void tok_setnext(token_s *cur, token_s *next)
{
	cur->next = next;
}

void tok_print(token_s *t)
{
	if (!t)
		return;
	switch (t->type) {
	case T_ERROR:
		printf("Error");
		break;
	case T_EOF:
		printf("EOF");
		break;
	case T_STAR:
		printf("*");
		break;
	case T_PLUS:
		printf("+");
		break;
	case T_DPLUS:
		printf("++");
		break;
	case T_MINUS:
		printf("-");
		break;
	case T_DMINUS:
		printf("--");
		break;
	default:
		printf("UNSUPPORTED TOKEN");
		break;
	}
	printf(" Line %lu Col %lu\n", t->line, t->col);
}

token_s *get_next_token(FILE *f, size_t *line, size_t *col)
{
	int c = fgetc(f);
	size_t temp_col = *col;
	if (c == '\n') {
		ungetc(c, f);
		while ((c = fgetc(f)) == '\n') {
			(*line) += 1;
			*col = 0;
		}
	}
	if (c == EOF)
		return tok_init_nl(T_EOF, *line, *col, 0);
	else if (c == '+') {
		c = fgetc(f);
		if (c == '+') {
			*col += 2;
			return tok_init_nl(T_DPLUS, *line, temp_col, 0);
		}
		ungetc(c, f);
		return tok_init_nl(T_PLUS, *line, (*col)++, 0);
	}
	else if (c == '-') {
		c = fgetc(f);
		if (c == '-') {
			*col += 2;
			return tok_init_nl(T_DMINUS, *line, temp_col, 0);
		}
		ungetc(c, f);
		return tok_init_nl(T_MINUS, *line, (*col)++, 0);
	}
	else if (c == '*')
		return tok_init_nl(T_STAR, *line, (*col)++, 0);
	return tok_init(T_ERROR, *line, *col, 0, 0, 0);
}

token_s *tokenize(FILE *f)
{
	token_s *head = 0;
	token_s *cur = 0;
	token_s *prev = 0;
	size_t line = 1;
	size_t col = 1;
	while (1) {
		prev = cur;
		cur = get_next_token(f, &line, &col);
		if (!head)
			head = cur;
		if (head != cur) {
			prev->next = cur;
			cur->prev = prev;
		}
		if (cur->type == T_EOF)
			return head;
	}
}
