#include "error.h"
#include "scan.h"

#include <ctype.h>
#include <stdio.h>

static token_s *check_if_keyword(strvec *word, size_t line, size_t col)
{
	// this approach feels slow but oh well.
	token_s *ret = NULL;
	if (strvec_equals_str(word, "i32"))
		ret = tok_init_nl(T_I32, line, col, NULL);
	else if (strvec_equals_str(word, "let"))
		ret = tok_init_nl(T_LET, line, col, NULL);
	else if (strvec_equals_str(word, "if"))
		ret = tok_init_nl(T_IF, line, col, NULL);
	else if (strvec_equals_str(word, "true"))
		ret = tok_init_nl(T_TRUE, line, col, NULL);
	else if (strvec_equals_str(word, "false"))
		ret = tok_init_nl(T_FALSE, line, col, NULL);
	else if (strvec_equals_str(word, "i64"))
		ret = tok_init_nl(T_I64, line, col, NULL);
	else if (strvec_equals_str(word, "u32"))
		ret = tok_init_nl(T_U32, line, col, NULL);
	else if (strvec_equals_str(word, "u64"))
		ret = tok_init_nl(T_U64, line, col, NULL);
	else if (strvec_equals_str(word, "usize"))
		ret = tok_init_nl(T_USIZE, line, col, NULL);
	else if (strvec_equals_str(word, "const"))
		ret = tok_init_nl(T_CONST, line, col, NULL);
	else if (strvec_equals_str(word, "break"))
		ret = tok_init_nl(T_BREAK, line, col, NULL);
	else if (strvec_equals_str(word, "continue"))
		ret = tok_init_nl(T_CONTINUE, line, col, NULL);
	else if (strvec_equals_str(word, "else"))
		ret = tok_init_nl(T_ELSE, line, col, NULL);
	else if (strvec_equals_str(word, "void"))
		ret = tok_init_nl(T_VOID, line, col, NULL);
	else if (strvec_equals_str(word, "char"))
		ret = tok_init_nl(T_CHAR, line, col, NULL);
	else if (strvec_equals_str(word, "return"))
		ret = tok_init_nl(T_RETURN, line, col, NULL);
	else if (strvec_equals_str(word, "while"))
		ret = tok_init_nl(T_WHILE, line, col, NULL);
	else if (strvec_equals_str(word, "bool"))
		ret = tok_init_nl(T_BOOL, line, col, NULL);
	else if (strvec_equals_str(word, "struct"))
		ret = tok_init_nl(T_STRUCT, line, col, NULL);
	else if (strvec_equals_str(word, "asm"))
		ret = tok_init_nl(T_ASM, line, col, NULL);
	else if (strvec_equals_str(word, "sizeof"))
		ret = tok_init_nl(T_SIZEOF, line, col, NULL);
	else if (strvec_equals_str(word, "cast"))
		ret = tok_init_nl(T_CAST, line, col, NULL);
	else if (strvec_equals_str(word, "null"))
		ret = tok_init_nl(T_NULL, line, col, NULL);
	else if (strvec_equals_str(word, "proto"))
		ret = tok_init_nl(T_PROTO, line, col, NULL);
	if (ret != NULL)
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

// Assumes you come into this function having just scanned a backslash
// quote char for single or double quotes. this works for strings and chars.
static char try_escape_sequence(FILE *f, size_t *line, size_t *col, char quote_char, int *failed) {
	char c;
	(*col)++;
	c = fgetc(f);
	*failed = 0;
	if (c == 'n')
		return '\n';
	else if (c == '\\')
		return '\\';
	else if (c == '\'' && quote_char == '\'')
		return '\'';
	else if (c == '"' && quote_char == '"')
		return '"';
	else if (c == '0')
		return '\0';

	if (c == '\n')
		ungetc(c, f);

	report_error(*line, *col - 1, "Unrecognized escape character\n");
	*failed = 1;
	return 'E';
}

static token_s *scan_char_literal(FILE *f, size_t *line, size_t *col)
{
	int c;
	int c2;
	int fail_flag = 0;
	strvec *character = NULL;
	size_t old_col = *col;
	(*col)++;
	c = fgetc(f);
	if (c == '\n') {
		ungetc(c, f);
		report_error(*line, old_col, "Bad char literal. Missing close quote?\n");
		return tok_init_nl(T_ERROR, *line, old_col, NULL);
	} else if (c == '\\') {
		c = try_escape_sequence(f, line, col, '\'', &fail_flag);
	} else if (c == '\'') {
		(*col)++;
		report_error(*line, old_col, "Empty char literal.\n");
		return tok_init_nl(T_ERROR, *line, old_col, NULL);
	}
	(*col)++;
	c2 = fgetc(f);
	if (c2 != '\'') {
		ungetc(c2, f);
		report_error(*line, old_col, "Bad char literal. Missing close quote?\n");
		return tok_init_nl(T_ERROR, *line, old_col, NULL);
	}
	(*col)++;
	// Don't quit at fail flag first time around: also warn if missing close quote.
	if (fail_flag)
		return tok_init_nl(T_ERROR, *line, old_col, NULL);
	character = strvec_init(1);
	strvec_append(character, c);
	return tok_init_nl(T_CHAR_LIT, *line, old_col, character);
}

static token_s *scan_string_literal(FILE *f, size_t *line, size_t *col)
{
	int c;
	int fail_flag = 0;
	strvec *str = strvec_init(5);
	size_t old_col = *col;
	(*col)++;
	while ((c = fgetc(f)) != '"') {
		(*col)++;
		if (c == '\n') {
			ungetc(c, f);
			(*col)--;
			strvec_destroy(str);
			report_error(*line, old_col, "Unterminated string literal.\n");
			return tok_init_nl(T_ERROR, *line, old_col, NULL);
		}
		if (c == '\\')
			strvec_append(str, try_escape_sequence(f, line, col, '"', &fail_flag));
		else
			strvec_append(str, c);
	}
	(*col)++;
	if (fail_flag) {
		strvec_destroy(str);
		return tok_init_nl(T_ERROR, *line, old_col, NULL);
	}
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
		return tok_init_nl(T_EOF, *line, *col, NULL);
	case '\'':
		return scan_char_literal(f, line, col);
	case '"':
		return scan_string_literal(f, line, col);
	case '+':
		c = fgetc(f);
		if (c == '=') {
			*col += 2;
			return tok_init_nl(T_ADD_ASSIGN, *line, temp_col, NULL);
		}
		ungetc(c, f);
		return tok_init_nl(T_PLUS, *line, (*col)++, NULL);
	case '-':
		c = fgetc(f);
		if (c == '=') {
			*col += 2;
			return tok_init_nl(T_SUB_ASSIGN, *line, temp_col, NULL);
		} else if (c == '>') {
			*col += 2;
			return tok_init_nl(T_ARROW, *line, temp_col, NULL);
		} else if (isdigit(c)) {
			*col += 1;
			ungetc(c, f);
			return scan_number(1, f, line, col);
		}
		ungetc(c, f);
		return tok_init_nl(T_MINUS, *line, (*col)++, NULL);
	case '*':
		c = fgetc(f);
		if (c == '=') {
			*col += 2;
			return tok_init_nl(T_MUL_ASSIGN, *line, temp_col, NULL);
		}
		ungetc(c, f);
		return tok_init_nl(T_STAR, *line, (*col)++, NULL);
	case '/':
		c = fgetc(f);
		if (c == '=') {
			*col += 2;
			return tok_init_nl(T_DIV_ASSIGN, *line, temp_col, NULL);
		} else if (c == '/') {
			while ((c = fgetc(f)) != '\n' && c != EOF);
			ungetc(c, f);
			return scan_next_token(f, line, col);
		}
		ungetc(c, f);
		return tok_init_nl(T_FSLASH, *line, (*col)++, NULL);
	case '%':
		c = fgetc(f);
		if (c == '=') {
			*col += 2;
			return tok_init_nl(T_MOD_ASSIGN, *line, temp_col, NULL);
		}
		ungetc(c, f);
		return tok_init_nl(T_PERCENT, *line, (*col)++, NULL);
	case '<':
		c = fgetc(f);
		if (c == '=') {
			*col += 2;
			return tok_init_nl(T_LTE, *line, temp_col, NULL);
		} else if (c == '<') {
			*col += 2;
			return tok_init_nl(T_LSHIFT, *line, temp_col, NULL);
		}
		ungetc(c, f);
		return tok_init_nl(T_LT, *line, (*col)++, NULL);
	case '>':
		c = fgetc(f);
		if (c == '=') {
			*col += 2;
			return tok_init_nl(T_GTE, *line, temp_col, NULL);
		} else if (c == '>') {
			*col += 2;
			return tok_init_nl(T_RSHIFT, *line, temp_col, NULL);
		}
		ungetc(c, f);
		return tok_init_nl(T_GT, *line, (*col)++, NULL);
	case '=':
		c = fgetc(f);
		if (c == '=') {
			*col += 2;
			return tok_init_nl(T_EQ, *line, temp_col, NULL);
		}
		ungetc(c, f);
		return tok_init_nl(T_ASSIGN, *line, (*col)++, NULL);
	case '!':
		c = fgetc(f);
		if (c == '=') {
			*col += 2;
			return tok_init_nl(T_NEQ, *line, temp_col, NULL);
		}
		ungetc(c, f);
		return tok_init_nl(T_NOT, *line, (*col)++, NULL);
	case '^':
		c = fgetc(f);
		if (c == '=') {
			*col += 2;
			return tok_init_nl(T_XOR_ASSIGN, *line, temp_col, NULL);
		}
		ungetc(c, f);
		return tok_init_nl(T_XOR, *line, (*col)++, NULL);
	case '&':
		c = fgetc(f);
		if (c == '&') {
			*col += 2;
			return tok_init_nl(T_AND, *line, temp_col, NULL);
		} else if (c == '=') {
			*col += 2;
			return tok_init_nl(T_BW_AND_ASSIGN, *line, temp_col, NULL);
		}
		ungetc(c, f);
		return tok_init_nl(T_AMPERSAND, *line, (*col)++, NULL);
	case '|':
		c = fgetc(f);
		if (c == '|') {
			*col += 2;
			return tok_init_nl(T_OR, *line, temp_col, NULL);
		} else if (c == '=') {
			*col += 2;
			return tok_init_nl(T_BW_OR_ASSIGN, *line, temp_col, NULL);
		}
		ungetc(c, f);
		return tok_init_nl(T_BW_OR, *line, (*col)++, NULL);
	case '@':
		return tok_init_nl(T_AT, *line, (*col)++, NULL);
	case '.': // TODO: support floating pt literals like .5
		return tok_init_nl(T_PERIOD, *line, (*col)++, NULL);
	case '~':
		return tok_init_nl(T_BW_NOT, *line, (*col)++, NULL);
	case ':':
		return tok_init_nl(T_COLON, *line, (*col)++, NULL);
	case ';':
		return tok_init_nl(T_SEMICO, *line, (*col)++, NULL);
	case ',':
		return tok_init_nl(T_COMMA, *line, (*col)++, NULL);
	case '(':
		return tok_init_nl(T_LPAREN, *line, (*col)++, NULL);
	case ')':
		return tok_init_nl(T_RPAREN, *line, (*col)++, NULL);
	case '{':
		return tok_init_nl(T_LCURLY, *line, (*col)++, NULL);
	case '}':
		return tok_init_nl(T_RCURLY, *line, (*col)++, NULL);
	case '[':
		return tok_init_nl(T_LBRACKET, *line, (*col)++, NULL);
	case ']':
		return tok_init_nl(T_RBRACKET, *line, (*col)++, NULL);
	default:
		report_error(*line, *col, "Unrecognized token '%c'.\n", '#');
		return tok_init_nl(T_ERROR, *line, (*col)++, NULL);
	}
}

token_s *scan(FILE *f)
{
	token_s *head = NULL;
	token_s *cur = NULL;
	token_s *prev = NULL;
	size_t line = 1;
	size_t col = 1;
	while (1) {
		prev = cur;
		cur = scan_next_token(f, &line, &col);
		if (!head)
			head = cur;
		else {
			prev->next = cur;
		}
		if (cur->type == T_EOF)
			return head;
	}
}
