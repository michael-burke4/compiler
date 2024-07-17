#ifndef CODEGEN_H
#define CODEGEN_H
#include <llvm-c/Core.h>
#include <llvm-c/ExecutionEngine.h>
#include <llvm-c/Target.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/BitWriter.h>
#include "ast.h"

LLVMModuleRef program_codegen(ast_decl *program, char *module_name);
void decl_codegen(LLVMModuleRef *mod, ast_decl *decl);
void stmt_codegen(LLVMModuleRef mod, LLVMBuilderRef builder, ast_stmt *stmt, vec *varvec, int in_fn);
LLVMValueRef expr_codegen(LLVMModuleRef mod, LLVMBuilderRef builder, ast_expr *expr, vec *v);


#endif
