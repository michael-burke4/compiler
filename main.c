#include <err.h>
#include <stdio.h>
#include "strvec.h"
#include "token.h"
#include "scan.h"

int main(int argc, const char *argv[])
{
	if (argc != 2) {
		printf("Invalid # of arguments: invoke like `./main [file]`\n");
		return 1;
	}
	FILE *f = fopen(argv[1], "r");
	if (!f)
		err(1, "Could not open specified file \"%s\"", argv[1]);

	token_s *t = scan(f);

	token_s *print = t;
	while (print != 0) {
		tok_print(print);
		print = print->next;
	}
	tok_list_destroy(t);
	fclose(f);
	return 0;
}
