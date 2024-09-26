#include <stdio.h>
#include <stdlib.h>
#include "util.h"
#include "token.h"

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

void tok_print(token_s *t)
{
	if (t == NULL)
		return;
	tok_t_print(t->type);
	printf(" ");
	if (t->text)
		strvec_print(t->text);
	printf("\tLine %lu Col %lu (type %d)\n", t->line, t->col, t->type);
}

void tok_t_print(token_t t)
{
	switch (t) {
	case T_ERROR:
		printf("ERROR");
		return;
	case T_EOF:
		printf("EOF");
		break;
	case T_I32:
		printf("i32");
		break;
	case T_I64:
		printf("i64");
		break;
	case T_U32:
		printf("u32");
		break;
	case T_U64:
		printf("u64");
		break;
	case T_STRING:
		printf("string");
		break;
	case T_CONST:
		printf("const");
		break;
	case T_BREAK:
		printf("break");
		break;
	case T_CONTINUE:
		printf("continue");
		break;
	case T_ELSE:
		printf("else");
		break;
	case T_FOR:
		printf("for");
		break;
	case T_VOID:
		printf("void");
		break;
	case T_CHAR:
		printf("char");
		break;
	case T_IF:
		printf("if");
		break;
	case T_RETURN:
		printf("return");
		break;
	case T_WHILE:
		printf("while");
		break;
	case T_BOOL:
		printf("bool");
		break;
	case T_FALSE:
		printf("false");
		break;
	case T_TRUE:
		printf("true");
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
	default:
		printf("%d", t);
		break;
	}
}
