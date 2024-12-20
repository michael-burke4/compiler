#include "ast.h"
#include "codegen.h"
#include "error.h"
#include "ht.h"
#include "symbol_table.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CTXT(mod) (LLVMGetModuleContext(mod))

extern struct stack *sym_tab;
LLVMTargetDataRef td = NULL;

static LLVMTypeRef to_llvm_type(LLVMModuleRef mod, ast_type *tp)
{
	char buffer[BUFFER_MAX_LEN];
	LLVMContextRef ctxt = CTXT(mod);
	switch (tp->kind) {
	case Y_I32:
	case Y_U32:
		return LLVMInt32TypeInContext(ctxt);
	case Y_I64:
	case Y_U64:
		return LLVMInt64TypeInContext(ctxt);
	case Y_BOOL:
		return LLVMInt1TypeInContext(ctxt);
	case Y_VOID:
		return LLVMVoidTypeInContext(ctxt);
	case Y_POINTER:
	case Y_CONSTPTR:
		return LLVMPointerType(LLVMInt8TypeInContext(ctxt), 0);
	case Y_CHAR:
		return LLVMInt8TypeInContext(ctxt);
	case Y_STRUCT:
		strvec_tostatic(tp->name, buffer);
		return LLVMGetTypeByName(mod, buffer);
	// LCOV_EXCL_START
	default:
		fprintf(stderr, "couldn't convert type\n");
		abort();
	// LCOV_EXCL_STOP
	}
}

LLVMModuleRef module_codegen(LLVMContextRef ctxt, ast_decl *start, char *module_name)
{
	LLVMModuleRef ret = LLVMModuleCreateWithNameInContext(module_name, ctxt);
	char *triple = LLVMGetDefaultTargetTriple();
	LLVMTargetRef tgt = NULL;
	LLVMGetTargetFromTriple(triple, &tgt, NULL);

	// TODO: make this customizable.
	LLVMTargetMachineRef tm = LLVMCreateTargetMachine(tgt, triple, "generic", "", LLVMCodeGenLevelNone, LLVMRelocDefault, LLVMCodeModelDefault);
	td = LLVMCreateTargetDataLayout(tm);


	while (start) {
		decl_codegen(&ret, start);
		start = start->next;
	}

	LLVMDisposeMessage(triple);
	LLVMDisposeTargetData(td);
	LLVMDisposeTargetMachine(tm);

	return ret;
}

// This function is a little intimidating: but it is quite simple.
// First it allocates space for an array on the stack (with element type determined by decl subtype)
// then it populates each array space with the corresponding element from the initializer
// then it builds a cast of the array type to a pointer type so it can be treated like any other ptr.
//	this casted pointer is what is used later.
static void initializer_codegen(LLVMModuleRef mod, LLVMTypeRef typ, LLVMBuilderRef builder, ast_stmt *stmt)
{
	char buffer[BUFFER_MAX_LEN];
	strvec_tostatic(stmt->decl->typesym->symbol, buffer);
	LLVMValueRef idx;

	LLVMTypeRef llvmified_inner = to_llvm_type(mod, stmt->decl->typesym->type->subtype);

	LLVMTypeRef array_type = LLVMArrayType(llvmified_inner, stmt->decl->initializer->size);
	LLVMValueRef init = LLVMBuildAlloca(builder, array_type, "");
	for (size_t i = 0 ; i < stmt->decl->initializer->size ; ++i) { // clang uses memcpy for this! would be much better!
		idx = LLVMConstInt(LLVMInt32TypeInContext(CTXT(mod)), i, 0);
		LLVMValueRef gep = LLVMBuildGEP2(builder, typ, init, &idx, 1, "");
		LLVMValueRef value = expr_codegen(mod, builder, stmt->decl->initializer->elements[i], 0);
		LLVMBuildStore(builder, value, gep);
	}
	LLVMValueRef alloca2 = LLVMBuildAlloca(builder, LLVMPointerType(llvmified_inner, 0), buffer);
	idx = LLVMConstInt(LLVMInt32Type(), 0, 0);
	LLVMBuildStore(builder, LLVMBuildPointerCast(builder, init, LLVMPointerType(llvmified_inner, 0), ""), alloca2);
	scope_bind(alloca2, stmt->decl->typesym->symbol);
}

static int followed_by_branch(ast_stmt *stmt) {
	if (stmt == NULL)
		return 0;
	stmt = stmt->next;
	while (stmt != NULL) {
		if (stmt->kind == S_IFELSE
				|| stmt->kind == S_WHILE || stmt->kind == S_RETURN
				|| stmt->kind == S_BREAK || stmt->kind == S_CONTINUE)
			return 1;
		stmt = stmt->next;
	}
	return 0;
}

