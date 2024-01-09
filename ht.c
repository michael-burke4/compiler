#include "ht.h"
#include "util.h"

uint64_t hash(char *str)
{
	// FNV_offset_basis, per
	// https://en.wikipedia.org/wiki/Fowler–Noll–Vo_hash_function
	uint64_t hash = 14695981039346656037;

	while(str[0] != '\0') {
		hash ^= str[0];
		// FNV_prime, also from above wikipedia article.
		hash *= 1099511628211;
		str += 1;
	}
	return hash;
}

struct ht *ht_init(size_t capacity, bool dynamic)
{
	struct ht *ret = smalloc(sizeof(*ret));
	ret->data = scalloc(capacity, sizeof(*ret->data));
	ret->capacity = capacity;
	ret->size = 0;
	ret->dynamic_vals = dynamic;
	return ret;
}

static void free_data(struct kv **data, size_t cap, bool free_vals)
{
	size_t i = 0;
	for (i = 0 ; i < cap ; i++) {
		if (free_vals && data[i] != 0)
			free(data[i]->val);
		free(data[i]);
	}
	free(data);
}

void ht_end(struct ht *tab)
{
	if (!tab)
		return;
	free_data(tab->data, tab->capacity, tab->dynamic_vals);
	tab->capacity = 0;
	tab->size = 0;
	free(tab);
}

static int insert(struct kv **data, size_t cap, uint64_t key_hash, void *value)
{
	size_t index = key_hash % cap;
	while (data[index] != 0) {
		if (data[index]->key == key_hash)
			return 0;
		index = (index + 1) % cap;
	}
	data[index] = smalloc(sizeof(**(data)));
	data[index]->key = key_hash;
	data[index]->val = value;
	return 1;
}

int ht_insert(struct ht *tab, char *key, void *value)
{
	if (tab->size == tab->capacity * 3 / 4)
		ht_resize(tab, tab->capacity * 2);
	if (insert(tab->data, tab->capacity, hash(key), value)) {
		tab->size += 1;
		return 1;
	}
	return 0;
}

int ht_resize(struct ht *tab, size_t new_cap)
{
	
	struct kv **new_data;
	if (new_cap < tab->capacity)
		return 0;
	new_data = scalloc(new_cap, sizeof(*new_data));
	for (size_t i = 0 ; i < tab->capacity ; ++i) {
		struct kv *entry = tab->data[i];
		if (tab->data[i] == 0)
			continue;
		insert(new_data, new_cap, entry->key, entry->val);
	}
	free_data(tab->data, tab->capacity, tab->dynamic_vals);
	tab->data = new_data;
	tab->capacity = new_cap;
	
	return 1;
}

/**
 * Get the element with key 'key' from hashtable `tab`
 */
void *ht_get(struct ht *tab, char *key)
{
	uint64_t key_hash = hash(key);
	size_t index = key_hash % tab->capacity;
	size_t i = 0;
	while (i < tab->capacity) {
		struct kv *entry = tab->data[index];
		if (entry == 0)
			return 0;
		if (entry->key == key_hash)
			return entry->val;
		++index;
		index %= tab->capacity;
		++i;
	}
	// failsafe tab->capacity limit SHOULD be unnecessary!
	return 0;
}

/**
 * Remove the element with key `key` from hashtable `tab`.
 *
 * If that element does not exist within the table, or the key otherwise
 * can't be removed, false is returned.
 * Returns true on successful removal.
 */
bool ht_remove(struct ht *tab, char *key)
{
	uint64_t key_hash = hash(key);
	size_t index = key_hash % tab->capacity;
	size_t i = 0;
	while (i < tab->capacity) {
		struct kv *entry = tab->data[index];
		if (entry == 0)
			return false;
		if (entry->key == key_hash) {
			if (tab->dynamic_vals)
				free(entry->val);
			free(entry);
			tab->data[index] = 0;
			return true;
		}
			
		++index;
		index %= tab->capacity;
		++i;
	}
	// failsafe tab->capacity limit SHOULD be unnecessary!
	return false;
}
