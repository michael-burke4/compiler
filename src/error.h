#ifndef ERROR_H
#define ERROR_H

#include "token.h"

#include <stddef.h>

void report_error(size_t line, size_t col, const char *fmt, ...);
void report_error_tok(token_s *t, const char *fmt, ...);
void report_error_line(size_t line, const char *fmt, ...);
void eputs(const char *s);

#endif
