#include "util.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

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
		vec->text = reallocarray(vec->text, vec->size * 2,
					 sizeof(*(vec->text)));
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
