#include "stack.h"
#include "util.h"

stack *stack_init(void)
{
	stack *ret = smalloc(sizeof(*ret));
	ret->size = 0;
	ret->top = NULL;
	return ret;
}

void stack_push(stack *stk, void *item)
{
	struct node *new_top;
	if (stk == NULL)
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
	if (stk == NULL || stk->size == 0)
		return NULL;
	old_top = stk->top;
	ret = old_top->data;
	stk->top = old_top->next;
	free(old_top);
	stk->size--;
	return ret;
}

size_t stack_size(stack *stk)
{
	if (stk == NULL)
		return 0;
	return stk->size;
}

// Position 0 is the BASE of the stack!!!
void *stack_item_from_base(stack *stk, size_t position)
{
	size_t i;
	struct node *current;
	if (stk == NULL)
		return NULL;
	if (position >= stk->size)
		return NULL;

	i = stk->size - 1;
	current = stk->top;
	if (position > i)
		return NULL;
	while (i > position) {
		if (current == 0)
			return NULL;
		current = current->next;
		i--;
	}
	return current->data;
}

void *stack_item_from_top(stack *stk, size_t position)
{
	if (stk == NULL)
		return NULL;
	return stack_item_from_base(stk, stk->size - position - 1);
}
