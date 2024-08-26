#include "codegen.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include "print.h"
#include "ht.h"
#include <string.h>

static LLVMValueRef codegen_syscall3(LLVMBuilderRef builder, LLVMValueRef args[4])
{
	LLVMTypeRef argtypes[4] = {LLVMTypeOf(args[0]), LLVMTypeOf(args[1]), LLVMTypeOf(args[2]), LLVMTypeOf(args[3])};
	// TODO: don't just return void.
	LLVMTypeRef functy = LLVMFunctionType(LLVMVoidType(), argtypes, 4, 0);
	char *inline_asm = \
		"mov x8, ${0:x}"	"\n"\
		"mov x0, ${1:x}"	"\n"\
		"mov x1, $2"		"\n"\
		"mov x2, ${3:x}"	"\n"\
		"svc #0"		"\n"\
		;
	char constraints[256] = "r,r,r,r,~{x0},~{x1},~{x2},~{x8},~{memory}";
	LLVMValueRef asmcall = LLVMGetInlineAsm(functy, inline_asm, strlen(inline_asm), constraints, strlen(constraints), 1, 1, 0, 0);
	return LLVMBuildCall(builder, asmcall, args, 4, "");
}

static LLVMTypeRef to_llvm_type(ast_type *tp)
{
	switch (tp->kind) {
	case Y_I32:
		return LLVMInt32Type();
	case Y_STRING:
		return LLVMPointerType(LLVMInt8Type(), 0);
	case Y_VOID:
		return LLVMVoidType();
	case Y_POINTER:
		return LLVMPointerType(to_llvm_type(tp->subtype), 0);
	default:
		printf("couldn't convert type\n");
		abort();
	}
}

