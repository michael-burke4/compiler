#include "codegen.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include "print.h"
#include "ht.h"
#include <string.h>

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

static LLVMValueRef val_vec_lookup(vec *vec, char *name) {
	size_t dummy;
	LLVMValueRef cur;
	for (size_t i = 0  ; i < vec->size ; ++i) {
		cur = (LLVMValueRef)vec->elements[i];
		if (!strcmp(name, LLVMGetValueName2(cur, &dummy)))
			return cur;
	}
	return 0;
}

LLVMModuleRef program_codegen(ast_decl *program, char *module_name)
{
	LLVMModuleRef ret = LLVMModuleCreateWithName(module_name);
	while (program) {
		decl_codegen(&ret, program);
		program = program->next;
	}
	return ret;
}

void stmt_codegen(LLVMModuleRef mod, LLVMBuilderRef builder, ast_stmt *stmt, vec *v)
{
	LLVMValueRef v1;
	LLVMBasicBlockRef b1;
	LLVMBasicBlockRef b2;
	LLVMBasicBlockRef b3;
	LLVMValueRef cur_function;
	ast_stmt *cur;
	char buffer[BUFFER_MAX_LEN];

	if (!stmt)
		return;

	switch (stmt->kind) {
	case S_BLOCK:
		cur = stmt->body;
		while (cur) {
			stmt_codegen(mod, builder, cur, v);
			cur = cur->next;
		}

		break;
	case S_EXPR:
		expr_codegen(mod, builder, stmt->expr, v);
		break;
	case S_RETURN:
		LLVMBuildRet(builder, expr_codegen(mod, builder, stmt->expr, v));
		break;
	case S_IFELSE:
		cur_function = LLVMGetBasicBlockParent(LLVMGetInsertBlock(builder));
		v1 = expr_codegen(mod, builder, stmt->expr, v);
		b1 = LLVMAppendBasicBlock(cur_function, "");
		b2 = LLVMAppendBasicBlock(cur_function, "");
		b3 = LLVMAppendBasicBlock(cur_function, "");

		LLVMBuildCondBr(builder, v1, b1, b2);

		LLVMPositionBuilderAtEnd(builder, b1);
		stmt_codegen(mod, builder, stmt->body, v);
		LLVMBuildBr(builder, b3);

		LLVMPositionBuilderAtEnd(builder, b2);
		stmt_codegen(mod, builder, stmt->else_body, v);
		LLVMBuildBr(builder, b3);

		LLVMPositionBuilderAtEnd(builder, b3);
		break;
	case S_DECL:
		strvec_tostatic(stmt->decl->typesym->symbol, buffer);
		v1 = LLVMBuildAlloca(builder, to_llvm_type(stmt->decl->typesym->type), buffer);
		vec_append(v, (void *)v1);
		if (stmt->decl->expr != 0)
			LLVMBuildStore(builder, expr_codegen(mod, builder, stmt->decl->expr, v), v1);
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
static void get_fncall_args(LLVMModuleRef mod, LLVMBuilderRef builder,ast_expr *expr, unsigned argno, LLVMValueRef (*args)[MAX_ARGS], vec *v)
{
	ast_expr *cur_arg;
	if (argno == 0)
		return;
	cur_arg = expr->left;
	for (unsigned i = 0 ; i < MAX_ARGS ; ++i) {
		if (i < argno) {
			(*args)[i] = expr_codegen(mod, builder, cur_arg->left, v);
			cur_arg = cur_arg->right;
		} else
			(*args)[i] = 0;
	}
}

// Remaining expr types to codegen:
//E_STR_LIT,
//E_CHAR_LIT,
//E_PRE_UNARY,
//E_POST_UNARY,
LLVMValueRef expr_codegen(LLVMModuleRef mod, LLVMBuilderRef builder, ast_expr *expr, vec *nv)
{
	char buffer[BUFFER_MAX_LEN];
	LLVMValueRef v;
	//TODO: Strictly enforce a max number of args. Making this static is really nice. Honestly
	// convert all of the little hacks like 'link' expression type to be a static array within
	// the expr struct/decl struct etc.
	LLVMValueRef args[MAX_ARGS];
	LLVMValueRef ret;
	unsigned argno;
	switch (expr->kind) {
	case E_LINK:
		printf("TRIED TO CODEGEN LINK!\n");
		abort();
	case E_INT_LIT:
		return LLVMConstInt(LLVMInt32Type(), (unsigned long long)expr->int_lit,0);
	case E_MULDIV:
		if (expr->op == T_STAR)
			return LLVMBuildMul(builder, expr_codegen(mod, builder, expr->left, nv), expr_codegen(mod, builder, expr->right, nv), "");
		else
			return LLVMBuildSDiv(builder, expr_codegen(mod, builder, expr->left, nv), expr_codegen(mod, builder, expr->right, nv), "");
	case E_ADDSUB:
		if (expr->op == T_MINUS)
			return LLVMBuildSub(builder, expr_codegen(mod, builder, expr->left, nv), expr_codegen(mod, builder, expr->right, nv), "");
		else
			return LLVMBuildAdd(builder, expr_codegen(mod, builder, expr->left, nv), expr_codegen(mod, builder, expr->right, nv), "");
	case E_EQUALITY:
		return LLVMBuildICmp(builder, LLVMIntEQ, expr_codegen(mod, builder, expr->left, nv), expr_codegen(mod, builder, expr->right, nv), "");
	case E_FNCALL:
		strvec_tostatic(expr->name, buffer);
		v = LLVMGetNamedFunction(mod, buffer);
		argno = LLVMCountParams(v);
		get_fncall_args(mod, builder, expr, argno, &args, nv);
		return LLVMBuildCall(builder, v, args, argno, "");
	case E_IDENTIFIER:
		//TODO: not every identifier is an argument lol.
		strvec_tostatic(expr->name, buffer);
		v = LLVMGetBasicBlockParent(LLVMGetInsertBlock(builder));
		ret = get_param_by_name(v, buffer);
		if (!ret) {
			ret = val_vec_lookup(nv, buffer);
			ret = LLVMBuildLoad(builder, ret, "");
		}
		return ret;
	case E_ASSIGN:
		strvec_tostatic(expr->left->name, buffer);
		v = val_vec_lookup(nv, buffer);
		if (!v) {
			v = LLVMGetBasicBlockParent(LLVMGetInsertBlock(builder));
			v = get_param_by_name(v, buffer);
		}
		return LLVMBuildStore(builder, expr_codegen(mod, builder, expr->right, nv), v);
	case E_FALSE_LIT:
		return LLVMConstInt(LLVMInt32Type(), 0, 0);
	case E_TRUE_LIT:
		return LLVMConstInt(LLVMInt32Type(), 1, 0);
	case E_INEQUALITY:
		if (expr->op == T_LT)
			return LLVMBuildICmp(builder, LLVMIntSLT, expr_codegen(mod, builder, expr->left, nv), expr_codegen(mod, builder, expr->right, nv), "");
		else if (expr->op == T_LTE)
			return LLVMBuildICmp(builder, LLVMIntSLE, expr_codegen(mod, builder, expr->left, nv), expr_codegen(mod, builder, expr->right, nv), "");
		else if (expr->op == T_GT)
			return LLVMBuildICmp(builder, LLVMIntSGT, expr_codegen(mod, builder, expr->left, nv), expr_codegen(mod, builder, expr->right, nv), "");
		else
			return LLVMBuildICmp(builder, LLVMIntSGE, expr_codegen(mod, builder, expr->left, nv), expr_codegen(mod, builder, expr->right, nv), "");
	case E_PAREN:
		return expr_codegen(mod, builder, expr->left, nv);
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

static void alloca_params_as_local_vars(LLVMBuilderRef builder, LLVMValueRef fn, ast_decl *decl, LLVMTypeRef *param_types, unsigned count, vec *vc) {
	LLVMValueRef arg;
	ast_typed_symbol *ts = 0;
	LLVMValueRef v;
	char buf[BUFFER_MAX_LEN];

	for  (unsigned i = 0 ; i < count ; ++i) {
		if (!ts)
			ts = decl->typesym->type->arglist;
		strvec_tostatic(ts->symbol, buf);
		arg = LLVMGetParam(fn, i);
		v = LLVMBuildAlloca(builder, param_types[i], buf);
		LLVMBuildStore(builder, arg, v);
		vec_append(vc, (void *)v);
	}
}

void decl_codegen(LLVMModuleRef *mod, ast_decl *decl)
{
	if (!decl)
		return;
	if (decl->typesym->type->kind == Y_FUNCTION) {
		char buf[BUFFER_MAX_LEN];
		unsigned num_args;
		vec *v = vec_init(4);
		LLVMTypeRef *param_types = build_param_types(decl, &num_args);
		LLVMTypeRef ret_type = LLVMFunctionType(LLVMInt32Type(), param_types, num_args, 0);

		strvec_tostatic(decl->typesym->symbol, buf);
		LLVMValueRef fn_value = LLVMAddFunction(*mod, buf, ret_type);
		LLVMBasicBlockRef entry = LLVMAppendBasicBlock(fn_value, "");
		LLVMBuilderRef builder = LLVMCreateBuilder();
		LLVMPositionBuilderAtEnd(builder, entry);
		
		alloca_params_as_local_vars(builder, fn_value, decl, param_types, num_args, v);

		stmt_codegen(*mod, builder, decl->body, v);
		LLVMDisposeBuilder(builder);
		free(param_types);
		vec_destroy(v);
	}
	else {
		printf("Can't codegen decls of this type yet :(\n");
		exit(1);
	}
}
