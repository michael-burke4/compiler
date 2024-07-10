#include "util.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

static void *_reallocarray(void *ptr, size_t nmemb, size_t size)
{
	if (nmemb > 0 && SIZE_MAX / nmemb < size)
    		return 0;

	size_t total_size = nmemb * size;
	if (size != 0 && total_size / size != nmemb)
		return 0;
	return realloc(ptr, total_size);
}

vec *vec_init(size_t capacity)
{
	vec *ret = smalloc(sizeof(*ret));
	ret->capacity = capacity;
	ret->size = 0;
	ret->elements = calloc(capacity, sizeof(*(ret->elements)));
	return ret;
}

void vec_append(vec *v, void *e)
{
	if (v->capacity <= v->size) {
		v->elements = _reallocarray(v->elements, v->size * 2, sizeof(*(v->elements)));
		v->capacity *= 2;
	}
	v->elements[v->size] = e;
	v->size++;
}

// user must free/destroy each internal element themselves!
void vec_destroy(vec *v) {
	if (!v)
		return;
	free(v->elements);
	v->capacity = 0;
	v->size = 0;
	free(v);
}

void *vec_get(vec *v, size_t i) {
	if (i > v->size) {
		printf("Vec index out of bounds [%lu not within bound %lu]\n", i, v->size-1);
		exit(1);
	}
	return v->elements[i];
}

strvec *strvec_init(size_t capacity)
{
	strvec *ret = smalloc(sizeof(*ret));
	ret->capacity = capacity;
	ret->size = 0;
	ret->text = calloc(capacity, sizeof(*(ret->text)));
	return ret;
}

strvec *strvec_init_str(const char *str)
{
	size_t len = strlen(str);
	strvec *ret = smalloc(sizeof(*ret));
	ret->capacity = len;
	ret->size = len;
	ret->text = smalloc(len);
	memcpy(ret->text, str, len);
	return ret;
}

void strvec_append(strvec *vec, char c)
{
	if (vec->capacity <= vec->size) {
		vec->text = _reallocarray(vec->text, vec->size * 2, sizeof(*(vec->text)));
		vec->capacity *= 2;
	}
	vec->text[vec->size] = c;
	vec->size++;
}

void strvec_print(strvec *vec)
{
	// stupid int cast do something better (eventually)
	printf("%.*s", (int)vec->size, vec->text);
}

void strvec_destroy(strvec *vec)
{
	if (!vec)
		return;
	free(vec->text);
	vec->capacity = 0;
	vec->size = 0;
	free(vec);
}

int strvec_equals(strvec *a, strvec *b)
{
	// checking size first so it can short circuit when sizes are not the same.
	return a->size == b->size && !memcmp(a->text, b->text, a->size);
}

int strvec_equals_str(strvec *vec, const char *string)
{
	if (vec->size != strlen(string))
		return 0;
	// not to make it one or zero.
	return !memcmp(vec->text, string, vec->size);
}

// atoi returns 0 on fail, so you can detect under/overflows in parse.c by
// only accepting 0 parse results if the srvec's len is 1.
int strvec_toi(strvec *vec)
{
	int ret;
	char *tmp = smalloc(vec->size + 1);
	memcpy(tmp, vec->text, vec->size);
	tmp[vec->size] = '\0';

	ret = atoi(tmp);
	free(tmp);
	return ret;
}

strvec *strvec_copy(strvec *s)
{
	strvec *ret;
	if (!s)
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
	if (!ret)
		err(1, "malloc failed");
	return ret;
}

void *scalloc(size_t nmemb, size_t size)
{
	void *ret = calloc(nmemb, size);
	if (!ret)
		err(1, "calloc failed");
	return ret;
}

void *srealloc(void *ptr, size_t size)
{
	void *ret = realloc(ptr, size);
	if (!ret)
		err(1, "realloc failed");
	return ret;
}

void strvec_tostatic(strvec *vec, char buff[BUFFER_MAX_LEN])
{
	size_t i;
	if (vec->size > BUFFER_MAX_LEN - 1) {
		fprintf(stderr, "vector too big to put in static buffer!");
		exit(1);
	}
	for (i = 0; i < vec->size ; ++i) //strvec char array is NON-NULL TERMINATED!
		buff[i] = vec->text[i];
	buff[i] = '\0';
}
