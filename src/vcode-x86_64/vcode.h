#ifndef TICKC_X86_64_VCODE_H
#define TICKC_X86_64_VCODE_H

#ifndef __TICKC_ENV__
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#else
#ifndef TICKC_TARGET_INTEGER_TYPES
typedef unsigned long size_t;
typedef unsigned long uintptr_t;
typedef long intptr_t;
typedef unsigned int uint32_t;
typedef int int32_t;
typedef unsigned long uint64_t;
typedef long int64_t;
#define TICKC_TARGET_INTEGER_TYPES 1
#else
typedef unsigned long size_t;
typedef long intptr_t;
typedef unsigned int uint32_t;
typedef int int32_t;
typedef unsigned long uint64_t;
typedef long int64_t;
#endif
#endif

typedef unsigned char v_code;
extern uintptr_t v_ip;

typedef void (*v_vptr)();
typedef char (*v_cptr)();
typedef unsigned char (*v_ucptr)();
typedef short (*v_sptr)();
typedef unsigned short (*v_usptr)();
typedef int (*v_iptr)();
typedef unsigned int (*v_uptr)();
typedef long (*v_lptr)();
typedef unsigned long (*v_ulptr)();
typedef void *(*v_pptr)();
typedef float (*v_fptr)();
typedef double (*v_dptr)();

union v_fp {
	v_vptr v;
	v_ucptr uc;
	v_cptr c;
	v_usptr us;
	v_sptr s;
	v_iptr i;
	v_uptr u;
	v_lptr l;
	v_ulptr ul;
	v_pptr p;
	v_fptr f;
	v_dptr d;
};

typedef int v_reg_type;
typedef v_reg_type v_reg_t;
typedef int v_label_t;

enum {
	V_C, V_UC, V_S, V_US, V_I, V_U, V_L, V_UL, V_P,
	V_F, V_D, V_V, V_B, V_ERR,
	V_REGISTER = 1 << 5,
	V_IMMEDIATE = 1 << 6,
	V_MEMORY = 1 << 7
};

enum { V_FREE = 0, V_TEMP = 20, V_VAR = 21 };
enum { V_NLEAF = 0, V_LEAF = 1 };

#define MAX_ARGS 32
#define V_MAX_AGGREGATE_FIELDS 16

enum v_abi_class {
	V_ABI_NO_CLASS,
	V_ABI_INTEGER,
	V_ABI_SSE,
	V_ABI_MEMORY
};

struct v_aggregate_field {
	unsigned int offset;
	unsigned int type;
};

struct v_aggregate {
	unsigned int size;
	unsigned int alignment;
	unsigned int field_count;
	struct v_aggregate_field fields[V_MAX_AGGREGATE_FIELDS];
};

struct v_parameter {
	int type;
	const struct v_aggregate *aggregate;
};

struct v_cstate {
	int next_args;
	enum v_cstate_kind {
		CSTATE_IMMEDIATE,
		CSTATE_REGISTER,
		CSTATE_AGGREGATE
	} type[MAX_ARGS];
	int value_type[MAX_ARGS];
	uintptr_t val[MAX_ARGS];
	struct v_aggregate aggregates[MAX_ARGS];
};

void v_fatal(char *fmt, ...);
void v_emit_byte(unsigned int value);
void v_emit_u32(uint32_t value);
void v_emit_u64(uint64_t value);
void v_skip_bytes(size_t count);

void *v_code_alloc(size_t size);
int v_code_make_writable(void *address);
int v_code_finalize(void *address);
int v_code_free(void *address);

int v_getreg(v_reg_t *r, int type, int class_type);
int v_putreg(v_reg_t r, int type);
void v_pickreg(v_reg_type r, int type);
int v_local(int type);
int v_localb(unsigned int size);

v_label_t v_genlabel(void);
void v_label(v_label_t target);
void v_dlabel(void *address, v_label_t target);
void v_dmark(v_code *address, v_label_t target);
v_code *v_getlabel(v_label_t target);

void v_push_init(struct v_cstate *state);
void v_arg_push(struct v_cstate *state, int type, v_reg_type reg);
void v_arg_pushb(struct v_cstate *state, v_reg_type address,
                 const struct v_aggregate *aggregate);
void push_arg_reg(struct v_cstate *state, int type, v_reg_t reg);
void push_arg_imm(struct v_cstate *state, int type, uintptr_t imm);
void v_push_argf(struct v_cstate *state, v_reg_type reg);
void v_push_argd(struct v_cstate *state, v_reg_type reg);