static LLVMValueRef val_vect_lookup(vect *v, char *name) {
	size_t dummy;
	LLVMValueRef cur;
	for (size_t i = 0  ; i < v->size ; ++i) {
		cur = (LLVMValueRef)v->elements[i];
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

	LLVMDumpModule(ret);
	return ret;
}


static void initializer_codegen(LLVMModuleRef mod,LLVMBuilderRef builder, ast_stmt *stmt, vect *v)
{
	char buffer[BUFFER_MAX_LEN];
	LLVMTypeRef inner_type = to_llvm_type(stmt->decl->typesym->type->subtype);
	LLVMTypeRef arr_type = LLVMArrayType(inner_type, stmt->decl->initializer->size);
	LLVMTypeRef ptr_type = LLVMPointerType(inner_type, 0);

	LLVMValueRef allo = LLVMBuildAlloca(builder, arr_type, "");
	strvec_tostatic(stmt->decl->typesym->symbol, buffer);
	LLVMValueRef ptr = LLVMBuildBitCast(builder, allo, ptr_type, buffer);

	for (size_t i = 0 ; i < stmt->decl->initializer->size ; ++i) {
		LLVMValueRef i_as_llvm = LLVMConstInt(LLVMInt32Type(), i, 0);
		LLVMValueRef a[] = {i_as_llvm};
		LLVMValueRef cur_index = LLVMBuildGEP(builder, ptr, a, 1, "");
		LLVMValueRef cur_expr = expr_codegen(mod, builder, stmt->decl->initializer->elements[i], v);
		LLVMBuildStore(builder, cur_expr, cur_index);
	}

	vect_append(v, (void *)ptr);
}

void stmt_codegen(LLVMModuleRef mod, LLVMBuilderRef builder, ast_stmt *stmt, vect *v, int in_fn)
{
	LLVMValueRef v1;
	LLVMBasicBlockRef b1;
	LLVMBasicBlockRef b2;
	LLVMBasicBlockRef b3;
	LLVMValueRef cur_function;
	ast_stmt *cur;
	char buffer[BUFFER_MAX_LEN];
	int need_retvoid = 1;

	if (!stmt)
		return;

	switch (stmt->kind) {
	case S_BLOCK:
		cur = stmt->body;
		while (cur) {
			if (!in_fn || cur->kind == S_RETURN)
				need_retvoid = 0;
			stmt_codegen(mod, builder, cur, v, 0);
			cur = cur->next;
		}
		if (need_retvoid)
			LLVMBuildRetVoid(builder);
		break;
	case S_EXPR:
		expr_codegen(mod, builder, stmt->expr, v);
		break;
	case S_RETURN:
		if (stmt->expr == 0)
			LLVMBuildRetVoid(builder);
		else
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
		stmt_codegen(mod, builder, stmt->body, v, 0);
		v1 = LLVMGetLastInstruction(b1);
		if (!LLVMIsATerminatorInst(v1))
			LLVMBuildBr(builder, b3);

		LLVMPositionBuilderAtEnd(builder, b2);
		stmt_codegen(mod, builder, stmt->else_body, v, 0);
		LLVMBuildBr(builder, b3);

		LLVMPositionBuilderAtEnd(builder, b3);
		break;
	case S_DECL:
		if (stmt->decl->initializer) {
			initializer_codegen(mod, builder, stmt, v);
		} else {
			strvec_tostatic(stmt->decl->typesym->symbol, buffer);
			v1 = LLVMBuildAlloca(builder, to_llvm_type(stmt->decl->typesym->type), buffer);
			vect_append(v, (void *)v1);
			if (stmt->decl->expr != 0)
				LLVMBuildStore(builder, expr_codegen(mod, builder, stmt->decl->expr, v), v1);
		}
		break;
	case S_WHILE:
		cur_function = LLVMGetBasicBlockParent(LLVMGetInsertBlock(builder));
		v1 = expr_codegen(mod, builder, stmt->expr, v);
		b1 = LLVMAppendBasicBlock(cur_function, "");
		b2 = LLVMAppendBasicBlock(cur_function, "");
		LLVMBuildCondBr(builder, v1, b1, b2);

		LLVMPositionBuilderAtEnd(builder, b1);
		stmt_codegen(mod, builder, stmt->body, v, 0);
		v1 = LLVMGetLastInstruction(b1);
		if (!LLVMIsATerminatorInst(v1)) {
			v1 = expr_codegen(mod, builder, stmt->expr, v);
			LLVMBuildCondBr(builder, v1, b1, b2);
		}

		LLVMPositionBuilderAtEnd(builder, b2);
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

// TODO undo MAX_ARGS? Enforce MAX_ARGS? IDK just decide on something!!
#define MAX_ARGS 32
static void get_fncall_args(LLVMModuleRef mod, LLVMBuilderRef builder,ast_expr *expr, unsigned argno, LLVMValueRef (*args)[MAX_ARGS], vect *v)
{
	if (argno == 0)
		return;
	for (size_t i = 0 ; i < expr->sub_exprs->size ; ++i)
		(*args)[i] = expr_codegen(mod, builder, expr->sub_exprs->elements[i], v);
}

static LLVMValueRef assign_codegen(LLVMModuleRef mod, LLVMBuilderRef builder, ast_expr *expr, vect *nv, LLVMValueRef loc)
{
	LLVMValueRef ret;
	LLVMValueRef tempval;
	ast_expr *temp;
	if (expr->op == T_ASSIGN)
		return LLVMBuildStore(builder, expr_codegen(mod, builder, expr->right, nv), loc);

	switch (expr->op) {
	case T_MUL_ASSIGN:
		temp = expr_init(E_MULDIV, expr->left, expr->right, T_STAR, 0, 0, 0);
		break;
	case T_DIV_ASSIGN:
		temp = expr_init(E_MULDIV, expr->left, expr->right, T_FSLASH, 0, 0, 0);
		break;
	case T_MOD_ASSIGN:
		temp = expr_init(E_MULDIV, expr->left, expr->right, T_PERCENT, 0, 0, 0);
		break;
	case T_ADD_ASSIGN:
		temp = expr_init(E_ADDSUB, expr->left, expr->right, T_PLUS, 0, 0, 0);
		break;
	case T_SUB_ASSIGN:
		temp = expr_init(E_ADDSUB, expr->left, expr->right, T_MINUS, 0, 0, 0);
		break;
	default:
		expr_print(expr);
		printf("\nCan't codegen assignment type with operation token %d\n", expr->op);
		printf("%d\n", T_MUL_ASSIGN);
		abort();
	}
	tempval = expr_codegen(mod, builder, temp, nv);
	ret = LLVMBuildStore(builder, tempval, loc);
	free(temp); // do NOT expr_destroy!!!
	return ret;
}

// this function is courtesy of
// https://stackoverflow.com/questions/65042902/create-and-reference-a-string-literal-via-llvm-c-interface
LLVMValueRef define_string_literal(LLVMModuleRef mod, LLVMBuilderRef builder, const char *source_string, size_t size) {
	LLVMTypeRef str_type = LLVMArrayType(LLVMInt8Type(), size);
	LLVMValueRef str = LLVMAddGlobal(mod, str_type, "");
	LLVMSetInitializer(str, LLVMConstString(source_string, size, 1));
	LLVMSetGlobalConstant(str, 1);
	LLVMSetLinkage(str, LLVMPrivateLinkage);
	LLVMSetUnnamedAddress(str, LLVMGlobalUnnamedAddr);
	LLVMSetAlignment(str, 1);

	LLVMValueRef zero_index = LLVMConstInt(LLVMInt64Type(), 0, 1);
	LLVMValueRef indices[2] = {zero_index, zero_index};
	LLVMValueRef g = LLVMBuildInBoundsGEP2(builder, str_type, str, indices, 2, "");
	return g;
}

// Remaining expr types to codegen:
//E_CHAR_LIT,
//E_PRE_UNARY,
//E_POST_UNARY,
LLVMValueRef expr_codegen(LLVMModuleRef mod, LLVMBuilderRef builder, ast_expr *expr, vect *nv)
{
	char buffer[BUFFER_MAX_LEN];
	LLVMValueRef v;
	//TODO: Strictly enforce a max number of args. Making this static is really nice. Honestly
	// convert all of the little hacks like 'link' expression type to be a static array within
	// the expr struct/decl struct etc.
	LLVMValueRef args[MAX_ARGS];
	LLVMValueRef ret;
	LLVMValueRef syscall_args[4];
	unsigned argno;
	switch (expr->kind) {
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
	case E_SYSCALL:
		for (size_t i = 0 ; i < expr->sub_exprs->size ; ++i) {
			syscall_args[i] = expr_codegen(mod, builder, expr->sub_exprs->elements[i], nv);
		}
		return codegen_syscall3(builder, syscall_args);
	case E_IDENTIFIER:
		strvec_tostatic(expr->name, buffer);
		v = LLVMGetBasicBlockParent(LLVMGetInsertBlock(builder));
		ret = get_param_by_name(v, buffer);
		if (!ret) {
			ret = val_vect_lookup(nv, buffer);
			ret = LLVMBuildLoad(builder, ret, "");
		}
		return ret;
	case E_ASSIGN:
		if (expr->left->kind == E_IDENTIFIER) {
			strvec_tostatic(expr->left->name, buffer);
			v = val_vect_lookup(nv, buffer);
			if (!v) {
				v = LLVMGetBasicBlockParent(LLVMGetInsertBlock(builder));
				v = get_param_by_name(v, buffer);
			}
		} else if (expr->left->kind == E_PRE_UNARY && expr->left->op == T_STAR) {
			v = expr_codegen(mod, builder, expr->left->left, nv);
		} else {
			puts("Can't assign to this expr type right now.");
			exit(1);
		}
		return assign_codegen(mod, builder, expr, nv, v);
	case E_FALSE_LIT:
		return LLVMConstInt(LLVMInt1Type(), 0, 0);
	case E_TRUE_LIT:
		return LLVMConstInt(LLVMInt1Type(), 1, 0);
	case E_STR_LIT:
		return define_string_literal(mod, builder, expr->string_literal->text, expr->string_literal->size);
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
	case E_PRE_UNARY:
		if (expr->op == T_STAR) {
			return LLVMBuildLoad(builder, expr_codegen(mod, builder, expr->left, nv), "");
		} else if (expr->op == T_AMPERSAND) {
			strvec_tostatic(expr->left->name, buffer);
			return val_vect_lookup(nv, buffer);
		}
		//fallthrough
	default:
		printf("can't codegen that expr kind right now.\n");
		exit(1);
	}
}

static LLVMTypeRef *build_param_types(ast_decl *decl)
{
	LLVMTypeRef *ret;
	size_t i;
	vect *arglist = decl->typesym->type->arglist;

	if (!arglist || arglist->size == 0) {
		return 0;
	}
	ret = malloc(sizeof (*ret) * arglist->size);
	for (i = 0 ; i < arglist->size ; ++i) {
		ret[i] = to_llvm_type(arglist_get(arglist, i)->type);
	}
	return ret;
}

static void alloca_params_as_local_vars(LLVMBuilderRef builder, LLVMValueRef fn, ast_decl *decl, LLVMTypeRef *param_types, vect *vc)
{
	char buf[BUFFER_MAX_LEN];
	LLVMValueRef arg;
	LLVMValueRef v;
	vect *arglist = decl->typesym->type->arglist;
	if (!arglist || arglist->size == 0)
		return;
	for (size_t i = 0 ; i < arglist->size ; ++i) {
		strvec_tostatic(arglist_get(arglist, i)->symbol, buf);
		arg = LLVMGetParam(fn, i);
		v = LLVMBuildAlloca(builder, param_types[i], buf);
		LLVMBuildStore(builder, arg, v);
		vect_append(vc, (void *)v);
	}
	
}

void decl_codegen(LLVMModuleRef *mod, ast_decl *decl)
{
	if (!decl)
		return;
	if (decl->typesym->type->kind == Y_FUNCTION) {

		char buf[BUFFER_MAX_LEN];
		vect *v = vect_init(4);
		LLVMTypeRef *param_types = build_param_types(decl);
		vect *arglist = decl->typesym->type->arglist;
		size_t size = !arglist ? 0 : arglist->size;
		LLVMTypeRef ret_type = LLVMFunctionType(to_llvm_type(decl->typesym->type->subtype), param_types, size, 0);

		strvec_tostatic(decl->typesym->symbol, buf);
		LLVMValueRef fn_value = LLVMAddFunction(*mod, buf, ret_type);
		LLVMBasicBlockRef entry = LLVMAppendBasicBlock(fn_value, "");
		LLVMBuilderRef builder = LLVMCreateBuilder();
		LLVMPositionBuilderAtEnd(builder, entry);

		alloca_params_as_local_vars(builder, fn_value, decl, param_types, v);

		stmt_codegen(*mod, builder, decl->body, v, 1);
		LLVMDisposeBuilder(builder);
		free(param_types);
		vect_destroy(v);
	}
	else {
		printf("Can't codegen decls of this type yet :(\n");
		exit(1);
	}
}