static void ifelse_codegen(LLVMModuleRef mod, LLVMBuilderRef builder, ast_stmt *stmt, LLVMBasicBlockRef p_con) {
	LLVMValueRef cur_function;
	LLVMContextRef ctxt = CTXT(mod);
	cur_function = LLVMGetBasicBlockParent(LLVMGetInsertBlock(builder));

	LLVMBasicBlockRef iff = LLVMAppendBasicBlockInContext(ctxt, cur_function, "if");
	LLVMBasicBlockRef els = LLVMAppendBasicBlockInContext(ctxt, cur_function, "else");
	LLVMBasicBlockRef con = p_con;
	if (stmt->next != NULL)
		con = LLVMAppendBasicBlockInContext(ctxt, cur_function, "ifelse_continue");

	// if (condition)
	LLVMValueRef v1 = expr_codegen(mod, builder, stmt->expr, 0);
	v1 = LLVMBuildCondBr(builder, v1, iff, els);

	// {
	//	// if branch code
	// }
	LLVMPositionBuilderAtEnd(builder, iff);
	stmt_codegen(mod, builder, stmt->body, con);
	v1 = LLVMGetLastInstruction(iff);
	if (!LLVMIsATerminatorInst(v1))
		LLVMBuildBr(builder, con);

	// else {
	//	// else branch code
	// }
	LLVMPositionBuilderAtEnd(builder, els);
	stmt_codegen(mod, builder, stmt->else_body, con);
	v1 = LLVMGetLastInstruction(els);
	if (v1 == NULL || !LLVMIsATerminatorInst(v1))
		LLVMBuildBr(builder, con);

	if (con == NULL)
		return;

	LLVMPositionBuilderAtEnd(builder, con);
	if (p_con != con && !followed_by_branch(stmt)) {
		v1 = LLVMBuildBr(builder, p_con);
		LLVMPositionBuilderBefore(builder, v1);
	}
}

static void distribute_breaks_and_continues(ast_stmt *stmt, LLVMBasicBlockRef brk, LLVMBasicBlockRef con) {
	while (stmt != NULL) {
		if (stmt->kind == S_BREAK)
			stmt->extra = brk;
		else if (stmt->kind == S_CONTINUE)
			stmt->extra = con;
		else if (stmt->kind == S_IFELSE) {
			distribute_breaks_and_continues(stmt->body->body, brk, con);
		}
		stmt = stmt->next;
	}
}

static void while_codegen(LLVMModuleRef mod, LLVMBuilderRef builder, ast_stmt *stmt, LLVMBasicBlockRef p_con) {
	LLVMValueRef cur_function;
	LLVMContextRef ctxt = CTXT(mod);
	cur_function = LLVMGetBasicBlockParent(LLVMGetInsertBlock(builder));


	LLVMBasicBlockRef cod = LLVMAppendBasicBlockInContext(ctxt, cur_function, "cond");
	LLVMBasicBlockRef whi = LLVMAppendBasicBlockInContext(ctxt, cur_function, "while");
	LLVMBasicBlockRef con = p_con;
	if (stmt->next != NULL)
		con = LLVMAppendBasicBlockInContext(ctxt, cur_function, "while_continue");

	distribute_breaks_and_continues(stmt->body->body, con, cod);

	LLVMBuildBr(builder, cod);
	// while (condition)
	LLVMPositionBuilderAtEnd(builder, cod);
	LLVMValueRef v1 = expr_codegen(mod, builder, stmt->expr, 0);
	v1 = LLVMBuildCondBr(builder, v1, whi, con);

	// {
	//	// while code
	// }
	LLVMPositionBuilderAtEnd(builder, whi);
	stmt_codegen(mod, builder, stmt->body, cod);
	v1 = LLVMGetLastInstruction(whi);
	if (v1 == NULL || !LLVMIsATerminatorInst(v1))
		LLVMBuildBr(builder, cod);


	LLVMPositionBuilderAtEnd(builder, con);
	if (p_con != con && !followed_by_branch(stmt)) {
		v1 = LLVMBuildBr(builder, p_con);
		LLVMPositionBuilderBefore(builder, v1);
	}
}

