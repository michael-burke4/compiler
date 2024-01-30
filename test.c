#include <stdio.h>
#include "symbol_table.h"
#include "ast.h"
#include "token.h"
#include "print.h"
#include "stack.h"

int had_error = 0;
extern struct stack *sym_tab;

//ast_typed_symbol *ast_typed_symbol_init(ast_type *type, strvec *symbol);
int main(void)
{
	st_init();
	ast_typed_symbol *s = ast_typed_symbol_init(type_init(T_I32, 0), strvec_init_str("hello"));
	ast_typed_symbol *s2 = ast_typed_symbol_init(type_init(T_U32, 0), strvec_init_str("other"));
	scope_bind(strvec_init_str("hello"), s);
	printf("(expecting none) ");
	typed_sym_print(scope_lookup(strvec_init_str("hell")));
	printf("\n");
	typed_sym_print(scope_lookup(strvec_init_str("hello")));
	printf("\n");
	typed_sym_print(scope_lookup(strvec_init_str("hello")));
	printf("\n");
	typed_sym_print(scope_lookup_current(strvec_init_str("hello")));
	printf("\n");

	puts("entering new scope :)");
	scope_enter();
	scope_bind(strvec_init_str("other"), s2);
	printf("(expecting none) ");
	typed_sym_print(scope_lookup_current(strvec_init_str("hello")));
	puts("");
	typed_sym_print(scope_lookup_current(strvec_init_str("other")));
	puts("");

	puts("exiting scope!");
	scope_exit();
	printf("(expecting none)");
	typed_sym_print(scope_lookup_current(strvec_init_str("other")));
	printf("\n");

	typed_sym_print(scope_lookup(strvec_init_str("hello")));
	printf("\n");



	return 0;
}
