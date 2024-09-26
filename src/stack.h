#ifndef STACK_H
#define STACK_H

#include <stddef.h>

struct node {
	void *data;
	struct node *next;
};

typedef struct stack {
	size_t size;
	struct node *top;
} stack;

stack *stack_init(void);
void stack_push(stack *stk, void *item);
void *stack_pop(stack *stk);
void *stack_item_from_base(stack *stk, size_t position);
void *stack_item_from_top(stack *stk, size_t position);
size_t stack_size(stack *stk);

#endif
