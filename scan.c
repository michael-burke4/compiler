#include <ctype.h>
#include "scan.h"
#include <stdio.h>

static token_s *check_if_keyword(strvec *word, size_t line, size_t col)
{
	// this approach feels slow but oh well.
	token_s *ret = 0;
	if (strvec_equals_str(word, "i32"))
		ret = tok_init_nl(T_I32, line, col, 0);
	else if (strvec_equals_str(word, "i64"))
		ret = tok_init_nl(T_I64, line, col, 0);
	else if (strvec_equals_str(word, "u32"))
		ret = tok_init_nl(T_U64, line, col, 0);
	else if (strvec_equals_str(word, "u64"))
		ret = tok_init_nl(T_U64, line, col, 0);
	else if (strvec_equals_str(word, "string"))
		ret = tok_init_nl(T_STRING, line, col, 0);
	else if (strvec_equals_str(word, "const"))
		ret = tok_init_nl(T_CONST, line, col, 0);
	else if (strvec_equals_str(word, "break"))
		ret = tok_init_nl(T_BREAK, line, col, 0);
	else if (strvec_equals_str(word, "continue"))
		ret = tok_init_nl(T_CONTINUE, line, col, 0);
	else if (strvec_equals_str(word, "else"))
		ret = tok_init_nl(T_ELSE, line, col, 0);
	else if (strvec_equals_str(word, "for"))
		ret = tok_init_nl(T_FOR, line, col, 0);
	else if (strvec_equals_str(word, "void"))
		ret = tok_init_nl(T_VOID, line, col, 0);
	else if (strvec_equals_str(word, "char"))
		ret = tok_init_nl(T_CHAR, line, col, 0);
	else if (strvec_equals_str(word, "if"))
		ret = tok_init_nl(T_IF, line, col, 0);
	else if (strvec_equals_str(word, "return"))
		ret = tok_init_nl(T_RETURN, line, col, 0);
	else if (strvec_equals_str(word, "while"))
		ret = tok_init_nl(T_WHILE, line, col, 0);
	if (ret != 0)
		strvec_destroy(word);
	else
		ret = tok_init_nl(T_IDENTIFIER, line, col, word);
	return ret;
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
	return check_if_keyword(word, *line, old_col);
}

static token_s *scan_number(FILE *f, size_t *line, size_t *col)
{
	int c;
	strvec *num = strvec_init(5);
	size_t old_col = *col;
	while (1) {
		c = fgetc(f);
		// TODO: Support float literals
		if (!(isdigit(c)))
			break;
		++(*col);
		strvec_append(num, c);
	}
	ungetc(c, f);
	return tok_init_nl(T_INT_LIT, *line, old_col, num);
	return 0;
}

token_s *scan_next_token(FILE *f, size_t *line, size_t *col)
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
	} 
	else if (isdigit(c)) {
		ungetc(c, f);
		return scan_number(f, line, col);
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
	} else if (c == '.') // TODO: support floating pt literals like .5
		return tok_init_nl(T_PERIOD, *line, (*col)++, 0);
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

token_s *scan(FILE *f)
{
	token_s *head = 0;
	token_s *cur = 0;
	token_s *prev = 0;
	size_t line = 1;
	size_t col = 1;
	while (1) {
		prev = cur;
		cur = scan_next_token(f, &line, &col);
		if (!head)
			head = cur;
		else {
			prev->next = cur;
			cur->prev = prev;
		}
		if (cur->type == T_EOF)
			return head;
	}
}
