#ifndef UTIL_H
#define UTIL_H

#include <err.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define BUFFER_MAX_LEN 128

typedef struct strvec {
	size_t size;
	size_t capacity;
	char *text;
} strvec;

typedef struct vect {
	size_t size;
	size_t capacity;
	void **elements;
} vect;

// CURRENLTY: strvec and vec are two different things

void *smalloc(size_t size);
void *scalloc(size_t nmemb, size_t size);
void *srealloc(void *ptr, size_t size);

strvec *strvec_init(size_t capacity);
strvec *strvec_copy(strvec *s);
strvec *strvec_init_str(const char *str);
void strvec_append(strvec *vec, char c);
int strvec_equals(strvec *a, strvec *b);
void fstrvec_print(FILE *f, strvec *vec);
void strvec_print(strvec *vec);
void strvec_destroy(strvec *vec);
int strvec_equals_str(strvec *vec, const char *string);
long strvec_tol(strvec *vec);
void strvec_tostatic(strvec *vec, char buff[BUFFER_MAX_LEN]);

vect *vect_init(size_t capacity);
void vect_append(vect *vect, void *e);
void vect_destroy(vect *vect);
void *vect_get(vect *v, size_t i);

void print_bits(uint64_t x);

#endif
