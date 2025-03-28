#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

#include "ast.h"
#include "scope.h"

void st_init(void);
void st_destroy(void);
void st_level_destroy(scope *level);
void scope_enter(void);
void scope_exit(void);
void scope_bind(void *symbol, strvec *name);
void scope_bind_ts(ast_typed_symbol *symbol);
void scope_bind_return_type(ast_type *type);
ast_type *scope_get_return_type(void);
void *scope_lookup(strvec *name);
void *scope_lookup_current(strvec *name);

#endif