static void asm_codegen(LLVMModuleRef mod, LLVMBuilderRef builder, ast_stmt *stmt)
{
	asm_struct *a = stmt->asm_obj;
	vect *in_ops = a->in_operands;
	vect *out_ops = a->out_operands;
	size_t in_s = in_ops == NULL ? 0 : in_ops->size;
	size_t out_s = out_ops == NULL ? 0 : out_ops->size;

	vect *val_vect = in_ops == NULL ? NULL : vect_init(in_s);
	vect *type_vect = in_ops == NULL ? NULL : vect_init(in_s);

	vect *ret_types = out_ops == NULL ? NULL : vect_init(out_s);

	for (size_t i = 0 ; out_ops != NULL && i < out_s ; ++i) {
		LLVMTypeRef t = LLVMTypeOf(expr_codegen(mod, builder, vect_get(out_ops, i), 0));
		vect_append(ret_types, t);
	}

	for (size_t i = 0 ; in_ops != NULL && i < in_s ; ++i) {
		LLVMValueRef v = expr_codegen(mod, builder, vect_get(in_ops, i), 0);
		vect_append(val_vect, v);
		vect_append(type_vect, LLVMTypeOf(v));
	}


	LLVMTypeRef ret_type = ret_types == NULL ? LLVMVoidTypeInContext(CTXT(mod))
					: LLVMStructTypeInContext(
						CTXT(mod),
						(LLVMTypeRef *)ret_types->elements,
						ret_types->size,
						0
					);
	LLVMTypeRef fn_type = LLVMFunctionType(ret_type,
				type_vect == NULL ? NULL : (LLVMTypeRef *)type_vect->elements,
				type_vect == NULL ? 0 : type_vect->size,
				0);
	LLVMValueRef asm_call = LLVMGetInlineAsm(fn_type,
					a->code->string_literal->text,
					a->code->string_literal->size,
					a->constraints == NULL ? NULL : a->constraints->string_literal->text,
					a->constraints == NULL ? 0 : a->constraints->string_literal->size,
					1, 1, 0, 0);
	LLVMValueRef res = LLVMBuildCall2(builder, fn_type, asm_call,
				val_vect == NULL ? NULL : (LLVMValueRef *)val_vect->elements,
				in_s, "");

	for (size_t i = 0 ; in_ops != NULL && i < out_s ; ++i) {
		ast_expr *e = vect_get(out_ops, i);
		LLVMValueRef loc = scope_lookup(e->name);
		LLVMValueRef to_store = LLVMBuildExtractValue(builder, res, i, "");
		LLVMBuildStore(builder, to_store, loc);
	}

	vect_destroy(val_vect);
	vect_destroy(type_vect);
	vect_destroy(ret_types);
}

void stmt_codegen(LLVMModuleRef mod, LLVMBuilderRef builder, ast_stmt *stmt, LLVMBasicBlockRef p_con)
{
	LLVMValueRef v1;
	ast_stmt *cur;
	char buffer[BUFFER_MAX_LEN];

	if (stmt == NULL)
		return;

	switch (stmt->kind) {
	case S_ASM:
		asm_codegen(mod, builder, stmt);
		break;
	case S_BLOCK:
		scope_enter();
		cur = stmt->body;
		while (cur) {
			stmt_codegen(mod, builder, cur, p_con);
			cur = cur->next;
		}
		scope_exit();
		break;
	case S_EXPR:
		expr_codegen(mod, builder, stmt->expr, 0);
		break;
	case S_RETURN:
		if (stmt->expr == NULL)
			LLVMBuildRetVoid(builder);
		else
			LLVMBuildRet(builder, expr_codegen(mod, builder, stmt->expr, 0));
		break;
	case S_IFELSE:
		ifelse_codegen(mod, builder, stmt, p_con);
		break;
	case S_DECL:
		if (stmt->decl->initializer != NULL) {
			initializer_codegen(mod, to_llvm_type(mod, stmt->decl->typesym->type->subtype), builder, stmt);
		} else {
			strvec_tostatic(stmt->decl->typesym->symbol, buffer);
			v1 = LLVMBuildAlloca(builder, to_llvm_type(mod, stmt->decl->typesym->type), buffer);
			scope_bind(v1, stmt->decl->typesym->symbol);
			if (stmt->decl->expr != NULL)
				LLVMBuildStore(builder, expr_codegen(mod, builder, stmt->decl->expr, 0), v1);
		}
		break;
	case S_WHILE:
		while_codegen(mod, builder, stmt, p_con);
		break;
	case S_CONTINUE:
	case S_BREAK:
		LLVMBuildBr(builder, stmt->extra);
		break;
	// LCOV_EXCL_START
	case S_ERROR:
		fputs("CRITICAL: reached unexpected error condition.", stderr);
		break;
	// LCOV_EXCL_STOP
	}
}

// TODO undo MAX_ARGS? Enforce MAX_ARGS? IDK just decide on something!!
#define MAX_ARGS 32
static void get_fncall_args(LLVMModuleRef mod, LLVMBuilderRef builder, ast_expr *expr, unsigned argno, LLVMValueRef (*args)[MAX_ARGS], LLVMTypeRef (*argtypes)[MAX_ARGS])
{
	if (argno == 0)
		return;
	for (size_t i = 0 ; i < expr->sub_exprs->size ; ++i) {
		(*args)[i] = expr_codegen(mod, builder, expr->sub_exprs->elements[i], 0);
		(*argtypes)[i] = to_llvm_type(mod, ((ast_expr *)(expr->sub_exprs->elements[i]))->type);
	}
}

