#include <err.h>
#include <stdio.h>
#include "strvec.h"
#include "token.h"

int main(void)
{
	FILE *f = fopen("./test.txt", "r");
	if (!f)
		err(1, "Could not open specified file");
	token_s *t = tokenize(f);
	
	token_s *print = t;
	while (print != 0) {
		tok_print(print);
		print = print->next;
	}
	tok_list_destroy(t);
	fclose(f);
	return 0;
}
