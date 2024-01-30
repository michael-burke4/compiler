#ifndef UTIL_H
#define UTIL_H

#include <stdlib.h>
#include <err.h>
#include <stddef.h>

typedef struct {
	size_t size;
	size_t capacity;
	char *text;
} strvec;

void *smalloc(size_t size);
void *scalloc(size_t nmemb, size_t size);
void *srealloc(void *ptr, size_t size);

strvec *strvec_init(size_t capacity);
strvec *strvec_copy(strvec *s);
strvec *strvec_init_str(const char *str);
void strvec_append(strvec *vec, char c);
int strvec_equals(strvec *a, strvec *b);
void strvec_print(strvec *vec);
void strvec_destroy(strvec *vec);
int strvec_equals_str(strvec *vec, const char *string);
int strvec_toi(strvec *vec);
#endif
