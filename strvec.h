#ifndef STRVEC_H
#define STRVEC_H
#include <stddef.h>

typedef struct {
	size_t size;
	size_t capacity;
	char *text;
} strvec;

strvec *strvec_init(size_t capacity);
strvec *strvec_init_str(const char *str);
void strvec_append(strvec *vec, char c);
void strvec_print(strvec *vec);
void strvec_destroy(strvec *vec);
int strvec_equals_str(strvec *vec, const char *string);

#endif
