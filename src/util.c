#include "util.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void *_reallocarray(void *ptr, size_t nmemb, size_t size)
{
	if (nmemb > 0 && SIZE_MAX / nmemb < size)
		return NULL;

	size_t total_size = nmemb * size;
	if (size != 0 && total_size / size != nmemb)
		return NULL;
	return srealloc(ptr, total_size);
}

vect *vect_init(size_t capacity)
{
	vect *ret = smalloc(sizeof(*ret));
	ret->capacity = capacity;
	ret->size = 0;
	ret->elements = calloc(capacity, sizeof(*(ret->elements)));
	return ret;
}

void vect_append(vect *v, void *e)
{
	if (v->capacity <= v->size) {
		v->elements = _reallocarray(v->elements, v->size * 2, sizeof(*(v->elements)));
		v->capacity *= 2;
	}
	v->elements[v->size] = e;
	(v->size)++;
}

// user must free/destroy each internal element themselves!
void vect_destroy(vect *v) {
	if (v == NULL)
		return;
	free(v->elements);
	v->capacity = 0;
	v->size = 0;
	free(v);
}

void *vect_get(vect *v, size_t i) {
	if (i > v->size) {
		fprintf(stderr, "Vec index out of bounds [%lu not within bound %lu]\n", i, v->size-1);
		exit(1);
	}
	return v->elements[i];
}

strvec *strvec_init(size_t capacity)
{
	strvec *ret = smalloc(sizeof(*ret));
	ret->capacity = capacity + 1;
	ret->size = 1;
	ret->text = smalloc(ret->capacity);
	ret->text[0] = '\0';
	return ret;
}

strvec *strvec_init_str(const char *str)
{
	strvec *ret = smalloc(sizeof(*ret));
	size_t len = strlen(str);
	ret->capacity = len + 1;
	ret->size = ret->capacity;
	ret->text = smalloc(ret->capacity);
	memcpy(ret->text, str, len);
	ret->text[len] = '\0';
	return ret;
}

void strvec_append(strvec *vec, char c)
{
	if (vec->capacity <= vec->size) {
		vec->text = _reallocarray(vec->text, vec->size * 2, sizeof(*(vec->text)));
		vec->capacity *= 2;
	}
	vec->text[vec->size] = '\0';
	vec->text[vec->size - 1] = c;
	vec->size += 1;
}

void fstrvec_print(FILE *f, strvec *vec)
{
	fprintf(f, "%s", vec->text);
}

void strvec_print(strvec *vec)
{
	fstrvec_print(stdout, vec);
}

void strvec_destroy(strvec *vec)
{
	if (vec == NULL)
		return;
	free(vec->text);
	vec->capacity = 0;
	vec->size = 0;
	free(vec);
}

int strvec_equals(strvec *a, strvec *b)
{
	// checking size first so it can short circuit when sizes are not the same.
	return a->size == b->size && !memcmp(a->text, b->text, a->size-1);
}

int strvec_equals_str(strvec *vec, const char *string)
{
	if (vec->size - 1 != strlen(string))
		return 0;
	// not to make it one or zero.
	return !memcmp(vec->text, string, vec->size-1);
}

// Check errno at calling code!
long strvec_tol(strvec *vec)
{
	long ret = 0;
	char *tmp = smalloc(vec->size + 1);
	errno = 0;
	memcpy(tmp, vec->text, vec->size);
	tmp[vec->size] = '\0';
	ret = strtol(tmp, 0, 10);
	free(tmp);
	return ret;
}

strvec *strvec_copy(strvec *s)
{
	strvec *ret;
	if (s == NULL)
		return 0;
	ret = strvec_init(s->capacity);
	ret->size = s->size;
	ret->text = memcpy(ret->text, s->text, s->size);
	return ret;
}

// Smalloc for 'safe malloc'
// errors out if malloc fails.
void *smalloc(size_t size)
{
	void *ret = malloc(size);
	if (ret == NULL)
		err(1, "malloc failed");
	return ret;
}

void *scalloc(size_t nmemb, size_t size)
{
	void *ret = calloc(nmemb, size);
	if (ret == NULL)
		err(1, "calloc failed");
	return ret;
}

void *srealloc(void *ptr, size_t size)
{
	void *ret = realloc(ptr, size);
	if (ret == NULL)
		err(1, "realloc failed");
	return ret;
}

void print_bits(uint64_t x)
{
	uint64_t top_bit = 1l << 63l;
	for (int i = 64 ; i > 0 ; --i) {
		printf("%d", (x & top_bit) != 0);
		x <<= 1;
		if ((i-1) % 4 == 0)
			printf(" ");
	}
	puts("");
}
