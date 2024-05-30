#include "codegen.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>

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

void decl_codegen(LLVMModuleRef *mod, ast_decl *decl)
{
	if (!decl)
		return;
	if (decl->typesym->type->kind == Y_FUNCTION) {
		char buf[BUFFER_MAX_LEN];
		LLVMTypeRef param_types[] = { LLVMInt32Type(), LLVMInt32Type() };
		LLVMTypeRef ret_type = LLVMFunctionType(LLVMInt32Type(), param_types, 2, 0);
		strvec_tostatic(decl->typesym->symbol, buf);
		LLVMValueRef fn_value = LLVMAddFunction(*mod, buf, ret_type);
		LLVMBasicBlockRef entry = LLVMAppendBasicBlock(fn_value, "entry");
		LLVMBuilderRef builder = LLVMCreateBuilder();
		LLVMPositionBuilderAtEnd(builder, entry);
		stmt_codegen(builder, decl->body);
		LLVMDisposeBuilder(builder);
	}
	else {
		printf("Can't codegen decls of this type yet :(\n");
		exit(1);
	}
}
