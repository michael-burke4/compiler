#ifndef SCAN_H
#define SCAN_H
#include <stddef.h>
#include <stdio.h>
#include "token.h"

token_s *scan(FILE *f);
token_s *scan_next_token(FILE *f, size_t *line, size_t *col);
#endif
