#ifndef SCOPE_H
#define SCOPE_H
#include "ht.h"
#include "ast.h"

typedef struct scope {
	struct ht *table;
	ast_type *return_type;
} scope;

struct scope *scope_init(size_t capacity);
int scope_insert(struct scope *s, strvec *key, ast_typed_symbol *value);
ast_typed_symbol *scope_get(struct scope *s, strvec *key);
void scope_destroy(struct scope *s);
#endif
