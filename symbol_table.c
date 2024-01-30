#include "symbol_table.h"
#include "ast.h"
#include "util.h"
#include "stack.h"

struct stack *sym_tab;

void st_level_destroy(struct ht *level)
{
	size_t i = 0;
	if (!level)
		return;

	for (i = 0; i < level->capacity; ++i) {
		if (level->data[i]) {
			ast_typed_symbol_destroy(
				(ast_typed_symbol *)level->data[i]->val); // val holds type
			free(level->data[i]); // Destroy the kv
		}
	}
	free(level->data);
	free(level);
}

void st_destroy()
{
	struct ht *level = (struct ht *)stack_item_from_top(sym_tab, 0);

	while (level) {
		st_level_destroy(level);
		stack_pop(sym_tab);
		level = (struct ht *)stack_item_from_top(sym_tab, 0);
	}

	free(sym_tab);
	sym_tab = 0;
}

void st_init(void)
{
	sym_tab = stack_init();
	stack_push(sym_tab, ht_init(8));
}

void scope_enter(void)
{
	struct ht *new = ht_init(8);
	stack_push(sym_tab, (void *)new);
}

void scope_exit(void)
{
	st_level_destroy((struct ht *)stack_pop(sym_tab));
}

size_t scope_level(void)
{
	return stack_size(sym_tab);
}

void scope_bind(ast_typed_symbol *symbol)
{
	struct ht *top = (struct ht *)stack_item_from_top(sym_tab, 0);
	ht_insert(top, symbol->symbol, symbol);
}

ast_typed_symbol *scope_lookup(strvec *name)
{
	int i = 0;
	ast_typed_symbol *found;
	struct ht *current = (struct ht *)stack_item_from_top(sym_tab, 0);
	while (current != 0) {
		if ((found = (ast_typed_symbol *)ht_get(current, name)))
			return found;
		i++;
		current = (struct ht *)stack_item_from_top(sym_tab, i);
	}
	return 0;
}

ast_typed_symbol *scope_lookup_current(strvec *name)
{
	struct ht *current = (struct ht *)stack_item_from_top(sym_tab, 0);
	return ht_get(current, name);
}
