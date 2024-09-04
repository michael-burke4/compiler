#include "symbol_table.h"
#include "ast.h"
#include "util.h"
#include "stack.h"
#include "scope.h"

struct stack *sym_tab;

void st_destroy(void)
{
	scope *level = (scope *)stack_item_from_top(sym_tab, 0);

	while (level) {
		scope_destroy(level);
		stack_pop(sym_tab);
		level = (scope *)stack_item_from_top(sym_tab, 0);
	}

	free(sym_tab);
	sym_tab = 0;
}

void st_init(void)
{
	sym_tab = stack_init();
	stack_push(sym_tab, scope_init(8));
}

void scope_enter(void)
{
	scope *new_s = scope_init(8);
	stack_push(sym_tab, (void *)new_s);
}

void scope_exit(void)
{
	scope_destroy((scope *)stack_pop(sym_tab));
}

size_t scope_level(void)
{
	return stack_size(sym_tab);
}

void scope_bind(void *symbol, strvec *name)
{
	scope *top = (scope *)stack_item_from_top(sym_tab, 0);
	scope_insert(top, name, symbol);
}

void scope_bind_ts(ast_typed_symbol *symbol)
{
	scope_bind(symbol, symbol->symbol);
}

void *scope_lookup(strvec *name)
{
	int i = 0;
	void *found;
	scope *current = (scope *)stack_item_from_top(sym_tab, 0);
	while (current != 0) {
		if ((found = scope_get(current, name)))
			return found;
		i++;
		current = (scope *)stack_item_from_top(sym_tab, i);
	}
	return 0;
}

void *scope_lookup_current(strvec *name)
{
	scope *current = (scope *)stack_item_from_top(sym_tab, 0);
	return scope_get(current, name);
}

void scope_bind_return_type(ast_type *type)
{
	scope *top = (scope *)stack_item_from_top(sym_tab, 0);
	top->return_type = type;
}

ast_type *scope_get_return_type(void)
{
	scope *current = (scope *)stack_item_from_top(sym_tab, 0);
	return current->return_type;
}
