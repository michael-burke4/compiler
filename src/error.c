#include "error.h"

#include <stdio.h>

extern int had_error;

void report_error(const char *msg, size_t line, size_t col)
{
	had_error = 1;
	printf("[line %lu col %lu] %s\n", line, col, msg);
}

void report_error_tok(const char *msg, token_s *t)
{
	report_error(msg, t->line, t->col);
}
