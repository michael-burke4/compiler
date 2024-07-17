#ifndef HT_H
#define HT_H
#include <stddef.h>
#include <stdint.h>
#include "util.h"

uint64_t hash(strvec *str);

struct kv {
	uint64_t key;
	void *val;
};

struct ht {
	struct kv **data;
	size_t capacity;
	size_t size;
};

struct ht *ht_init(size_t capacity);
// Freeing the hash table is left as an exercise for the reader.
int ht_insert(struct ht *tab, strvec *key, void *value);
void *ht_get(struct ht *tab, strvec *key);
int ht_resize(struct ht *tab, size_t new_cap);

#endif
