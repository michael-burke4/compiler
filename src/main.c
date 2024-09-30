#include "ast.h"
#include "codegen.h"
#include "parse.h"
#include "print.h"
#include "scan.h"
#include "symbol_table.h"
#include "token.h"
#include "typecheck.h"
#include "util.h"

#include <assert.h>
#include <err.h>
#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <llvm-c/Core.h>

extern int had_error;
extern struct stack *sym_tab;

char *cmd = NULL;

void usage() {
	printf("Usage: %s input_file [-o output file]\n", cmd);
	exit(1);
}

int main(int argc, char *argv[])
{
	FILE *f;
	token_s *head;
	ast_decl *program;
	int retcode = 0;
	char path[4096];
	char *modname;

	char *infile = NULL;
	char *outfile = NULL;
	int option;

	cmd = argv[0];

	// Hey, who knows.
	assert(CHAR_BIT == 8);
	assert(sizeof(float) * CHAR_BIT == 32);
	assert(sizeof(double) * CHAR_BIT == 64);


	while (optind < argc) {
		// This check if cur arg starts with dash should be unnecessary
		// but it doesn't work if I remove it?
		if (argv[optind][0] == '-' && (option = getopt(argc, argv, "o:")) != -1) {
			switch (option) {
			case 'o':
				outfile = optarg;
				break;
			default:
				usage();
			}
		} else {
			if (infile != NULL) {
				printf("%s: Only expecting one positional argument\n", cmd);
				usage();
			}
			infile = argv[optind++];
		}
	}

	if (infile == NULL) {
		printf("%s: Missing input file\n", cmd);
		usage();
	}

	f = fopen(infile, "r");
	if (f == NULL)
		err(1, "Could not open specified file \"%s\"", infile);

	head = scan(f);
	if (had_error) {
		retcode = 1;
		goto error_noast;
	}
	program = parse_program(head);
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

	// TODO: check path can fit infile?
	strcpy(path, infile);
	modname = basename(path);

	LLVMContextRef ctxt = LLVMContextCreate();
	LLVMModuleRef mod = module_codegen(ctxt, program, modname);
	char *error = 0;
	LLVMVerifyModule(mod, LLVMAbortProcessAction, &error);
	LLVMDisposeMessage(error);
	LLVMDumpModule(mod);

	error = 0;
	LLVMInitializeNativeTarget();
	LLVMInitializeNativeAsmPrinter();

	if (!outfile)
		outfile = "a.bc";
	if (LLVMWriteBitcodeToFile(mod, outfile) != 0) {
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
