#ifndef HT_H
#define HT_H
#include <stddef.h>
#include <stdint.h>
#include "util.h"

// see TODO later in file. Remove this eventually
#include "ast.h"

uint64_t hash(strvec *str);

struct kv {
	uint64_t key;
	void *val;
};

struct ht {
	struct kv **data;
	size_t capacity;
	size_t size;

	// TODO move this elsewhere.
	// this is ugly and pollutes the stand-alone purity of this
	// hash table implementation. Make a separate 'scope' implementation that
	// wraps ht but also has a return type field, or something like that.
	ast_type *return_type;
	// DO NOT FREE THIS RETURN TYPE.
	// YOU HAVE BEEN WARNED.
};

struct ht *ht_init(size_t capacity);
// Freeing the hash table is left as an exercise for the reader.
int ht_insert(struct ht *tab, strvec *key, void *value);
void *ht_get(struct ht *tab, strvec *key);
int ht_resize(struct ht *tab, size_t new_cap);

#endif
