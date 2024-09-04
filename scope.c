#include "scope.h"
#include "util.h"

static void destroyer(void *element) {
	// We don't actually want to free any elements
	// The elements are ast_typed_symbols
	// that are owned by the AST.
	if (!element)
		return;
	return;
}

struct scope *scope_init(size_t capacity) {
	scope *ret = smalloc(sizeof *ret);
	ret->table = ht_init(capacity, &destroyer);
	return ret;
}

int scope_insert(struct scope *s, strvec *key, void *value) {
	return ht_insert(s->table, key, (void *)value);
}

void *scope_get(struct scope *s, strvec *key) {
	return (ast_typed_symbol *)ht_get(s->table, key);
}

void scope_destroy(struct scope *s) {
	ht_destroy(s->table);
	free(s);
}
