#include "error.h"

#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>

int had_error = 0;


void report_error_tok(token_s *t, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	report_error(t->line, t->col, fmt, args);
	va_end(args);
}

void report_error(size_t line, size_t col, const char *fmt, ...)
{
	va_list args;
	had_error = 1;
	va_start(args, fmt);
	fprintf(stderr, "[line %lu col %lu] ", line, col);
	fprintf(stderr, fmt, args);
	fprintf(stderr, "\n");
	va_end(args);
}

void eputs(const char *s)
{
        fputs(s, stderr);
}
