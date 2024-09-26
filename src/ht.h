#ifndef HT_H
#define HT_H

#include "util.h"

#include <stddef.h>
#include <stdint.h>

uint64_t hash(strvec *str);

struct kv {
	uint64_t key;
	void *val;
};

struct ht {
	struct kv **data;
	size_t capacity;
	size_t num_elements;
	void (*element_destroyer)(void *);
};

struct ht *ht_init(size_t capacity, void (*destroyer)(void *));
int ht_insert(struct ht *tab, strvec *key, void *value);
void *ht_get(struct ht *tab, strvec *key);
int ht_resize(struct ht *tab, size_t new_cap);
// Freeing the hash table is NO LONGER left as an exercise for the reader!
// Simply provide a means of destroying the elements in the hash table!
void ht_destroy(struct ht *tab);

#endif
