#include "codegen.h"
#include "error.h"
#include "ht.h"
#include "print.h"
#include "symbol_table.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CTXT(mod) (LLVMGetModuleContext(mod))

extern struct stack *sym_tab;

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

static LLVMTypeRef int_kind_to_llvm_type(LLVMModuleRef mod, type_t kind) {
	LLVMContextRef ctxt = CTXT(mod);
	switch (kind) {
	case Y_I32:
		return LLVMInt32TypeInContext(ctxt);
	case Y_I64:
		return LLVMInt64TypeInContext(ctxt);
	default:
		fprintf(stderr, "couldn't convert type a\n");
		abort();
	}
}

static LLVMTypeRef to_llvm_type(LLVMModuleRef mod, ast_type *tp)
{
	char buffer[BUFFER_MAX_LEN];
	LLVMContextRef ctxt = CTXT(mod);
	switch (tp->kind) {
	case Y_I32:
		return LLVMInt32TypeInContext(ctxt);
	case Y_I64:
		return LLVMInt64TypeInContext(ctxt);
	case Y_BOOL:
		return LLVMInt1TypeInContext(ctxt);
	case Y_STRING:
		return LLVMPointerType(LLVMInt8TypeInContext(ctxt), 0);
	case Y_VOID:
		return LLVMVoidTypeInContext(ctxt);
	case Y_POINTER:
		return LLVMPointerType(to_llvm_type(mod, tp->subtype), 0);
	case Y_CHAR:
		return LLVMInt8TypeInContext(ctxt);
	case Y_STRUCT:
		strvec_tostatic(tp->name, buffer);
		return LLVMGetTypeByName(mod, buffer);
	default:
		fprintf(stderr, "couldn't convert type\n");
		abort();
	}
}

// If LLVMBuildRef2 is so good, why is there no LLVMBuildRef3??
static LLVMValueRef LLVMBuildLoad3(LLVMBuilderRef b, LLVMValueRef pointer_val, const char *name) {
	return LLVMBuildLoad2(b, LLVMGetElementType(LLVMTypeOf(pointer_val)), pointer_val, name);
}

LLVMModuleRef module_codegen(LLVMContextRef ctxt, ast_decl *start, char *module_name)
{
	LLVMModuleRef ret = LLVMModuleCreateWithNameInContext(module_name, ctxt);

	while (start) {
		decl_codegen(&ret, start);
		start = start->next;
	}
	return ret;
}

// This function is a little intimidating: but it is quite simple.
// First it allocates space for an array on the stack (with element type determined by decl subtype)
// then it populates each array space with the corresponding element from the initializer
// then it builds a cast of the array type to a pointer type so it can be treated like any other ptr.
// 	this casted pointer is what is used later.
static void initializer_codegen(LLVMModuleRef mod, LLVMBuilderRef builder, ast_stmt *stmt)
{
	char buffer[BUFFER_MAX_LEN];
	strvec_tostatic(stmt->decl->typesym->symbol, buffer);
	LLVMValueRef a[2];
	a[0] = LLVMConstInt(LLVMInt32TypeInContext(CTXT(mod)), 0, 0);

	LLVMTypeRef llvmified_inner = to_llvm_type(mod, stmt->decl->typesym->type->subtype);

	LLVMTypeRef array_type = LLVMArrayType(llvmified_inner, stmt->decl->initializer->size);
	LLVMValueRef init = LLVMBuildAlloca(builder, array_type, "");
	for (size_t i = 0 ; i < stmt->decl->initializer->size ; ++i) { // clang uses memcpy for this! would be much better!
		a[1] = LLVMConstInt(LLVMInt32TypeInContext(CTXT(mod)), i, 0);
		LLVMValueRef gep = LLVMBuildGEP(builder, init, a, 2, "");
		LLVMValueRef value = expr_codegen(mod, builder, stmt->decl->initializer->elements[i], 0);
		LLVMBuildStore(builder, value, gep);
	}
	LLVMValueRef alloca2 = LLVMBuildAlloca(builder, LLVMPointerType(llvmified_inner, 0), buffer);
	a[1] = LLVMConstInt(LLVMInt32Type(), 0, 0);
	LLVMBuildStore(builder, LLVMBuildPointerCast(builder, init, LLVMPointerType(llvmified_inner, 0), ""), alloca2);
	//vect_append(v, (void *)alloca2);
	scope_bind(alloca2, stmt->decl->typesym->symbol);
}