static LLVMValueRef assign_codegen(LLVMModuleRef mod, LLVMBuilderRef builder, ast_expr *expr, LLVMValueRef loc)
{
	LLVMValueRef ret;
	LLVMValueRef tempval;
	ast_expr *temp;
	if (expr->op == T_ASSIGN)
		return LLVMBuildStore(builder, expr_codegen(mod, builder, expr->right, 0), loc);

	switch (expr->op) {
	case T_MUL_ASSIGN:
		temp = expr_init(E_MULDIV, expr->left, expr->right, T_STAR, NULL, 0, NULL);
		break;
	case T_DIV_ASSIGN:
		temp = expr_init(E_MULDIV, expr->left, expr->right, T_FSLASH, NULL, 0, NULL);
		break;
	case T_MOD_ASSIGN:
		temp = expr_init(E_MULDIV, expr->left, expr->right, T_PERCENT, NULL, 0, NULL);
		break;
	case T_ADD_ASSIGN:
		temp = expr_init(E_ADDSUB, expr->left, expr->right, T_PLUS, NULL, 0, NULL);
		break;
	case T_SUB_ASSIGN:
		temp = expr_init(E_ADDSUB, expr->left, expr->right, T_MINUS, NULL, 0, NULL);
		break;
	case T_BW_AND_ASSIGN:
		temp = expr_init(E_BW_AND, expr->left, expr->right, T_AMPERSAND, NULL, 0, NULL);
		break;
	case T_BW_OR_ASSIGN:
		temp = expr_init(E_BW_OR, expr->left, expr->right, T_BW_OR, NULL, 0, NULL);
		break;
	case T_XOR_ASSIGN:
		temp = expr_init(E_BW_XOR, expr->left, expr->right, T_XOR, NULL, 0, NULL);
		break;
	// LCOV_EXCL_START
	default:
		fprintf(stderr, "Can't codegen assignment type with operation token %d\n", expr->op);
		abort();
	// LCOV_EXCL_STOP
	}
	tempval = expr_codegen(mod, builder, temp, 0);
	ret = LLVMBuildStore(builder, tempval, loc);
	free(temp); // do NOT expr_destroy!!!
	return ret;
}

// this function is courtesy of
// https://stackoverflow.com/questions/65042902/create-and-reference-a-string-literal-via-llvm-c-interface
LLVMValueRef define_const_string_literal(LLVMModuleRef mod, LLVMBuilderRef builder, const char *source_string, size_t size) {
	LLVMTypeRef str_type = LLVMArrayType(LLVMInt8TypeInContext(CTXT(mod)), size);
	LLVMValueRef str = LLVMAddGlobal(mod, str_type, "");
	LLVMSetInitializer(str, LLVMConstStringInContext(CTXT(mod), source_string, size, 1));
	LLVMSetGlobalConstant(str, 1);
	LLVMSetLinkage(str, LLVMPrivateLinkage);
	LLVMSetUnnamedAddress(str, LLVMGlobalUnnamedAddr);
	LLVMSetAlignment(str, 1);

	LLVMValueRef zero_index = LLVMConstInt(LLVMInt64TypeInContext(CTXT(mod)), 0, 1);
	LLVMValueRef indices[2] = {zero_index, zero_index};
	LLVMValueRef g = LLVMBuildInBoundsGEP2(builder, str_type, str, indices, 2, "");
	return g;
}

static size_t get_member_position(ast_decl *decl, strvec *name) {
	vect *arglist = decl->typesym->type->arglist;
	for (size_t i = 0 ; i < arglist->size ; ++i) {
		if (strvec_equals(arglist_get(arglist, i)->symbol, name)) {
			return i;
		}
	}
	// LCOV_EXCL_START
	eputs("DIDN'T FIND REQUESTED STRUCT MEMBER. FATAL ERROR.");
	exit(1);
	// LCOV_EXCL_STOP
}

static LLVMValueRef log_or_codegen(LLVMModuleRef mod, LLVMBuilderRef builder, ast_expr *expr)
{
	LLVMValueRef cur_function = LLVMGetBasicBlockParent(LLVMGetInsertBlock(builder));
	LLVMBasicBlockRef L = LLVMAppendBasicBlock(cur_function, "L");
	LLVMBasicBlockRef R = LLVMAppendBasicBlock(cur_function, "R");
	LLVMBasicBlockRef C = LLVMAppendBasicBlock(cur_function, "C");
	LLVMValueRef l_cond = expr_codegen(mod, builder, expr->left, 0);
	LLVMBuildCondBr(builder, l_cond, L, R);

	LLVMPositionBuilderAtEnd(builder, L);
	LLVMBuildBr(builder, C);

	LLVMPositionBuilderAtEnd(builder, R);
	LLVMValueRef r_cond = expr_codegen(mod, builder, expr->right, 0);
	LLVMBuildBr(builder, C);

	LLVMPositionBuilderAtEnd(builder, C);
	LLVMTypeRef bool_type = LLVMInt1TypeInContext(CTXT(mod));
	LLVMValueRef phi = LLVMBuildPhi(builder, bool_type, "");
	LLVMAddIncoming(phi, (LLVMValueRef[]){l_cond, r_cond}, (LLVMBasicBlockRef[]){L, R}, 2);

	return phi;
}

