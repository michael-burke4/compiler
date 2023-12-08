#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "strvec.h"

strvec *strvec_init(size_t capacity)
{
	strvec *ret = malloc(sizeof(*ret));
	ret->capacity = capacity;
	ret->size = 0;
	ret->text = calloc(capacity, sizeof(*(ret->text)));
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

int strvec_equals_str(strvec *vec, const char *string)
{
	if (vec->size != strlen(string))
		return 0;
	// not to make it one or zero.
	return !memcmp(vec->text, string, vec->size);
}
