#ifndef ERROR_H
#define ERROR_H

#include "token.h"

#include <stdarg.h>
#include <stddef.h>

void report_error(size_t line, size_t col, const char *fmt, ...);
void vreport_error(size_t line, size_t col, const char *fmt, va_list args);
void report_error_tok(token_s *t, const char *fmt, ...);
void report_error_line(size_t line, const char *fmt, ...);
void vreport_error_line(size_t line, const char *fmt, va_list args);
void eputs(const char *s);

#endif