static LLVMValueRef log_and_codegen(LLVMModuleRef mod, LLVMBuilderRef builder, ast_expr *expr)
{
	LLVMValueRef cur_function = LLVMGetBasicBlockParent(LLVMGetInsertBlock(builder));
	LLVMBasicBlockRef L = LLVMAppendBasicBlock(cur_function, "L");
	LLVMBasicBlockRef R = LLVMAppendBasicBlock(cur_function, "R");
	LLVMBasicBlockRef C = LLVMAppendBasicBlock(cur_function, "C");
	LLVMValueRef l_cond = expr_codegen(mod, builder, expr->left, 0);

	// note R and L are flipped here from log_or.
	// If the l cond is false, we won't bother checking R cond
	// and jump right to the continue block and stuff false into the phi.
	// Otherwise the l cond is true and we need to check the r cond.
	LLVMBuildCondBr(builder, l_cond, R, L);

	LLVMPositionBuilderAtEnd(builder, L);
	LLVMBuildBr(builder, C);

	LLVMPositionBuilderAtEnd(builder, R);
	LLVMValueRef r_cond = expr_codegen(mod, builder, expr->right, 0);
	LLVMBuildBr(builder, C);

	LLVMPositionBuilderAtEnd(builder, C);
	LLVMTypeRef bool_type = LLVMInt1TypeInContext(CTXT(mod));
	LLVMValueRef phi = LLVMBuildPhi(builder, bool_type, "");
	LLVMAddIncoming(phi, (LLVMValueRef[]){l_cond, r_cond}, (LLVMBasicBlockRef[]){L, R}, 2);

	return phi;
}

LLVMValueRef pre_unary_codegen(LLVMModuleRef mod, LLVMBuilderRef builder, ast_expr *expr, int store_ctxt)
{
	LLVMValueRef v = NULL;
	unsigned long long s = 0;
	LLVMTypeRef t = NULL;
	switch (expr->op) {
	case T_STAR:
		v = expr_codegen(mod, builder, expr->left, store_ctxt);
		if (store_ctxt) {
			return v;
		}
		return LLVMBuildLoad2(builder, to_llvm_type(mod, expr->type), v, "");
	case T_AMPERSAND:
		if (expr->left->name != NULL)
			return scope_lookup(expr->left->name);
		return expr_codegen(mod, builder, expr->left, 1);
	case T_MINUS:
		v = expr_codegen(mod, builder, expr->left, 0);
		return LLVMBuildSub(builder, LLVMConstInt(LLVMTypeOf(v), 0, 0), v, "");
	case T_BW_NOT:
		v = expr_codegen(mod, builder, expr->left, 0);
		return LLVMBuildXor(builder, LLVMConstInt(LLVMTypeOf(v), -1, 0), v, "");
	case T_NOT:
		v = expr_codegen(mod, builder, expr->left, 0);
		return LLVMBuildXor(builder, LLVMConstInt(LLVMTypeOf(v), 1, 0), v, "");
	case T_SIZEOF:
		v = expr_codegen(mod, builder, expr->left, 0);
		s = LLVMABISizeOfType(td, LLVMTypeOf(v));
		// TODO: make a size_t/usize type that will vary with target triple or target
		// data or whatever.
		t = LLVMInt64TypeInContext(CTXT(mod));
		return LLVMConstInt(t, s, 0);
	// LCOV_EXCL_START
	default:
		fprintf(stderr, "can't codegen that expr kind right now.\n");
		exit(1);
	// LCOV_EXCL_STOP
	}
}

static LLVMValueRef cast_codegen(LLVMModuleRef mod, LLVMBuilderRef builder, ast_expr *expr, int store_ctxt)
{
	ast_type *from_cast_t = expr->left->type;
	ast_type *to_cast_t = expr->type;
	if (is_integer(from_cast_t)) {
		if (is_integer(to_cast_t)) {
			if (TYPE_WIDTH(to_cast_t->kind) < TYPE_WIDTH(from_cast_t->kind)) {
				return LLVMBuildTrunc(builder, expr_codegen(mod, builder, expr->left, store_ctxt),
						to_llvm_type(mod, to_cast_t), "");
			} else if (IS_UNSIGNED(expr)) {
				return LLVMBuildZExt(builder, expr_codegen(mod,
						builder, expr->left, store_ctxt),
						to_llvm_type(mod, to_cast_t), "");
			} else {
				return LLVMBuildSExt(builder, expr_codegen(mod,
						builder, expr->left, store_ctxt),
						to_llvm_type(mod, to_cast_t), "");
			}
		} else if (to_cast_t->kind == Y_POINTER) {
			return LLVMBuildIntToPtr(builder, expr_codegen(mod, builder, expr->left, store_ctxt),
						to_llvm_type(mod, to_cast_t), "");
		}
	}

	else if (from_cast_t->kind == Y_POINTER) {
		if (to_cast_t->kind == Y_POINTER) {
			return expr_codegen(mod, builder, expr->left, store_ctxt);
		}
		else if (is_integer(to_cast_t)) {
			return LLVMBuildPtrToInt(builder, expr_codegen(mod, builder, expr->left, store_ctxt),
						to_llvm_type(mod, to_cast_t), "");
		}
	}
	// LCOV_EXCL_START
	fprintf(stderr, "Can't codegen a cast to this type!\n");
	exit(1);
	// LCOV_EXCL_STOP
}



