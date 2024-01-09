#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

uint64_t hash(char *str);

struct kv {
	uint64_t key;
	void *val;
};

struct ht {
	struct kv **data;
	size_t capacity;
	size_t size;
	bool dynamic_vals;
};

struct ht *ht_init(size_t capacity, bool dynamic);
void ht_end(struct ht *tab);
int ht_insert(struct ht *tab, char *key, void *value);
void *ht_get(struct ht *tab, char *key);
int ht_resize(struct ht *tab, size_t new_cap);
