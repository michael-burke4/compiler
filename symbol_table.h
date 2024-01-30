#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H
#include "ast.h"

void st_init(void);
void scope_enter(void);
void scope_exit(void);
size_t scope_level(void);
void scope_bind(strvec *name, ast_typed_symbol *symbol);
ast_typed_symbol *scope_lookup(strvec *name);
ast_typed_symbol *scope_lookup_current(strvec *name);


#endif
