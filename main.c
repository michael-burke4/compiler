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
#include <string.h>
#include <libgen.h>
#include <assert.h>
#include <limits.h>

int had_error = 0;
extern struct stack *sym_tab;

static void remove_extension(char *buf)
{
	for (int i = strlen(buf) ; i > 0 ; i--) {
		if (buf[i] == '.') {
			buf[i] = '\0';
			break;
		}
	}
}

int main(int argc, const char *argv[])
{
	FILE *f;
	token_s *t;
	token_s *head;
	ast_decl *program;
	int retcode = 0;
	char path[4096];
	char modname[4096];
	char outname[4096];

	// Hey, who knows.
	assert(CHAR_BIT == 8);
	assert(sizeof(float) * CHAR_BIT == 32);
	assert(sizeof(double) * CHAR_BIT == 64);

	if (argc != 2) {
		printf("Invalid # of arguments: invoke like `./main [file]`\n");
		return 1;
	}

	f = fopen(argv[1], "r");
	if (f == NULL)
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
	else {
		goto error_typecheck;
	}

	st_destroy();
	st_init();
	strcpy(path, argv[1]);
	strcpy(modname, basename(path));

	LLVMContextRef ctxt = LLVMContextCreate();
	LLVMModuleRef mod = module_codegen(ctxt, program, modname);
	char *error = 0;
	LLVMVerifyModule(mod, LLVMAbortProcessAction, &error);
	LLVMDisposeMessage(error);
	LLVMDumpModule(mod);

	error = 0;
	LLVMInitializeNativeTarget();
	LLVMInitializeNativeAsmPrinter();

	remove_extension(modname);
	sprintf(outname, "%s.bc", modname);
	if (LLVMWriteBitcodeToFile(mod, outname) != 0) {
		fprintf(stderr, "Could not write bitcode to file!");
	}

	st_destroy();
	ast_free(program);
	tok_list_destroy(head);
	LLVMDisposeModule(mod);
	LLVMContextDispose(ctxt);
	LLVMShutdown();

	fclose(f);
	return 0;

error_typecheck:
	st_destroy();
error_ast:
	ast_free(program);
error_noast:
	tok_list_destroy(head);
	fclose(f);
	LLVMShutdown();
	return retcode;
}
