#include <err.h>
#include <stdio.h>
#include "strvec.h"
#include "token.h"
#include "scan.h"
#include "ast.h"
#include "parse.h"

int main(int argc, const char *argv[])
{
	FILE *f;
	token_s *t;
	token_s *head;
	ast_decl *program;

	if (argc != 2) {
		printf("Invalid # of arguments: invoke like `./main [file]`\n");
		return 1;
	}

	f = fopen(argv[1], "r");
	if (!f)
		err(1, "Could not open specified file \"%s\"", argv[1]);

	head = scan(f);
	t = head;

	program = parse_program(&t);
	program_print(program);

	tok_list_destroy(head);
	fclose(f);
	return 0;
}
