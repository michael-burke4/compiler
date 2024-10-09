#include "token.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>

/**
 *	Like tok_init, but with no provided links to next/prev tokens.
 *	(links can still be set later)
 */
token_s *tok_init_nl(token_t type, size_t line, size_t col, strvec *text)
{
	return tok_init(type, line, col, NULL, NULL, text);
}

token_s *tok_init(token_t type, size_t line, size_t col, token_s *prev, token_s *next, strvec *text)
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

void fprint_tok(FILE *f, token_s *t)
{
	if (t == NULL)
		return;
	fprint_tok_t(f, t->type);
	fprintf(f, " ");
	if (t->text)
		fstrvec_print(f, t->text);
	fprintf(f, "\tLine %lu Col %lu (type %d)\n", t->line, t->col, t->type);
}

void fprint_tok_t(FILE *f, token_t t)
{
	switch (t) {
	case T_ERROR:
	case T_EOF:
		fputs("EOF", f);
		break;
	case T_I32:
		fprintf(f, "i32");
		break;
	case T_I64:
		fprintf(f, "i64");
		break;
	case T_U32:
		fprintf(f, "u32");
		break;
	case T_U64:
		fprintf(f, "u64");
		break;
	case T_CONST:
		fprintf(f, "const");
		break;
	case T_BREAK:
		fprintf(f, "break");
		break;
	case T_CONTINUE:
		fprintf(f, "continue");
		break;
	case T_ELSE:
		fprintf(f, "else");
		break;
	case T_FOR:
		fprintf(f, "for");
		break;
	case T_VOID:
		fprintf(f, "void");
		break;
	case T_CHAR:
		fprintf(f, "char");
		break;
	case T_IF:
		fprintf(f, "if");
		break;
	case T_RETURN:
		fprintf(f, "return");
		break;
	case T_WHILE:
		fprintf(f, "while");
		break;
	case T_BOOL:
		fprintf(f, "bool");
		break;
	case T_FALSE:
		fprintf(f, "false");
		break;
	case T_TRUE:
		fprintf(f, "true");
		break;
	case T_DPLUS:
		fprintf(f, "++");
		break;
	case T_DMINUS:
		fprintf(f, "--");
		break;
	case T_PLUS:
		fprintf(f, "+");
		break;
	case T_MINUS:
		fprintf(f, "-");
		break;
	case T_STAR:
		fprintf(f, "*");
		break;
	case T_FSLASH:
		fprintf(f, "/");
		break;
	case T_PERCENT:
		fprintf(f, "%%");
		break;
	case T_AT:
		fprintf(f, "@");
		break;
	case T_LT:
		fprintf(f, "<");
		break;
	case T_LTE:
		fprintf(f, "<=");
		break;
	case T_GT:
		fprintf(f, ">");
		break;
	case T_GTE:
		fprintf(f, ">=");
		break;
	case T_EQ:
		fprintf(f, "==");
		break;
	case T_NEQ:
		fprintf(f, "!=");
		break;
	case T_AND:
		fprintf(f, "&&");
		break;
	case T_OR:
		fprintf(f, "||");
		break;
	case T_NOT:
		fprintf(f, "!");
		break;
	case T_AMPERSAND:
		fprintf(f, "&");
		break;
	case T_BW_OR:
		fprintf(f, "|");
		break;
	case T_LSHIFT:
		fprintf(f, "<<");
		break;
	case T_RSHIFT:
		fprintf(f, ">>");
		break;
	case T_BW_NOT:
		fprintf(f, "~");
		break;
	case T_XOR:
		fprintf(f, "^");
		break;
	case T_ASSIGN:
		fprintf(f, "=");
		break;
	case T_ADD_ASSIGN:
		fprintf(f, "+=");
		break;
	case T_SUB_ASSIGN:
		fprintf(f, "-=");
		break;
	case T_MUL_ASSIGN:
		fprintf(f, "*=");
		break;
	case T_DIV_ASSIGN:
		fprintf(f, "/=");
		break;
	case T_MOD_ASSIGN:
		fprintf(f, "%%=");
		break;
	case T_QMARK:
		fprintf(f, "?");
		break;
	case T_COLON:
		fprintf(f, ":");
		break;
	case T_ARROW:
		fprintf(f, "->");
		break;
	case T_SEMICO:
		fprintf(f, ";");
		break;
	case T_COMMA:
		fprintf(f, ",");
		break;
	case T_LPAREN:
		fprintf(f, "(");
		break;
	case T_RPAREN:
		fprintf(f, ")");
		break;
	case T_LCURLY:
		fprintf(f, "{");
		break;
	case T_RCURLY:
		fprintf(f, "}");
		break;
	case T_LBRACKET:
		fprintf(f, "[");
		break;
	case T_RBRACKET:
		fprintf(f, "]");
		break;
	default:
		fprintf(f, "%d", t);
		break;
	}
}
