#ifndef SCAN_H
#define SCAN_H

#include "token.h"

#include <stddef.h>
#include <stdio.h>

token_s *scan(FILE *f);
token_s *scan_next_token(FILE *f, size_t *line, size_t *col);
#endif
