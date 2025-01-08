#ifndef AST_H
#define AST_H

#include <stdbool.h>

#include "token.h"
#include "util.h"

#define arglist_get(list, i) ((ast_typed_symbol *)vect_get(list, i))

typedef struct ast_decl {
	struct ast_typed_symbol *typesym;
	struct ast_expr *expr;
	struct ast_stmt *body;
	struct ast_decl *next;
	struct vect *initializer;
	size_t line;
} ast_decl;

// Quick note on Y_STRUCT type and declarations:
//
// a decl declaring a new type of stuct will look like the following:
//
// let example: struct = {...};
// ast_decl -> typesym -> type -> kind = Y_STRUCT
//                     -> symbol = example
//
// let instance: struct example;
// ast_decl -> typesym -> type -> kind = Y_STRUCT
//                             -> name = "example"
//                     -> symbol = "instance"

#define TYPE_SIGNEDNESS_MASK 0xF000
#define TYPE_WIDTH_MASK 0x0F00

#define UNSIGNED(typ) ((typ & TYPE_SIGNEDNESS_MASK) == 0x1000)
#define SIGNED(typ) ((typ & TYPE_SIGNEDNESS_MASK) == 0x2000)
#define TYPE_WIDTH(typ) (typ & TYPE_WIDTH_MASK)
#define TYPE_SIGNEDNESS(typ) (typ & TYPE_SIGNEDNESS_MASK)

// Explicitly assigning Y_CHAR..Y_STRUCT values to make it 100% clear that they don't collide with
// the integer size/signedness shenanigans
typedef enum {
	Y_BOOL = 0x0000,
	Y_VOID = 0x0001,
	Y_POINTER = 0x0002,
	Y_CONSTPTR = 0x0003, // the values at the memory address being pointed to cannot be changed using this ptr
	Y_FUNCTION = 0x0004,
	Y_STRUCT = 0x0005,
	// kinda dumb but useful: integer types encode bit witdth in the 4 binary digits at 0x0F00,
	// and the 0xF000 bits encode signedness: 1 is unsigned, 2 is signed.
	// TYPE_SIGNEDNESS_MASK and TYPE_WIDTH_MASK help keep track of this!
	// Should things change, don't forget to change those macros!
	Y_CHAR = 0x1600,
	Y_U32 = 0x1800,
	Y_U64 = 0x1900,
	Y_I32 = 0x2800,
	Y_I64 = 0x2900,
} type_t;

// TODO: dynamically set usize according to provided target.
#define Y_USIZE Y_U64

typedef enum {
	VM_DEFAULT,
	VM_CONST,
	VM_PROTO,
	VM_PROTO_DEFINED, // is a prototype and has a definition in the current module
} value_modifier_t;

typedef struct ast_type {
	struct ast_type *subtype;
	bool owns_subtype;
	vect *arglist;
	type_t kind;
	strvec *name;
	value_modifier_t modif;
} ast_type;

typedef struct ast_typed_symbol {
	struct ast_type *type;
	strvec *symbol;
} ast_typed_symbol;

typedef enum {
	E_ADDSUB,
	E_MULDIV,
	E_EQUALITY,
	E_INEQUALITY,
	E_LOG_OR,
	E_LOG_AND,
	E_BW_XOR,
	E_BW_OR,
	E_BW_AND,
	E_ASSIGN,
	E_PAREN,
	E_INT_LIT,
	E_STR_LIT,
	E_CHAR_LIT,
	E_FALSE_LIT,
	E_TRUE_LIT,
	E_IDENTIFIER,
	E_PRE_UNARY,
	E_POST_UNARY,
	E_FNCALL,
	E_CAST,
	E_SHIFT,
	E_NULL,
} expr_t;

type_t smallest_fit(int64_t num);

typedef struct ast_expr {
	expr_t kind;
	struct ast_expr *left;
	struct ast_expr *right;
	token_t op;
	strvec *name;
	int64_t num;
	strvec *string_literal;
	vect *sub_exprs;
	uint8_t is_lvalue;

	// Expressions may either own their types or not own their types. Exprs must free types
	// if and only if they own their types. Exprs only own types if the type of the expr
	// was specifically made for that expr. An expr's type ptr can point to a type in the
	// symbol table, another expr's type, or to a novel type created just for the expr.
	// We don't care about the specifics, we just have to know if it is the current expr's
	// responsibility to free that type.
	//
	// Sharing types can save tons of time and memory, it is just a little annoying to keep
	// track of which exprs are in charge of freeing which types.
	bool owns_type;
	ast_type *type;
} ast_expr;

#define IS_UNSIGNED(e) UNSIGNED(e->type->kind)

void expr_add_sub_expr(ast_expr *e, ast_expr *sub);
vect *sub_exprs_init(size_t size);
void destroy_expr_vect(vect *expr_vect);

typedef enum { S_ERROR, S_BLOCK, S_DECL, S_EXPR, S_IFELSE, S_RETURN, S_WHILE, S_BREAK, S_CONTINUE, S_ASM} stmt_t;

typedef struct asm_struct {
	ast_expr *code;
	ast_expr *constraints;
	vect *out_operands;
	vect *in_operands;
} asm_struct;

typedef enum {RETW_UNCHECKED, RETW_FALSE, RETW_TRUE} retw_t;

typedef struct ast_stmt {
	stmt_t kind;
	struct ast_decl *decl;
	struct ast_expr *expr;
	struct ast_stmt *body;
	struct ast_stmt *else_body;
	struct ast_stmt *next;
	struct asm_struct *asm_obj;
	void *extra; // THIS SUCKS: break/continues need to know where to go next, this is where I stuff the LLVMBasicBlockRef
	size_t line;
	retw_t return_worthy;
} ast_stmt;

ast_decl *decl_init(ast_typed_symbol *typesym, ast_expr *expr, ast_stmt *stmt, ast_decl *next, size_t line);
ast_type *type_init(type_t kind, strvec *name);
ast_typed_symbol *ast_typed_symbol_init(ast_type *type, strvec *symbol);
ast_expr *expr_init(expr_t kind, ast_expr *left, ast_expr *right, token_t op, strvec *name,
			int64_t num, strvec *str_lit);
ast_stmt *stmt_init(stmt_t kind, ast_decl *decl, ast_expr *expr, ast_stmt *body,
			ast_stmt *else_body, size_t line);
void ast_free(ast_decl *program);
void type_destroy(ast_type *type);
void expr_destroy(ast_expr *expr);
void decl_destroy(ast_decl *decl);
void stmt_destroy(ast_stmt *stmt);
void ast_typed_symbol_destroy(ast_typed_symbol *typesym);
ast_type *type_copy(ast_type *t);
vect *arglist_copy(vect *arglist);

void arglist_destroy(vect *arglist);
ast_stmt *last(ast_stmt *block);
bool is_integer(ast_type *t);
#endif
