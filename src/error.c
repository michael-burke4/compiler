#include "error.h"

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

void vreport_error(size_t line, size_t col, const char *fmt, va_list args)
{
	had_error = 1;
	fprintf(stderr, "[line %lu col %lu] ", line, col);
	vfprintf(stderr, fmt, args);
}

void report_error(size_t line, size_t col, const char *fmt, ...)
{
	va_list args;
	had_error = 1;
	fprintf(stderr, "[line %lu col %lu] ", line, col);
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
}

void report_error_line(size_t line, const char *fmt, ...)
{
	va_list args;
	had_error = 1;
	fprintf(stderr, "[line %lu] ", line);
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
}

void eputs(const char *s)
{
	fputs(s, stderr);
	fputs("\n", stderr);
}