LLVMValueRef expr_codegen(LLVMModuleRef mod, LLVMBuilderRef builder, ast_expr *expr, int store_ctxt)
{
	char buffer[BUFFER_MAX_LEN];
	LLVMValueRef v;
	LLVMValueRef v2;
	LLVMValueRef args[MAX_ARGS];
	LLVMTypeRef argtypes[MAX_ARGS];
	LLVMValueRef ret = NULL;
	LLVMTypeRef t;
	buffer[0] = '\0';
	unsigned argno;
	switch (expr->kind) {
	case E_INT_LIT:
		t = to_llvm_type(mod, expr->type);
		return LLVMConstInt(t, (unsigned long long)expr->num, 0);
	case E_SHIFT:
		if (expr->op == T_RSHIFT)
			if (IS_UNSIGNED(expr->left))
				return LLVMBuildLShr(builder, expr_codegen(mod, builder, expr->left, 0), expr_codegen(mod, builder, expr->right, 0), "");
			else
				return LLVMBuildAShr(builder, expr_codegen(mod, builder, expr->left, 0), expr_codegen(mod, builder, expr->right, 0), "");
		else
			return LLVMBuildShl(builder, expr_codegen(mod, builder, expr->left, 0), expr_codegen(mod, builder, expr->right, 0), "");
	case E_BW_OR:
		return LLVMBuildOr(builder, expr_codegen(mod, builder, expr->left, 0), expr_codegen(mod, builder, expr->right, 0), "");
	case E_BW_XOR:
		return LLVMBuildXor(builder, expr_codegen(mod, builder, expr->left, 0), expr_codegen(mod, builder, expr->right, 0), "");
	case E_BW_AND:
		return LLVMBuildAnd(builder, expr_codegen(mod, builder, expr->left, 0), expr_codegen(mod, builder, expr->right, 0), "");
	case E_MULDIV:
		if (expr->op == T_STAR)
			return LLVMBuildMul(builder, expr_codegen(mod, builder, expr->left, 0), expr_codegen(mod, builder, expr->right, 0), "");
		if (IS_UNSIGNED(expr->left)) {
			if (expr->op == T_PERCENT)
				return LLVMBuildURem(builder, expr_codegen(mod, builder, expr->left, 0), expr_codegen(mod, builder, expr->right, 0), "");
			else
				return LLVMBuildUDiv(builder, expr_codegen(mod, builder, expr->left, 0), expr_codegen(mod, builder, expr->right, 0), "");
		}
		if (expr->op == T_PERCENT)
			return LLVMBuildSRem(builder, expr_codegen(mod, builder, expr->left, 0), expr_codegen(mod, builder, expr->right, 0), "");
		else
			return LLVMBuildSDiv(builder, expr_codegen(mod, builder, expr->left, 0), expr_codegen(mod, builder, expr->right, 0), "");
	case E_ADDSUB:
		// TODO: nsw flag
		// TODO: pointer math
		if (expr->op == T_MINUS)
			v = LLVMBuildSub(builder, expr_codegen(mod, builder, expr->left, 0), expr_codegen(mod, builder, expr->right, 0), "");
		else
			v = LLVMBuildAdd(builder, expr_codegen(mod, builder, expr->left, 0), expr_codegen(mod, builder, expr->right, 0), "");
		return v;

	// Because the lhs and rhs of logical and/or must be bools, this is fine.
	// This wouldn't work if any old int could be in a logical and/or expr.
	case E_LOG_OR:
		return log_or_codegen(mod, builder, expr);
	case E_LOG_AND:
		return log_and_codegen(mod, builder, expr);
	case E_EQUALITY:
		if (expr->op == T_EQ)
			return LLVMBuildICmp(builder, LLVMIntEQ, expr_codegen(mod, builder, expr->left, 0), expr_codegen(mod, builder, expr->right, 0), "");
		else
			return LLVMBuildICmp(builder, LLVMIntNE, expr_codegen(mod, builder, expr->left, 0), expr_codegen(mod, builder, expr->right, 0), "");
	case E_FNCALL:
		strvec_tostatic(expr->name, buffer);
		v = LLVMGetNamedFunction(mod, buffer);
		argno = LLVMCountParams(v);
		get_fncall_args(mod, builder, expr, argno, &args, &argtypes);
		LLVMTypeRef fn_t = LLVMFunctionType(to_llvm_type(mod, expr->type), argtypes, argno, 0);
		return LLVMBuildCall2(builder, fn_t, v, args, argno, "");
	case E_IDENTIFIER:
		ret = scope_lookup(expr->name);
		ret = LLVMBuildLoad2(builder, to_llvm_type(mod, expr->type), ret, "");
		return ret;
	case E_ASSIGN:
		if (expr->left->kind == E_IDENTIFIER) {
			v = scope_lookup(expr->left->name);
		} else if (expr->left->kind == E_PRE_UNARY && expr->left->op == T_STAR) {
			v = expr_codegen(mod, builder, expr->left->left, 1);
		} else if (expr->left->kind == E_POST_UNARY && (expr->left->op == T_LBRACKET || expr->left->op == T_PERIOD)) {
			v = expr_codegen(mod, builder, expr->left, 1);
		}
		//LCOV_EXCL_START
		else {
			eputs("CRITICAL: Attempting assignment to unexpected expression type?");
			exit(1);
		}
		//LCOV_EXCL_STOP
		return assign_codegen(mod, builder, expr, v);
	case E_FALSE_LIT:
		return LLVMConstInt(LLVMInt1TypeInContext(CTXT(mod)), 0, 0);
	case E_TRUE_LIT:
		return LLVMConstInt(LLVMInt1TypeInContext(CTXT(mod)), 1, 0);
	case E_STR_LIT:
		return define_const_string_literal(mod, builder, expr->string_literal->text, expr->string_literal->size);
	case E_CHAR_LIT:
		return LLVMConstInt(LLVMInt8TypeInContext(CTXT(mod)), (int)expr->string_literal->text[0], 0);
	case E_INEQUALITY:
		if (expr->op == T_LT)
			if (IS_UNSIGNED(expr->left))
				return LLVMBuildICmp(builder, LLVMIntULT, expr_codegen(mod, builder, expr->left, 0), expr_codegen(mod, builder, expr->right, 0), "");
			else
				return LLVMBuildICmp(builder, LLVMIntSLT, expr_codegen(mod, builder, expr->left, 0), expr_codegen(mod, builder, expr->right, 0), "");
		else if (expr->op == T_LTE)
			if (IS_UNSIGNED(expr->left))
				return LLVMBuildICmp(builder, LLVMIntULE, expr_codegen(mod, builder, expr->left, 0), expr_codegen(mod, builder, expr->right, 0), "");
			else
				return LLVMBuildICmp(builder, LLVMIntSLE, expr_codegen(mod, builder, expr->left, 0), expr_codegen(mod, builder, expr->right, 0), "");
		else if (expr->op == T_GT)
			if (IS_UNSIGNED(expr->left))
				return LLVMBuildICmp(builder, LLVMIntUGT, expr_codegen(mod, builder, expr->left, 0), expr_codegen(mod, builder, expr->right, 0), "");
			else
				return LLVMBuildICmp(builder, LLVMIntSGT, expr_codegen(mod, builder, expr->left, 0), expr_codegen(mod, builder, expr->right, 0), "");
		else
			if (IS_UNSIGNED(expr->left))
				return LLVMBuildICmp(builder, LLVMIntUGE, expr_codegen(mod, builder, expr->left, 0), expr_codegen(mod, builder, expr->right, 0), "");
			else
				return LLVMBuildICmp(builder, LLVMIntSGE, expr_codegen(mod, builder, expr->left, 0), expr_codegen(mod, builder, expr->right, 0), "");
	case E_PAREN:
		return expr_codegen(mod, builder, expr->left, store_ctxt);
	case E_POST_UNARY:
		if (expr->op == T_LBRACKET) {
			v = expr_codegen(mod, builder, expr->left, 0);
			v2 = expr_codegen(mod, builder, expr->right, 0);
			v = LLVMBuildGEP2(builder, to_llvm_type(mod, expr->left->type->subtype), v, &v2, 1, "");
			if (!store_ctxt)
				v = LLVMBuildLoad2(builder, to_llvm_type(mod, expr->type), v, "");
			return v;
		} else if (expr->op == T_PERIOD) {
			if (expr->left->kind == E_IDENTIFIER) {
				v = scope_lookup(expr->left->name);
			} else {
				v = expr_codegen(mod, builder, expr->left, 1);
			}
			// TODO: clean up all this to static back to strvec nonsense.
			strvec_tostatic(expr->left->type->name, buffer);
			t = LLVMGetTypeByName2(CTXT(mod), buffer);

			strvec *name_vec = strvec_init_str(buffer);

			// Look up the decl in sym tab so we can get field names
			ast_decl *struct_decl;
			struct_decl = scope_lookup(name_vec);
			strvec_destroy(name_vec);

			size_t ind = get_member_position(struct_decl, expr->right->name);

			// set up gep indices
			v2 = LLVMBuildStructGEP2(builder, t, v, ind, "");
			if (!store_ctxt) {
				return LLVMBuildLoad2(builder, to_llvm_type(mod, expr->type), v2, "");
			}
			return v2;
		}
		// LCOV_EXCL_START
		else {
			eputs("can't codegen this post unary expr type yet");
			exit(1);
		}
		// LCOV_EXCL_STOP
	case E_CAST:
		return cast_codegen(mod, builder, expr, store_ctxt);
	case E_PRE_UNARY:
		ret = pre_unary_codegen(mod, builder, expr, store_ctxt);
	}
	return ret; // This is dumb but silences a warning.
}

