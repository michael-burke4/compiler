#include "symbol_table.h"
#include "ast.h"
#include "util.h"
#include "stack.h"
#include "scope.h"

struct stack *sym_tab;

void st_level_destroy(struct scope *level)
{
	size_t i = 0;
	if (!level)
		return;

	for (i = 0; i < level->table->capacity; ++i) {
		if (level->table->data[i]) {
			// THE SYMBOL TABLE DOES NOT OWN THIS TYPED SYMBOL.
			// THIS BELONGS TO THE AST.
			// I REPEAT.
			// THE SYMBOL TABLE DOES NOT OWN THIS TYPED SYMBOL!
			// DON'T TRY TO FREE IT! IT IS ALREADY FREED IN FREEING THE AST!!!!
			// DONT UNCOMMENT THE FOLLOWING TWO LINES!
			//ast_typed_symbol_destroy(
			//	(ast_typed_symbol *)level->data[i]->val); // val holds type
			free(level->table->data[i]); // Destroy the kv
		}
	}
	free(level->table->data);
	free(level->table);
	free(level);
}

void st_destroy(void)
{
	scope *level = (scope *)stack_item_from_top(sym_tab, 0);

	while (level) {
		st_level_destroy(level);
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
	st_level_destroy((scope *)stack_pop(sym_tab));
}

size_t scope_level(void)
{
	return stack_size(sym_tab);
}

void scope_bind(ast_typed_symbol *symbol)
{
	scope *top = (scope *)stack_item_from_top(sym_tab, 0);
	scope_insert(top, symbol->symbol, symbol);
}

ast_typed_symbol *scope_lookup(strvec *name)
{
	int i = 0;
	ast_typed_symbol *found;
	scope *current = (scope *)stack_item_from_top(sym_tab, 0);
	while (current != 0) {
		if ((found = (ast_typed_symbol *)scope_get(current, name)))
			return found;
		i++;
		current = (scope *)stack_item_from_top(sym_tab, i);
	}
	return 0;
}

ast_typed_symbol *scope_lookup_current(strvec *name)
{
	scope *current = (scope *)stack_item_from_top(sym_tab, 0);
	return (ast_typed_symbol *)scope_get(current, name);
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
