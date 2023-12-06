#include <ctype.h>
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
	case T_DPLUS:
		printf("++");
		break;
	case T_DMINUS:
		printf("--");
		break;
	case T_PLUS:
		printf("+");
		break;
	case T_MINUS:
		printf("-");
		break;
	case T_STAR:
		printf("*");
		break;
	case T_FSLASH:
		printf("/");
		break;
	case T_PERCENT:
		printf("%%");
		break;
	case T_LT:
		printf("<");
		break;
	case T_LTE:
		printf("<=");
		break;
	case T_GT:
		printf(">");
		break;
	case T_GTE:
		printf(">=");
		break;
	case T_EQ:
		printf("==");
		break;
	case T_NEQ:
		printf("!=");
		break;
	case T_AND:
		printf("&&");
		break;
	case T_OR:
		printf("||");
		break;
	case T_NOT:
		printf("!");
		break;
	case T_AMPERSAND:
		printf("&");
		break;
	case T_BW_OR:
		printf("|");
		break;
	case T_LSHIFT:
		printf("<<");
		break;
	case T_RSHIFT:
		printf(">>");
		break;
	case T_BW_NOT:
		printf("~");
		break;
	case T_XOR:
		printf("^");
		break;
	case T_ASSIGN:
		printf("=");
		break;
	case T_ADD_ASSIGN:
		printf("+=");
		break;
	case T_SUB_ASSIGN:
		printf("-=");
		break;
	case T_MUL_ASSIGN:
		printf("*=");
		break;
	case T_DIV_ASSIGN:
		printf("/=");
		break;
	case T_MOD_ASSIGN:
		printf("%%=");
		break;
	case T_QMARK:
		printf("?");
		break;
	case T_COLON:
		printf(":");
		break;
	case T_ARROW:
		printf("->");
		break;
	case T_SEMICO:
		printf(";");
		break;
	case T_COMMA:
		printf(",");
		break;
	case T_LPAREN:
		printf("(");
		break;
	case T_RPAREN:
		printf(")");
		break;
	case T_LCURLY:
		printf("{");
		break;
	case T_RCURLY:
		printf("}");
		break;
	case T_LBRACKET:
		printf("[");
		break;
	case T_RBRACKET:
		printf("]");
		break;
	case T_IDENTIFIER:
		strvec_print(t->text);
		break;
	default:
		printf("%d", t->type);
		break;
	}
	printf(" Line %lu Col %lu\n", t->line, t->col);
}

static token_s *scan_word(FILE *f, size_t *line, size_t *col)
{
	int c;
	strvec *word = strvec_init(5);
	size_t old_col = *col;
	while (1) {
		c = fgetc(f);
		if (!(isalpha(c) || c == '_' || isdigit(c)))
			break;
		++(*col);
		strvec_append(word, c);

	}
	ungetc(c, f);
	//strvec_print(word);
	return tok_init_nl(T_IDENTIFIER, *line, old_col, word);
	return 0;
}

