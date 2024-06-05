#include "codegen.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include "print.h"
#include "ht.h"
#include <string.h>

LLVMModuleRef program_codegen(ast_decl *program, char *module_name)
{
	LLVMModuleRef ret = LLVMModuleCreateWithName(module_name);
	while (program) {
		decl_codegen(&ret, program);
		program = program->next;
	}
	return ret;
}

void stmt_codegen(LLVMModuleRef mod, LLVMBuilderRef builder, ast_stmt *stmt)
{
	LLVMValueRef v1;
	LLVMBasicBlockRef b1;
	LLVMBasicBlockRef b2;
	LLVMValueRef cur_function;
	ast_stmt *cur;

	if (!stmt)
		return;

	switch (stmt->kind) {
	case S_BLOCK:
		cur = stmt->body;
		while (cur) {
			stmt_codegen(mod, builder, cur);
			cur = cur->next;
		}
		break;
	case S_EXPR:
		expr_codegen(mod, builder, stmt->expr);
		break;
	case S_RETURN:
		LLVMBuildRet(builder, expr_codegen(mod, builder, stmt->expr));
		break;
	case S_IFELSE:
		cur_function = LLVMGetBasicBlockParent(LLVMGetInsertBlock(builder));
		v1 = expr_codegen(mod, builder, stmt->expr);
		b1 = LLVMAppendBasicBlock(cur_function, "");
		b2 = LLVMAppendBasicBlock(cur_function, "");

		LLVMBuildCondBr(builder, v1, b1, b2);

		LLVMPositionBuilderAtEnd(builder, b1);
		stmt_codegen(mod, builder, stmt->body);

		LLVMPositionBuilderAtEnd(builder, b2);
		stmt_codegen(mod, builder, stmt->else_body);
		break;
	default:
		printf("can't codegen that stmt kind right now. (%d)\n", stmt->kind);
		exit(1);
	}
}

static LLVMValueRef get_param_by_name(LLVMValueRef function, char *name)
{
	LLVMValueRef param;
	for (unsigned int i = 0 ; i < LLVMCountParams(function) ; ++i) {
		param = LLVMGetParam(function, i);
		if (LLVMGetValueName(param) && strcmp(LLVMGetValueName(param), name) == 0)
			return param;
	}
	return 0;
}

#define MAX_ARGS 32
static void get_fncall_args(LLVMModuleRef mod, LLVMBuilderRef builder,ast_expr *expr, unsigned argno, LLVMValueRef (*args)[MAX_ARGS])
{
	ast_expr *cur_arg;
	if (argno == 0)
		return;
	cur_arg = expr->left;
	for (unsigned i = 0 ; i < MAX_ARGS ; ++i) {
		if (i < argno) {
			(*args)[i] = expr_codegen(mod, builder, cur_arg->left);
			cur_arg = cur_arg->right;
		} else
			(*args)[i] = 0;
	}
}


static LLVMTypeRef to_llvm_type(ast_type *tp)
{
	switch (tp->kind) {
	case Y_I32:
		return LLVMInt32Type();
	default:
		printf("couldn't convert type\n");
		abort();
	}
}

LLVMValueRef expr_codegen(LLVMModuleRef mod, LLVMBuilderRef builder, ast_expr *expr)
{
	char buffer[BUFFER_MAX_LEN];
	LLVMValueRef fn;
	//TODO: Strictly enforce a max number of args. Making this static is really nice. Honestly
	// convert all of the little hacks like 'link' expression type to be a static array within
	// the expr struct/decl struct etc.
	LLVMValueRef args[MAX_ARGS];
	unsigned argno;
	switch (expr->kind) {
	case E_LINK:
		printf("TRIED TO CODEGEN LINK!\n");
		abort();
	case E_INT_LIT:
		return LLVMConstInt(LLVMInt32Type(), (unsigned long long)expr->int_lit,0);
	case E_MULDIV:
		if (expr->op == T_STAR)
			return LLVMBuildMul(builder, expr_codegen(mod, builder, expr->left), expr_codegen(mod, builder, expr->right), "");
		else
			return LLVMBuildSDiv(builder, expr_codegen(mod, builder, expr->left), expr_codegen(mod, builder, expr->right), "");
	case E_ADDSUB:
		if (expr->op == T_MINUS)
			return LLVMBuildSub(builder, expr_codegen(mod, builder, expr->left), expr_codegen(mod, builder, expr->right), "");
		else
			return LLVMBuildAdd(builder, expr_codegen(mod, builder, expr->left), expr_codegen(mod, builder, expr->right), "");
	case E_EQUALITY:
		return LLVMBuildICmp(builder, LLVMIntEQ, expr_codegen(mod, builder, expr->left), expr_codegen(mod, builder, expr->right), "");
	case E_FNCALL:
		strvec_tostatic(expr->name, buffer);
		fn = LLVMGetNamedFunction(mod, buffer);
		argno = LLVMCountParams(fn);
		get_fncall_args(mod, builder, expr, argno, &args);
		return LLVMBuildCall(builder, fn, args, argno, "");
	case E_IDENTIFIER:
		//TODO: not every identifier is an argument lol.
		strvec_tostatic(expr->name, buffer);
		fn = LLVMGetBasicBlockParent(LLVMGetInsertBlock(builder));
		return get_param_by_name(fn, buffer);
	case E_FALSE_LIT:
		return LLVMConstInt(LLVMInt32Type(), 0, 0);
	case E_TRUE_LIT:
		return LLVMConstInt(LLVMInt32Type(), 1, 0);
	case E_INEQUALITY:
		if (expr->op == T_LT) {
			return LLVMBuildICmp(builder, LLVMIntSLT, expr_codegen(mod, builder, expr->left), expr_codegen(mod, builder, expr->right), "");
		}
	default:
		printf("can't codegen that expr kind right now.");
		exit(1);
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
		LLVMBasicBlockRef entry = LLVMAppendBasicBlock(fn_value, "");

		LLVMBuilderRef builder = LLVMCreateBuilder();
		LLVMPositionBuilderAtEnd(builder, entry);
		stmt_codegen(*mod, builder, decl->body);
		LLVMDisposeBuilder(builder);
		free(param_types);
	}
	else {
		printf("Can't codegen decls of this type yet :(\n");
		exit(1);
	}
}
