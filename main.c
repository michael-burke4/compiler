#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include "strvec.h"
#include "token.h"
#include "scan.h"
#include "ast.h"
#include "parse.h"

int had_error = 0;

int main(int argc, const char *argv[])
{
	FILE *f;
	token_s *t;
	token_s *head;
	ast_decl *program;
	int retcode = 0;

	if (argc != 2) {
		printf("Invalid # of arguments: invoke like `./main [file]`\n");
		return 1;
	}

	f = fopen(argv[1], "r");
	if (!f)
		err(1, "Could not open specified file \"%s\"", argv[1]);

	head = scan(f);
	t = head;
	if (had_error) {
		retcode = 1;
		goto error_noast;
	}

	program = parse_program(&t);
	if (had_error) {
		retcode = 1;
		goto error_ast;
	}

	program_print(program);
	ast_free(program);
	tok_list_destroy(head);

	fclose(f);
	return 0;

error_ast:
	ast_free(program);
error_noast:
	tok_list_destroy(head);
	fclose(f);
	return retcode;
}