token_s *get_next_token(FILE *f, size_t *line, size_t *col)
{
	int c;
	size_t temp_col;
	while (isspace(c = fgetc(f))) {
		if (c == '\n') {
			++(*line);
			(*col) = 1;
		} else
			++(*col);
	}
	temp_col = *col;
	if (c == EOF)
		return tok_init_nl(T_EOF, *line, *col, 0);
	else if (isalpha(c) || c == '_') {
		ungetc(c, f);
		return scan_word(f, line, col);
	} else if (c == '+') {
		c = fgetc(f);
		if (c == '+') {
			*col += 2;
			return tok_init_nl(T_DPLUS, *line, temp_col, 0);
		} else if (c == '=') {
			*col += 2;
			return tok_init_nl(T_ADD_ASSIGN, *line, temp_col, 0);
		}
		ungetc(c, f);
		return tok_init_nl(T_PLUS, *line, (*col)++, 0);
	} else if (c == '-') {
		c = fgetc(f);
		if (c == '-') {
			*col += 2;
			return tok_init_nl(T_DMINUS, *line, temp_col, 0);
		} else if (c == '=') {
			*col += 2;
			return tok_init_nl(T_SUB_ASSIGN, *line, temp_col, 0);
		} else if (c == '>') {
			*col += 2;
			return tok_init_nl(T_ARROW, *line, temp_col, 0);
		}
		ungetc(c, f);
		return tok_init_nl(T_MINUS, *line, (*col)++, 0);
	} else if (c == '*') {
		c = fgetc(f);
		if (c == '=') {
			*col += 2;
			return tok_init_nl(T_MUL_ASSIGN, *line, temp_col, 0);
		}
		ungetc(c, f);
		return tok_init_nl(T_STAR, *line, (*col)++, 0);
	} else if (c == '/') {
		c = fgetc(f);
		if (c == '=') {
			*col += 2;
			return tok_init_nl(T_DIV_ASSIGN, *line, temp_col, 0);
		}
		ungetc(c, f);
		return tok_init_nl(T_FSLASH, *line, (*col)++, 0);
	} else if (c == '%') {
		c = fgetc(f);
		if (c == '=') {
			*col += 2;
			return tok_init_nl(T_MOD_ASSIGN, *line, temp_col, 0);
		}
		ungetc(c, f);
		return tok_init_nl(T_PERCENT, *line, (*col)++, 0);
	} else if (c == '<') {
		c = fgetc(f);
		if (c == '=') {
			*col += 2;
			return tok_init_nl(T_LTE, *line, temp_col, 0);
		} else if (c == '<') {
			*col += 2;
			return tok_init_nl(T_LSHIFT, *line, temp_col, 0);
		}
		ungetc(c, f);
		return tok_init_nl(T_LT, *line, (*col)++, 0);
	} else if (c == '>') {
		c = fgetc(f);
		if (c == '=') {
			*col += 2;
			return tok_init_nl(T_GTE, *line, temp_col, 0);
		} else if (c == '>') {
			*col += 2;
			return tok_init_nl(T_RSHIFT, *line, temp_col, 0);
		}
		ungetc(c, f);
		return tok_init_nl(T_GT, *line, (*col)++, 0);
	} else if (c == '=') {
		c = fgetc(f);
		if (c == '=') {
			*col += 2;
			return tok_init_nl(T_EQ, *line, temp_col, 0);
		}
		ungetc(c, f);
		return tok_init_nl(T_ASSIGN, *line, (*col)++, 0);
	} else if (c == '!') {
		c = fgetc(f);
		if (c == '=') {
			*col += 2;
			return tok_init_nl(T_NEQ, *line, temp_col, 0);
		}
		ungetc(c, f);
		return tok_init_nl(T_NOT, *line, (*col)++, 0);
	} else if (c == '&') {
		c = fgetc(f);
		if (c == '&') {
			*col += 2;
			return tok_init_nl(T_AND, *line, temp_col, 0);
		} else if (c == '=') {
			*col += 2;
			return tok_init_nl(T_BW_AND_ASSIGN, *line, temp_col, 0);
		}
		ungetc(c, f);
		return tok_init_nl(T_AMPERSAND, *line, (*col)++, 0);
	} else if (c == '|') {
		c = fgetc(f);
		if (c == '|') {
			*col += 2;
			return tok_init_nl(T_OR, *line, temp_col, 0);
		} else if (c == '=') {
			*col += 2;
			return tok_init_nl(T_BW_OR_ASSIGN, *line, temp_col, 0);
		}
		ungetc(c, f);
		return tok_init_nl(T_BW_OR, *line, (*col)++, 0);
	}
	else if (c == '?')
		return tok_init_nl(T_QMARK, *line, (*col)++, 0);
	else if (c == ':')
		return tok_init_nl(T_COLON, *line, (*col)++, 0);
	else if (c == ';')
		return tok_init_nl(T_SEMICO, *line, (*col)++, 0);
	else if (c == ',')
		return tok_init_nl(T_COMMA, *line, (*col)++, 0);
	else if (c == '(')
		return tok_init_nl(T_LPAREN, *line, (*col)++, 0);
	else if (c == ')')
		return tok_init_nl(T_RPAREN, *line, (*col)++, 0);
	else if (c == '{')
		return tok_init_nl(T_LCURLY, *line, (*col)++, 0);
	else if (c == '}')
		return tok_init_nl(T_RCURLY, *line, (*col)++, 0);
	else if (c == '[')
		return tok_init_nl(T_LBRACKET, *line, (*col)++, 0);
	else if (c == ']')
		return tok_init_nl(T_RBRACKET, *line, (*col)++, 0);
	return tok_init(T_ERROR, *line, (*col)++, 0, 0, 0);
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
