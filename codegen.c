#include "codegen.h"
#include "util.h"
#include <stdio.h>

LLVMModuleRef program_codegen(ast_decl *program)
{
	LLVMModuleRef ret = LLVMModuleCreateWithName("my_module");
	while (program) {
		decl_codegen(&ret, program);
		program = program->next;
	}
	return ret;
}

void decl_codegen(LLVMModuleRef *mod, ast_decl *decl)
{
	if (!decl)
		return;
	if (decl->typesym->type->kind == Y_FUNCTION) {
		LLVMTypeRef param_types[] = { LLVMInt32Type(), LLVMInt32Type() };
		LLVMTypeRef ret_type = LLVMFunctionType(LLVMInt32Type(), param_types, 2, 0);
		LLVMValueRef sum = LLVMAddFunction(*mod, "sum", ret_type);
		LLVMBasicBlockRef entry = LLVMAppendBasicBlock(sum, "entry");
		LLVMBuilderRef builder = LLVMCreateBuilder();
		LLVMPositionBuilderAtEnd(builder, entry);
		LLVMValueRef tmp = LLVMBuildAdd(builder, LLVMGetParam(sum, 0), LLVMGetParam(sum, 1), "tmp");
		LLVMBuildRet(builder, tmp);
		LLVMDisposeBuilder(builder);
	}
	else
		printf("Can't codegen decls of this type yet :(\n");
}
