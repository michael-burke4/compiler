#ifndef CODEGEN_H
#define CODEGEN_H

#include "ast.h"

#include <llvm-c/Analysis.h>
#include <llvm-c/BitWriter.h>
#include <llvm-c/Core.h>
#include <llvm-c/ExecutionEngine.h>
#include <llvm-c/Target.h>

LLVMModuleRef module_codegen(LLVMContextRef ctxt, ast_decl *start, char *module_name);
void decl_codegen(LLVMModuleRef *mod, ast_decl *decl);
void stmt_codegen(LLVMModuleRef mod, LLVMBuilderRef builder, ast_stmt *stmt, int in_fn);
LLVMValueRef expr_codegen(LLVMModuleRef mod, LLVMBuilderRef builder, ast_expr *expr, int store_ctxt);
LLVMValueRef define_string_literal(LLVMModuleRef module, LLVMBuilderRef builder, const char *source_string, size_t size);


#endif
