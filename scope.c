#include "scope.h"
#include "util.h"

struct scope *scope_init(size_t capacity) {
	scope *ret = smalloc(sizeof *ret);
	ret->table = ht_init(capacity);
	return ret;
}

int scope_insert(struct scope *s, strvec *key, void *value) {
	return ht_insert(s->table, key, value);
}

void *scope_get(struct scope *s, strvec *key) {
	return ht_get(s->table, key);
}
