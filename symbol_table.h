#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H
#include "ast.h"
#include "ht.h"

void st_init(void);
void st_destroy();
void st_level_destroy(struct ht *level);
void scope_enter(void);
void scope_exit(void);
size_t scope_level(void);
void scope_bind(ast_typed_symbol *symbol);
ast_typed_symbol *scope_lookup(strvec *name);
ast_typed_symbol *scope_lookup_current(strvec *name);

#endif
