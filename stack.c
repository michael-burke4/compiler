#include "util.h"
#include "stack.h"


stack *stack_init()
{
	stack *ret = smalloc(sizeof(*ret));
	ret->size = 0;
	ret->top = 0;
	return ret;
}

void stack_push(stack *stk, void *item)
{
	struct node *new_top;
	if (!stk)
		return;
	new_top = malloc(sizeof(*new_top));
	new_top->data = item;
	new_top->next = stk->top;
	stk->top = new_top;
	stk->size++;

}

void *stack_pop(stack *stk)
{
	void *ret;
	struct node *old_top;
	if (!stk || stk->size == 0)
		return 0;
	old_top = stk->top;
	ret = old_top->data;
	stk->top = old_top->next;
	free(old_top);
	stk->size--;
	return ret;
}

size_t stack_size(stack *stk)
{
	if (!stk)
		return 0;
	return stk->size;
}

// Position 0 is the BASE of the stack!!!
void *stack_item_from_base(stack *stk, size_t position)
{
	size_t i;
	struct node *current;
	if (!stk)
		return 0;
	if (position >= stk->size)
		return 0;

	i = stk->size - 1;
	current = stk->top;
	if (position > i)
		return 0;
	while (i > position) {
		if (current == 0)
			return 0;
		current = current->next;
		i--;
	}
	return current->data;
}

void *stack_item_from_top(stack *stk, size_t position)
{
	if (!stk)
		return 0;
	return stack_item_from_base(stk, stk->size - position - 1);
}
