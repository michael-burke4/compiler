#ifndef STRVEC_H
#define STRVEC_H

typedef struct {
	size_t size;
	size_t capacity;
	char *text;
} strvec;

strvec *strvec_init(size_t capacity);
void strvec_append(strvec *vec, char c);
void strvec_print(strvec *vec);
void strvec_destroy(strvec *vec);

#endif
