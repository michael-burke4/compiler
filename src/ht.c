#include "ht.h"

uint64_t hash(strvec *str)
{
	// FNV offset basis, per
	// https://en.wikipedia.org/wiki/Fowler–Noll–Vo_hash_function
	uint64_t hash = 14695981039346656037UL;
	size_t i;

	for (i = 0; i < str->size; ++i) {
		hash ^= str->text[i];
		// FNV_prime, also from above wikipedia article.
		hash *= 1099511628211;
	}
	return hash;
}

struct ht *ht_init(size_t capacity, void (*destroyer)(void *))
{
	struct ht *ret = smalloc(sizeof(*ret));
	ret->data = scalloc(capacity, sizeof(*ret->data));
	ret->capacity = capacity;
	ret->num_elements = 0;
	ret->element_destroyer = destroyer;

	return ret;
}

static int insert(struct kv **data, size_t cap, uint64_t key_hash, void *value)
{
	size_t index = key_hash % cap;
	while (data[index] != NULL) {
		if (data[index]->key == key_hash)
			return 0;
		index = (index + 1) % cap;
	}
	data[index] = smalloc(sizeof(**(data)));
	data[index]->key = key_hash;
	data[index]->val = value;
	return 1;
}

int ht_insert(struct ht *tab, strvec *key, void *value)
{
	if (tab->num_elements >= tab->capacity * 3 / 4)
		ht_resize(tab, tab->capacity * 2);
	if (insert(tab->data, tab->capacity, hash(key), value)) {
		tab->num_elements += 1;
		return 1;
	}
	return 0;
}

static void ht_data_destroy(struct ht *tab) {
	for (size_t i = 0 ; i < tab->capacity ; ++i) {
		if (tab->data[i] != NULL)
			tab->element_destroyer(tab->data[i]);
		free(tab->data[i]);
	}
	free(tab->data);
}

int ht_resize(struct ht *tab, size_t new_cap)
{
	struct kv **new_data;
	if (new_cap < tab->capacity)
		return 0;
	new_data = scalloc(new_cap, sizeof(*new_data));
	for (size_t i = 0; i < tab->capacity; ++i) {
		struct kv *entry = tab->data[i];
		if (tab->data[i] == NULL)
			continue;
		insert(new_data, new_cap, entry->key, entry->val);
	}
	ht_data_destroy(tab);
	tab->data = new_data;
	tab->capacity = new_cap;

	return 1;
}

/**
 * Get the element with key 'key' from hashtable `tab`
 */
void *ht_get(struct ht *tab, strvec *key)
{
	uint64_t key_hash = hash(key);
	size_t index = key_hash % tab->capacity;
	size_t i = 0;
	while (i < tab->capacity) {
		struct kv *entry = tab->data[index];
		if (entry == NULL)
			return NULL;
		if (entry->key == key_hash)
			return entry->val;
		++index;
		index %= tab->capacity;
		++i;
	}
	// failsafe tab->capacity limit SHOULD be unnecessary!
	return NULL;
}
void ht_destroy(struct ht *tab)
{
	if (tab == NULL)
		return;
	ht_data_destroy(tab);
	free(tab);
}