#ifndef __TICKC_ENV__
v_reg_t scall(v_vptr ptr, char *fmt, va_list ap);
#endif
v_reg_t ccall(struct v_cstate *state, v_vptr ptr);
void v_scallv(v_vptr ptr, char *fmt, ...);
v_reg_t v_scalli(v_iptr ptr, char *fmt, ...);
v_reg_t v_scallu(v_uptr ptr, char *fmt, ...);
v_reg_t v_scallp(v_pptr ptr, char *fmt, ...);
v_reg_t v_scalll(v_lptr ptr, char *fmt, ...);
v_reg_t v_scallul(v_ulptr ptr, char *fmt, ...);
v_reg_t v_scallf(v_fptr ptr, char *fmt, ...);
v_reg_t v_scalld(v_dptr ptr, char *fmt, ...);
void v_ccallv(struct v_cstate *state, v_vptr ptr);
v_reg_t v_ccalli(struct v_cstate *state, v_iptr ptr);
v_reg_t v_ccallu(struct v_cstate *state, v_uptr ptr);
v_reg_t v_ccallp(struct v_cstate *state, v_pptr ptr);
v_reg_t v_ccalll(struct v_cstate *state, v_lptr ptr);
v_reg_t v_ccallul(struct v_cstate *state, v_ulptr ptr);
v_reg_t v_ccallf(struct v_cstate *state, v_fptr ptr);
v_reg_t v_ccalld(struct v_cstate *state, v_dptr ptr);
v_reg_t v_ccallb(struct v_cstate *state, v_vptr ptr,
                 const struct v_aggregate *result);
void v_rccallv(struct v_cstate *state, v_reg_type reg);
v_reg_t v_rccalli(struct v_cstate *state, v_reg_type reg);
v_reg_t v_rccallu(struct v_cstate *state, v_reg_type reg);
v_reg_t v_rccallp(struct v_cstate *state, v_reg_type reg);
v_reg_t v_rccalll(struct v_cstate *state, v_reg_type reg);
v_reg_t v_rccallul(struct v_cstate *state, v_reg_type reg);
v_reg_t v_rccallf(struct v_cstate *state, v_reg_type reg);
v_reg_t v_rccalld(struct v_cstate *state, v_reg_type reg);
v_reg_t v_rccallb(struct v_cstate *state, v_reg_type reg,
                  const struct v_aggregate *result);

void v_lambda(char *name, char *fmt, v_reg_t *args, int leaf,
              v_code *code, int nbytes);
void v_lambda_variadic(char *name, char *fmt, v_reg_t *args, int leaf,
                       v_code *code, int nbytes);
void v_lambda_aggregate(char *name, const struct v_aggregate *result,
                        const struct v_parameter *parameters, int count,
                        v_reg_t *args, int leaf, v_code *code, int nbytes);
void v_clambda(char *name, int leaf, v_code *code, int nbytes);
void v_param_alloc(int argno, int type, v_reg_type *reg);
void v_reset_closure_state(void);
void v_bpo_on(void);
void v_bpo_off(void);
union v_fp v_end(int *nbytes);
v_code *v_swapcp(v_code *address);
void v_begin_incremental(v_code *code, int nbytes);
int v_end_incremental(void);
int v_getra(v_reg_t *reg);
void v_dump(void *start);

void v_emit_binary(int operation, int type, v_reg_t rd, v_reg_t rs1,
                   v_reg_t rs2);
void v_emit_binary_imm(int operation, int type, v_reg_t rd, v_reg_t rs,
                       intptr_t imm);
void v_emit_unary(int operation, int type, v_reg_t rd, v_reg_t rs);
void v_emit_set(int type, v_reg_t rd, uintptr_t imm);
void v_emit_set_float(int type, v_reg_t rd, double value);
void v_emit_move(int type, v_reg_t rd, v_reg_t rs);
void v_emit_convert(int destination_type, int source_type,
                    v_reg_t rd, v_reg_t rs);
void v_emit_load(int type, v_reg_t rd, v_reg_t base, intptr_t offset,
                 int offset_is_register);
void v_emit_store(int type, v_reg_t value, v_reg_t base, intptr_t offset,
                  int offset_is_register);
void v_emit_branch(int condition, int type, v_reg_t lhs, intptr_t rhs,
                   int rhs_is_register, v_label_t target);
void v_emit_return(int type, v_reg_t reg, uintptr_t imm, int immediate);
void v_copyb(v_reg_t destination, v_reg_t source, unsigned int size);
void v_retb(v_reg_t address, const struct v_aggregate *aggregate);
int v_aggregate_classify(const struct v_aggregate *aggregate,
                         enum v_abi_class classes[2]);
void v_emit_jump_reg(v_reg_t reg);
void v_emit_jump_imm(uintptr_t address);
void v_emit_call_reg(v_reg_t reg);
void v_emit_call_imm(uintptr_t address);
void v_x64_emit_lea_rip(v_reg_t destination, int32_t displacement);
void v_emit_nop(void);
void v_va_start(v_reg_t list);
v_reg_t v_va_arg(v_reg_t list, int type);
void v_va_end(v_reg_t list);

void v_cmuli(v_reg_type rd, v_reg_type rs, int value);
void v_cmulu(v_reg_type rd, v_reg_type rs, unsigned int value);

#ifndef __TICKC_ENV__
#include <x86_64-codegen.h>
#endif

#endif