static LLVMTypeRef *build_param_types(LLVMModuleRef mod, ast_decl *decl)
{
	LLVMTypeRef *ret;
	size_t i;
	vect *arglist = decl->typesym->type->arglist;

	if (arglist == NULL || arglist->size == 0) {
		return NULL;
	}
	ret = malloc(sizeof (*ret) * arglist->size);
	for (i = 0 ; i < arglist->size ; ++i) {
		ret[i] = to_llvm_type(mod, arglist_get(arglist, i)->type);
	}
	return ret;
}

static void alloca_params_as_local_vars(LLVMBuilderRef builder, LLVMValueRef fn, ast_decl *decl, LLVMTypeRef *param_types)
{
	char buf[BUFFER_MAX_LEN];
	LLVMValueRef arg;
	LLVMValueRef v;
	vect *arglist = decl->typesym->type->arglist;
	if (arglist == NULL || arglist->size == 0)
		return;
	for (size_t i = 0 ; i < arglist->size ; ++i) {
		strvec_tostatic(arglist_get(arglist, i)->symbol, buf);
		arg = LLVMGetParam(fn, i);
		v = LLVMBuildAlloca(builder, param_types[i], buf);
		LLVMBuildStore(builder, arg, v);
		scope_bind(v, arglist_get(arglist,i)->symbol);
	}
}


