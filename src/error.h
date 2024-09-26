#ifndef ERROR_H
#define ERROR_H
#include <stddef.h>
#include "token.h"

void report_error(const char *msg, size_t line, size_t col);
void report_error_tok(const char *msg, token_s *t);

#endif
