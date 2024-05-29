#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include "codegen.h"
#include "util.h"
#include "token.h"
#include "scan.h"
#include "ast.h"
#include "parse.h"
#include "print.h"
#include "symbol_table.h"
#include "typecheck.h"
#include <llvm-c/Core.h>

int had_error = 0;
extern struct stack *sym_tab;

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

	st_init();
	typecheck_program(program);
	if (!had_error)
		puts("typecheck passed!");

	LLVMModuleRef mod = program_codegen(program);
	char *error = 0;
	LLVMVerifyModule(mod, LLVMAbortProcessAction, &error);
	LLVMDisposeMessage(error);

	error = 0;
	LLVMLinkInMCJIT();
	LLVMInitializeNativeTarget();
	LLVMInitializeNativeAsmPrinter();

	if (LLVMWriteBitcodeToFile(mod, "out.bc") != 0) {
		fprintf(stderr, "Could not write bitcode to file!");
	}

	st_destroy();
	ast_free(program);
	tok_list_destroy(head);
	LLVMDisposeModule(mod);
	LLVMShutdown();

	fclose(f);
	return 0;

error_ast:
	ast_free(program);
error_noast:
	tok_list_destroy(head);
	fclose(f);
	return retcode;
}