static void define_struct(LLVMModuleRef mod, ast_decl *decl) {
	char buf[BUFFER_MAX_LEN];
	strvec_tostatic(decl->typesym->symbol, buf);
	vect *al = decl->typesym->type->arglist;
	size_t sz = al->size;
	vect *members = vect_init(sz);

	// A brief explaination of scope-binding structs:
	// Structs can only be defined at the top level, so scope binding
	//	them isn't reaaaally needed, but it's easier than keeping a
	//	second list just for struct defs.
	// Structs typesyms need to be accessible easily so that you can select struct fields
	//	by name, as LLVM doesn't keep track of the field names. It only keeps track
	//	of the types at each position.
	// This could just bind the decl->typesym but who is keeping track anyways.
	// Also don't free this decl! The symbol table does not own it!!
	scope_bind(decl, decl->typesym->symbol);

	LLVMTypeRef st_tp = LLVMStructCreateNamed(CTXT(mod), buf);
	for (size_t i = 0 ; i < al->size ; ++i) {
		LLVMTypeRef cur_type = to_llvm_type(mod, ((ast_typed_symbol *)vect_get(al, i))->type);
		vect_append(members, cur_type);
	}
	LLVMStructSetBody(st_tp, (LLVMTypeRef *)members->elements, members->size, 0);
	vect_destroy(members); // WARNING! Potential use-after-free situation?
	// The type references themelves aren't freed
	// but the array where the pointers are stored is within members->elements
	// If LLVMStructSetBody creates a copy of the whole array / each pointer that's cool
	// if LLVMStructSetBody itself holds onto a pointer to member->elements that
	// could be a problem, as the data stored here will be overwritten etc.
}


void decl_codegen(LLVMModuleRef *mod, ast_decl *decl)
{
	if (decl == NULL)
		return;
	if (decl->typesym->type->kind == Y_FUNCTION) {
		scope_enter();
		scope_bind_return_type(decl->typesym->type->subtype);
		char buf[BUFFER_MAX_LEN];
		vect *v = vect_init(4);
		LLVMTypeRef *param_types = build_param_types(*mod, decl);
		vect *arglist = decl->typesym->type->arglist;
		size_t size = arglist == NULL ? 0 : arglist->size;
		LLVMTypeRef ret_type = LLVMFunctionType(to_llvm_type(*mod, decl->typesym->type->subtype), param_types, size, 0);

		strvec_tostatic(decl->typesym->symbol, buf);
		LLVMValueRef fn_value = LLVMAddFunction(*mod, buf, ret_type);
		LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(CTXT(*mod), fn_value, "");
		LLVMBuilderRef builder = LLVMCreateBuilderInContext(CTXT(*mod));
		LLVMPositionBuilderAtEnd(builder, entry);

		alloca_params_as_local_vars(builder, fn_value, decl, param_types);

		stmt_codegen(*mod, builder, decl->body, NULL);
		LLVMDisposeBuilder(builder);
		free(param_types);
		vect_destroy(v);
	} else if (decl->typesym->type->kind == Y_STRUCT && decl->typesym->type->name == NULL) {
		define_struct(*mod, decl);
	} else {
	// LCOV_EXCL_START
		fprintf(stderr, "Can't codegen decls of this type yet :(\n");
		exit(1);
	// LCOV_EXCL_STOP
	}
}