void stmt_codegen(LLVMModuleRef mod, LLVMBuilderRef builder, ast_stmt *stmt, int in_fn)
{
	LLVMValueRef v1;
	LLVMBasicBlockRef b1;
	LLVMBasicBlockRef b2;
	LLVMBasicBlockRef b3;
	LLVMValueRef cur_function;
	LLVMContextRef ctxt = CTXT(mod);
	ast_stmt *cur;
	char buffer[BUFFER_MAX_LEN];
	int need_retvoid = 1;

	if (stmt == NULL)
		return;

	switch (stmt->kind) {
	case S_BLOCK:
		scope_enter();
		cur = stmt->body;
		while (cur) {
			if (in_fn == 0 || cur->kind == S_RETURN)
				need_retvoid = 0;
			stmt_codegen(mod, builder, cur, 0);
			cur = cur->next;
		}
		if (need_retvoid)
			LLVMBuildRetVoid(builder);
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
		cur_function = LLVMGetBasicBlockParent(LLVMGetInsertBlock(builder));
		v1 = expr_codegen(mod, builder, stmt->expr, 0);
		b1 = LLVMAppendBasicBlockInContext(ctxt, cur_function, "");
		b2 = LLVMAppendBasicBlockInContext(ctxt, cur_function, "");
		b3 = LLVMAppendBasicBlockInContext(ctxt, cur_function, "");

		LLVMBuildCondBr(builder, v1, b1, b2);

		LLVMPositionBuilderAtEnd(builder, b1);
		stmt_codegen(mod, builder, stmt->body, 0);
		v1 = LLVMGetLastInstruction(b1);
		if (!LLVMIsATerminatorInst(v1))
			LLVMBuildBr(builder, b3);

		LLVMPositionBuilderAtEnd(builder, b2);
		stmt_codegen(mod, builder, stmt->else_body, 0);
		LLVMBuildBr(builder, b3);

		LLVMPositionBuilderAtEnd(builder, b3);
		break;
	case S_DECL:
		if (stmt->decl->initializer != NULL) {
			initializer_codegen(mod, builder, stmt);
		} else {
			strvec_tostatic(stmt->decl->typesym->symbol, buffer);
			v1 = LLVMBuildAlloca(builder, to_llvm_type(mod, stmt->decl->typesym->type), buffer);
			//vect_append(v, (void *)v1);
			scope_bind(v1, stmt->decl->typesym->symbol);
			if (stmt->decl->expr != NULL)
				LLVMBuildStore(builder, expr_codegen(mod, builder, stmt->decl->expr, 0), v1);
		}
		break;
	case S_WHILE:
		cur_function = LLVMGetBasicBlockParent(LLVMGetInsertBlock(builder));
		v1 = expr_codegen(mod, builder, stmt->expr, 0);
		b1 = LLVMAppendBasicBlockInContext(ctxt, cur_function, "");
		b2 = LLVMAppendBasicBlockInContext(ctxt, cur_function, "");
		LLVMBuildCondBr(builder, v1, b1, b2);

		LLVMPositionBuilderAtEnd(builder, b1);
		stmt_codegen(mod, builder, stmt->body, 0);
		v1 = LLVMGetLastInstruction(b1);
		if (!LLVMIsATerminatorInst(v1)) {
			v1 = expr_codegen(mod, builder, stmt->expr, 0);
			LLVMBuildCondBr(builder, v1, b1, b2);
		}

		LLVMPositionBuilderAtEnd(builder, b2);
		break;
	default:
		fprintf(stderr, "can't codegen that stmt kind right now. (%d)\n", stmt->kind);
		exit(1);
	}
}

static LLVMValueRef get_param_by_name(LLVMValueRef function, char *name)
{
	LLVMValueRef param = NULL;
	for (unsigned int i = 0 ; i < LLVMCountParams(function) ; ++i) {
		param = LLVMGetParam(function, i);
		if (LLVMGetValueName(param) != NULL && strcmp(LLVMGetValueName(param), name) == 0)
			return param;
	}
	return NULL;
}

// TODO undo MAX_ARGS? Enforce MAX_ARGS? IDK just decide on something!!
#define MAX_ARGS 32
static void get_fncall_args(LLVMModuleRef mod, LLVMBuilderRef builder,ast_expr *expr, unsigned argno, LLVMValueRef (*args)[MAX_ARGS])
{
	if (argno == 0)
		return;
	for (size_t i = 0 ; i < expr->sub_exprs->size ; ++i)
		(*args)[i] = expr_codegen(mod, builder, expr->sub_exprs->elements[i], 0);
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
	default:
		fprintf(stderr, "\nCan't codegen assignment type with operation token %d\n", expr->op);
		abort();
	}
	tempval = expr_codegen(mod, builder, temp, 0);
	ret = LLVMBuildStore(builder, tempval, loc);
	free(temp); // do NOT expr_destroy!!!
	return ret;
}

