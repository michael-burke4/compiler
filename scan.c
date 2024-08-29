#include <ctype.h>
#include "error.h"
#include "scan.h"
#include <stdio.h>

static token_s *check_if_keyword(strvec *word, size_t line, size_t col)
{
	// this approach feels slow but oh well.
	token_s *ret = 0;
	if (strvec_equals_str(word, "i32"))
		ret = tok_init_nl(T_I32, line, col, 0);
	else if (strvec_equals_str(word, "let"))
		ret = tok_init_nl(T_LET, line, col, 0);
	else if (strvec_equals_str(word, "if"))
		ret = tok_init_nl(T_IF, line, col, 0);
	else if (strvec_equals_str(word, "true"))
		ret = tok_init_nl(T_TRUE, line, col, 0);
	else if (strvec_equals_str(word, "false"))
		ret = tok_init_nl(T_FALSE, line, col, 0);
	else if (strvec_equals_str(word, "i64"))
		ret = tok_init_nl(T_I64, line, col, 0);
	else if (strvec_equals_str(word, "u32"))
		ret = tok_init_nl(T_U32, line, col, 0);
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
	else if (strvec_equals_str(word, "return"))
		ret = tok_init_nl(T_RETURN, line, col, 0);
	else if (strvec_equals_str(word, "while"))
		ret = tok_init_nl(T_WHILE, line, col, 0);
	else if (strvec_equals_str(word, "bool"))
		ret = tok_init_nl(T_BOOL, line, col, 0);
	else if (strvec_equals_str(word, "syscall"))
		ret = tok_init_nl(T_SYSCALL, line, col, 0);
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
static token_s *scan_number(int negative, FILE *f, size_t *line, size_t *col)
{
	int c;
	strvec *num = strvec_init(5);
	size_t old_col = *col;
	if (negative)
		strvec_append(num, '-');
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
}

static token_s *scan_char_literal(FILE *f, size_t *line, size_t *col)
{
	int c;
	int c2;
	strvec *character = 0;
	size_t old_col = *col;
	(*col)++;
	c = fgetc(f);
	if (c == '\n') {
		ungetc(c, f);
		report_error("Bad char literal. Missing close quote?", *line, old_col);
		return tok_init_nl(T_ERROR, *line, old_col, 0);
	} else if (c == '\\') {
		*(col++);
		c = fgetc(f);
	}
	(*col)++;
	c2 = fgetc(f);
	if (c2 != '\'') {
		ungetc(c2, f);
		report_error("Bad char literal. Missing close quote?", *line, old_col);
		return tok_init_nl(T_ERROR, *line, old_col, 0);
	}
	character = strvec_init(1);
	strvec_append(character, c);
	(*col)++;
	return tok_init_nl(T_CHAR_LIT, *line, old_col, character);
}

static token_s *scan_string_literal(FILE *f, size_t *line, size_t *col)
{
	int c;
	strvec *str = strvec_init(5);
	size_t old_col = *col;
	(*col)++;
	while ((c = fgetc(f)) != '"') {
		(*col)++;
		if (c == '\n') {
			ungetc(c, f);
			(*col)--;
			strvec_destroy(str);
			report_error("Unterminated string literal.", *line, old_col);
			return tok_init_nl(T_ERROR, *line, old_col, 0);
		}
		if (c == '\\') {
			c = fgetc(f);
			if (c == 'n')
				strvec_append(str, '\n');
			else if (c == '"')
				strvec_append(str, '"');
			else {
				ungetc(c, f);
				(*col)--;
				strvec_destroy(str);
				report_error("Unsupported escape sequence.", *line, old_col);
				return tok_init_nl(T_ERROR, *line, old_col, 0);
			}
		} else
			strvec_append(str, c);
	}
	(*col)++;
	return tok_init_nl(T_STR_LIT, *line, old_col, str);
}

token_s *scan_next_token(FILE *f, size_t *line, size_t *col)
{
	int c;
	size_t temp_col;
	while (isspace(c = fgetc(f))) {
		if (c == '\n') {
			*col = 0;
			++(*line);
		}
		++(*col);
	}
	temp_col = *col;

	if (isalpha(c) || c == '_') {
		ungetc(c, f);
		return scan_word(f, line, col);
	} else if (isdigit(c)) {
		ungetc(c, f);
		return scan_number(0, f, line, col);
	}
	switch (c) {
	case EOF:
		return tok_init_nl(T_EOF, *line, *col, 0);
	case '\'':
		return scan_char_literal(f, line, col);
	case '"':
		return scan_string_literal(f, line, col);
	case '+':
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
	case '-':
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
		} else if (isdigit(c)) {
			*col += 1;
			ungetc(c, f);
			return scan_number(1, f, line, col);
		}
		ungetc(c, f);
		return tok_init_nl(T_MINUS, *line, (*col)++, 0);
	case '*':
		c = fgetc(f);
		if (c == '=') {
			*col += 2;
			return tok_init_nl(T_MUL_ASSIGN, *line, temp_col, 0);
		}
		ungetc(c, f);
		return tok_init_nl(T_STAR, *line, (*col)++, 0);
	case '/':
		c = fgetc(f);
		if (c == '=') {
			*col += 2;
			return tok_init_nl(T_DIV_ASSIGN, *line, temp_col, 0);
		}
		ungetc(c, f);
		return tok_init_nl(T_FSLASH, *line, (*col)++, 0);
	case '%':
		c = fgetc(f);
		if (c == '=') {
			*col += 2;
			return tok_init_nl(T_MOD_ASSIGN, *line, temp_col, 0);
		}
		ungetc(c, f);
		return tok_init_nl(T_PERCENT, *line, (*col)++, 0);
	case '<':
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
	case '>':
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
	case '=':
		c = fgetc(f);
		if (c == '=') {
			*col += 2;
			return tok_init_nl(T_EQ, *line, temp_col, 0);
		}
		ungetc(c, f);
		return tok_init_nl(T_ASSIGN, *line, (*col)++, 0);
	case '!':
		c = fgetc(f);
		if (c == '=') {
			*col += 2;
			return tok_init_nl(T_NEQ, *line, temp_col, 0);
		}
		ungetc(c, f);
		return tok_init_nl(T_NOT, *line, (*col)++, 0);
	case '&':
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
	case '|':
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
	case '.': // TODO: support floating pt literals like .5
		return tok_init_nl(T_PERIOD, *line, (*col)++, 0);
	case '~':
		return tok_init_nl(T_BW_NOT, *line, (*col)++, 0);
	case '?':
		return tok_init_nl(T_QMARK, *line, (*col)++, 0);
	case ':':
		return tok_init_nl(T_COLON, *line, (*col)++, 0);
	case ';':
		return tok_init_nl(T_SEMICO, *line, (*col)++, 0);
	case ',':
		return tok_init_nl(T_COMMA, *line, (*col)++, 0);
	case '(':
		return tok_init_nl(T_LPAREN, *line, (*col)++, 0);
	case ')':
		return tok_init_nl(T_RPAREN, *line, (*col)++, 0);
	case '{':
		return tok_init_nl(T_LCURLY, *line, (*col)++, 0);
	case '}':
		return tok_init_nl(T_RCURLY, *line, (*col)++, 0);
	case '[':
		return tok_init_nl(T_LBRACKET, *line, (*col)++, 0);
	case ']':
		return tok_init_nl(T_RBRACKET, *line, (*col)++, 0);
	default:
		report_error("Unrecognized token.", *line, *col);
		return tok_init_nl(T_ERROR, *line, (*col)++, 0);
	}
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
