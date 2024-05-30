#include "codegen.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include "print.h"
#include "ht.h"

unsigned long counter = 0;

LLVMModuleRef program_codegen(ast_decl *program, char *module_name)
{
	LLVMModuleRef ret = LLVMModuleCreateWithName(module_name);
	while (program) {
		decl_codegen(&ret, program);
		program = program->next;
	}
	return ret;
}

LLVMValueRef stmt_codegen(LLVMBuilderRef builder, ast_stmt *stmt)
{
	switch (stmt->kind) {
	case S_BLOCK:
		return stmt_codegen(builder, stmt->body);
	case S_EXPR:
		return expr_codegen(builder, stmt->expr);
	case S_RETURN:
		return LLVMBuildRet(builder, expr_codegen(builder, stmt->expr));
	default:
		printf("can't codegen that expr kind right now.");
		exit(1);
	}
}

LLVMValueRef expr_codegen(LLVMBuilderRef builder, ast_expr *expr)
{
	char buffer[128];
	switch (expr->kind) {
	case E_INT_LIT:
		return LLVMConstInt(LLVMInt32Type(), (unsigned long long)expr->int_lit,0);
	case E_ADDSUB:
		snprintf(buffer, sizeof(buffer), "e%lu", counter++);
		return LLVMBuildAdd(builder, expr_codegen(builder, expr->left), expr_codegen(builder, expr->right), buffer);
	default:
		printf("can't codegen that expr kind right now.");
		exit(1);
	}
}

static LLVMTypeRef to_llvm_type(ast_type *tp)
{
	switch (tp->kind) {
	case Y_I32:
		return LLVMInt32Type();
	default:
		printf("couldn't convert type");
		abort();
	}
}


static LLVMTypeRef *build_param_types(ast_decl *decl, unsigned *num)
{
	*num = 0;
	ast_typed_symbol *ts;
	LLVMTypeRef *ret;
	size_t i = 0;

	for (ts = decl->typesym->type->arglist ; ts != 0 ; ts = ts->next) {
		*num += 1;
	}
	if (*num == 0)
		return 0;
	ret = malloc(sizeof (*ret) * *num);
	for (ts = decl->typesym->type->arglist ; ts != 0 ; ts = ts->next) {
		ret[i] = to_llvm_type(ts->type);
		++i;
	}
	return ret;
}

static void set_param_names(LLVMValueRef fn, ast_decl *decl, unsigned count)
{
	LLVMValueRef arg;
	ast_typed_symbol *ts = 0;
	char buf[BUFFER_MAX_LEN];

	for (unsigned i = 0 ; i < count ; ++i) {
		if (!ts)
			ts = decl->typesym->type->arglist;
		strvec_tostatic(ts->symbol, buf);
		arg = LLVMGetParam(fn, i);
		LLVMSetValueName(arg, buf);
		ts = ts->next;
	}
}

void decl_codegen(LLVMModuleRef *mod, ast_decl *decl)
{
	if (!decl)
		return;
	if (decl->typesym->type->kind == Y_FUNCTION) {
		char buf[BUFFER_MAX_LEN];
		unsigned num_args;
		LLVMTypeRef *param_types = build_param_types(decl, &num_args);
		LLVMTypeRef ret_type = LLVMFunctionType(LLVMInt32Type(), param_types, num_args, 0);

		strvec_tostatic(decl->typesym->symbol, buf);
		LLVMValueRef fn_value = LLVMAddFunction(*mod, buf, ret_type);
		set_param_names(fn_value, decl, num_args);
		LLVMBasicBlockRef entry = LLVMAppendBasicBlock(fn_value, "entry");

		LLVMBuilderRef builder = LLVMCreateBuilder();
		LLVMPositionBuilderAtEnd(builder, entry);
		stmt_codegen(builder, decl->body);
		LLVMDisposeBuilder(builder);
		free(param_types);
	}
	else {
		printf("Can't codegen decls of this type yet :(\n");
		exit(1);
	}
}