// this function is courtesy of
// https://stackoverflow.com/questions/65042902/create-and-reference-a-string-literal-via-llvm-c-interface
LLVMValueRef define_string_literal(LLVMModuleRef mod, LLVMBuilderRef builder, const char *source_string, size_t size) {
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
	eputs("DIDN'T FIND REQUESTED STRUCT MEMBER. FATAL ERROR.");
	exit(1);
}

static char *shorten_struct_string(char *string) {
	char *tmp;
	string += 1;
	tmp = string;
	while (*tmp != '*' && *tmp != ' ') {
		if (*tmp == '\0') {
			fprintf(stderr, "original at point of failure: %s\n", string);
			fprintf(stderr, "tmp right now: %s\n", tmp);
			eputs("BIG PROBLEM IN CODEGEN. NO SPACE IN STRUCT NAME!");
			exit(1);
		}
		tmp += 1;
	}
	*tmp = '\0';
	return string;
}

LLVMValueRef expr_codegen(LLVMModuleRef mod, LLVMBuilderRef builder, ast_expr *expr, int store_ctxt)
{
	char buffer[BUFFER_MAX_LEN];
	LLVMValueRef v;
	LLVMValueRef v2;
	LLVMValueRef args[MAX_ARGS];
	LLVMValueRef ret;
	LLVMValueRef syscall_args[4];
	LLVMTypeRef t;
	buffer[0] = '\0';
	unsigned argno;
	switch (expr->kind) {
	case E_INT_LIT:
		t = int_kind_to_llvm_type(mod, expr->int_size);
		return LLVMConstInt(t, (unsigned long long)expr->num, 0);
	case E_MULDIV:
		if (expr->op == T_STAR)
			return LLVMBuildMul(builder, expr_codegen(mod, builder, expr->left, 0), expr_codegen(mod, builder, expr->right, 0), "");
		else
			return LLVMBuildSDiv(builder, expr_codegen(mod, builder, expr->left, 0), expr_codegen(mod, builder, expr->right, 0), "");
	case E_ADDSUB:
		if (expr->op == T_MINUS)
			return LLVMBuildSub(builder, expr_codegen(mod, builder, expr->left, 0), expr_codegen(mod, builder, expr->right, 0), "");
		else
			return LLVMBuildAdd(builder, expr_codegen(mod, builder, expr->left, 0), expr_codegen(mod, builder, expr->right, 0), "");
	case E_EQUALITY:
		return LLVMBuildICmp(builder, LLVMIntEQ, expr_codegen(mod, builder, expr->left, 0), expr_codegen(mod, builder, expr->right, 0), "");
	case E_FNCALL:
		strvec_tostatic(expr->name, buffer);
		v = LLVMGetNamedFunction(mod, buffer);
		argno = LLVMCountParams(v);
		get_fncall_args(mod, builder, expr, argno, &args);
		return LLVMBuildCall(builder, v, args, argno, "");
	case E_SYSCALL:
		for (size_t i = 0 ; i < expr->sub_exprs->size ; ++i) {
			syscall_args[i] = expr_codegen(mod, builder, expr->sub_exprs->elements[i], 0);
		}
		return codegen_syscall3(builder, syscall_args);
	case E_IDENTIFIER:
		v = LLVMGetBasicBlockParent(LLVMGetInsertBlock(builder));
		ret = get_param_by_name(v, buffer);
		if (ret == NULL) {
			ret = scope_lookup(expr->name);
			ret = LLVMBuildLoad3(builder, ret, "");
		}
		return ret;
	case E_ASSIGN:
		if (expr->left->kind == E_IDENTIFIER) {
			v = scope_lookup(expr->left->name);
			if (v == NULL) {
				strvec_tostatic(expr->left->name, buffer);
				v = LLVMGetBasicBlockParent(LLVMGetInsertBlock(builder));
				v = get_param_by_name(v, buffer);
			}
		} else if (expr->left->kind == E_PRE_UNARY && expr->left->op == T_STAR) {
			v = expr_codegen(mod, builder, expr->left->left, 1);
		} else if (expr->left->kind == E_POST_UNARY && (expr->left->op == T_LBRACKET || expr->left->op == T_PERIOD)) {
			v = expr_codegen(mod, builder, expr->left, 1);
		} else {
			eputs("Can't assign to this expr type right now.");
			exit(1);
		}
		return assign_codegen(mod, builder, expr, v);
	case E_FALSE_LIT:
		return LLVMConstInt(LLVMInt1TypeInContext(CTXT(mod)), 0, 0);
	case E_TRUE_LIT:
		return LLVMConstInt(LLVMInt1TypeInContext(CTXT(mod)), 1, 0);
	case E_STR_LIT:
		return define_string_literal(mod, builder, expr->string_literal->text, expr->string_literal->size);
	case E_CHAR_LIT:
		return LLVMConstInt(LLVMInt8TypeInContext(CTXT(mod)), (int)expr->string_literal->text[0], 0);
	case E_INEQUALITY:
		if (expr->op == T_LT)
			return LLVMBuildICmp(builder, LLVMIntSLT, expr_codegen(mod, builder, expr->left, 0), expr_codegen(mod, builder, expr->right, 0), "");
		else if (expr->op == T_LTE)
			return LLVMBuildICmp(builder, LLVMIntSLE, expr_codegen(mod, builder, expr->left, 0), expr_codegen(mod, builder, expr->right, 0), "");
		else if (expr->op == T_GT)
			return LLVMBuildICmp(builder, LLVMIntSGT, expr_codegen(mod, builder, expr->left, 0), expr_codegen(mod, builder, expr->right, 0), "");
		else
			return LLVMBuildICmp(builder, LLVMIntSGE, expr_codegen(mod, builder, expr->left, 0), expr_codegen(mod, builder, expr->right, 0), "");
	case E_PAREN:
		return expr_codegen(mod, builder, expr->left, store_ctxt);
	case E_POST_UNARY:
		if (expr->op == T_LBRACKET) {
			v = expr_codegen(mod, builder, expr->left, 0);
			v2 = expr_codegen(mod, builder, expr->right, 0);
			v = LLVMBuildGEP(builder, v, &v2, 1, "");
			if (!store_ctxt)
				v = LLVMBuildLoad3(builder, v, "");
			return v;
		} else if (expr->op == T_PERIOD) {
			if (expr->left->kind == E_IDENTIFIER) {
				v = scope_lookup(expr->left->name);
			} else {
				v = expr_codegen(mod, builder, expr->left, 1);
			}

			// stupid name extracting
			LLVMTypeRef val_tp = LLVMTypeOf(v);
			char *struct_string = LLVMPrintTypeToString(val_tp);
			char *shortened = shorten_struct_string(struct_string);
			strvec *name_vec = strvec_init_str(shortened);

			// Look up the decl in sym tab so we can get field names
			ast_decl *struct_decl;
			struct_decl = scope_lookup(name_vec);

			free(struct_string);
			strvec_destroy(name_vec);

			// set up gep indices
			size_t ind = get_member_position(struct_decl, expr->right->name);
			v2 = LLVMBuildStructGEP(builder, v, ind, "");
			if (!store_ctxt) {
				return LLVMBuildLoad3(builder, v2, "");
			}
			return v2;
		} else {
			eputs("can't codegen this post unary expr type yet");
			exit(1);
		}
	case E_CAST:
		// TODO: ZExt for unsigneds
		// TODO: cast down (truncate)
		return LLVMBuildSExt(builder, expr_codegen(mod,
				builder, expr->left, store_ctxt),
				int_kind_to_llvm_type(mod, expr->cast_to), "");
	case E_PRE_UNARY:
		if (expr->op == T_STAR) {
			v = expr_codegen(mod, builder, expr->left, 0);
			return LLVMBuildLoad3(builder, v, "");
		} else if (expr->op == T_AMPERSAND) {
			return scope_lookup(expr->left->name);
		}
		//fallthrough
	default:
		fprintf(stderr, "can't codegen that expr kind right now.\n");
		exit(1);
	}
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
		//vect_append(vc, (void *)v);
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
	// 	them isn't reaaaally needed, but it's easier than keeping a
	//	second list just for struct defs.
	// Structs typesyms need to be accessible easily so that you can select struct fields
	//	by name, as LLVM doesn't keep track of the field names. It only keeps track
	//	of the types at each position.
	// This could just bind the decl->typesym but who is keeping track anyways.
	// Also don't free this decl! The symbol table does not own it!!
	scope_bind(decl, decl->typesym->symbol);

	for (size_t i = 0 ; i < al->size ; ++i) {
		LLVMTypeRef cur_type = to_llvm_type(mod, ((ast_typed_symbol *)vect_get(al, i))->type);
		vect_append(members, cur_type);
	}
	LLVMTypeRef st_tp = LLVMStructCreateNamed(CTXT(mod), buf);
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

		stmt_codegen(*mod, builder, decl->body, 1);
		LLVMDisposeBuilder(builder);
		free(param_types);
		vect_destroy(v);
	} else if (decl->typesym->type->kind == Y_STRUCT && decl->typesym->type->name == NULL) {
		define_struct(*mod, decl);
	} else {
		fprintf(stderr, "Can't codegen decls of this type yet :(\n");
		exit(1);
	}
}
