#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vcode.h"

struct integer_pair { long first, second; };
struct float_pair { double first, second; };
struct mixed_pair { long integer; double floating; };
struct reverse_mixed_pair { double floating; long integer; };
struct large_value { long first, second, third; };
struct nested_value { struct { int first, second; } inner; double value; };
union integer_union { long integer; double floating; };
struct union_value { union integer_union value; int tag; };
struct __attribute__((packed)) packed_value { char tag; long value; };
struct __attribute__((aligned(16))) aligned_pair { long first, second; };
struct byte_triple { unsigned char first, second, third; };

#define FIELD(at, kind) { (at), (kind) }
static const struct v_aggregate integer_pair_layout = {
	16, 8, 2, { FIELD(0, V_L), FIELD(8, V_L) }
};
static const struct v_aggregate float_pair_layout = {
	16, 8, 2, { FIELD(0, V_D), FIELD(8, V_D) }
};
static const struct v_aggregate mixed_pair_layout = {
	16, 8, 2, { FIELD(0, V_L), FIELD(8, V_D) }
};
static const struct v_aggregate reverse_mixed_pair_layout = {
	16, 8, 2, { FIELD(0, V_D), FIELD(8, V_L) }
};
static const struct v_aggregate large_value_layout = {
	24, 8, 3, { FIELD(0, V_L), FIELD(8, V_L), FIELD(16, V_L) }
};
static const struct v_aggregate nested_value_layout = {
	16, 8, 3, { FIELD(0, V_I), FIELD(4, V_I), FIELD(8, V_D) }
};
static const struct v_aggregate union_value_layout = {
	16, 8, 3, { FIELD(0, V_L), FIELD(0, V_D), FIELD(8, V_I) }
};
static const struct v_aggregate packed_value_layout = {
	9, 1, 2, { FIELD(0, V_C), FIELD(1, V_L) }
};
static const struct v_aggregate aligned_pair_layout = {
	16, 16, 2, { FIELD(0, V_L), FIELD(8, V_L) }
};
static const struct v_aggregate byte_triple_layout = {
	3, 1, 3, { FIELD(0, V_UC), FIELD(1, V_UC), FIELD(2, V_UC) }
};

static struct integer_pair gcc_integer_pair(struct integer_pair value)
{ value.first += 10; value.second += 20; return value; }
static struct float_pair gcc_float_pair(struct float_pair value)
{ value.first += 1.25; value.second += 2.5; return value; }
static struct mixed_pair gcc_mixed_pair(struct mixed_pair value)
{ value.integer += 7; value.floating += 0.5; return value; }
static struct reverse_mixed_pair gcc_reverse_mixed_pair(
	struct reverse_mixed_pair value)
{ value.floating += 0.75; value.integer += 8; return value; }
static struct large_value gcc_large_value(struct large_value value)
{ value.first++; value.second += 2; value.third += 3; return value; }
static struct nested_value gcc_nested_value(struct nested_value value)
{ value.inner.first++; value.inner.second += 2; value.value += 3.0; return value; }
static struct union_value gcc_union_value(struct union_value value)
{ value.value.integer += 4; value.tag += 5; return value; }
static struct packed_value gcc_packed_value(struct packed_value value)
{ value.tag += 1; value.value += 9; return value; }
static struct byte_triple gcc_byte_triple(struct byte_triple value)
{ value.first++; value.second += 2; value.third += 3; return value; }

static long gcc_integer_rollback(long a, long b, long c, long d, long e,
	struct integer_pair pair, long tail)
{ return a + b + c + d + e + pair.first * 10 + pair.second * 100 + tail * 1000; }

static double gcc_sse_rollback(double a, double b, double c, double d,
	double e, double f, double g, struct float_pair pair, double tail)
{ return a + b + c + d + e + f + g + pair.first * 10 + pair.second * 100 + tail * 1000; }

static long gcc_aligned_stack(long a, long b, long c, long d, long e, long f,
	long stack_scalar, struct aligned_pair pair, long tail)
{
	if (((uintptr_t)&pair & 15) != 0)
		return -1;
	return a + b + c + d + e + f + stack_scalar +
	       pair.first * 10 + pair.second * 100 + tail * 1000;
}

static v_vptr as_vptr(const void *function_address)
{
	v_vptr result;
	memcpy(&result, &function_address, sizeof result);
	return result;
}

static union v_fp generate_identity(const struct v_aggregate *layout)
{
	v_code *code = v_code_alloc(4096);
	struct v_parameter parameter = { V_B, layout };
	v_reg_t argument;
	union v_fp result;
	if (!code) abort();
	v_lambda_aggregate("aggregate_identity", layout, &parameter, 1,
	                   &argument, V_LEAF, code, 4096);
	v_retb(argument, layout);
	result = v_end(NULL);
	return result;
}

static union v_fp generate_gcc_round_trip(const struct v_aggregate *layout,
	const void *value, const void *gcc_function)
{
	v_code *code = v_code_alloc(4096);
	struct v_cstate call;
	v_reg_t address, returned;
	union v_fp result;
	if (!code) abort();
	v_lambda_aggregate("gcc_aggregate_round_trip", layout, NULL, 0, NULL,
	                   V_NLEAF, code, 4096);
	if (!v_getreg(&address, V_P, V_VAR)) abort();
	v_setp(address, (uintptr_t)value);
	v_push_init(&call);
	v_arg_pushb(&call, address, layout);
	returned = v_ccallb(&call, as_vptr(gcc_function), layout);
	v_retb(returned, layout);
	result = v_end(NULL);
	return result;
}

static long generate_integer_rollback(void)
{
	static struct integer_pair pair = { 6, 7 };
	v_code *code = v_code_alloc(4096);
	struct v_cstate call;
	v_reg_t address, result;
	union v_fp function;
	if (!code) abort();
	v_lambda("integer_rollback", "", NULL, V_NLEAF, code, 4096);
	if (!v_getreg(&address, V_P, V_VAR)) abort();
	v_setp(address, (uintptr_t)&pair);
	v_push_init(&call);
	push_arg_imm(&call, V_L, 1); push_arg_imm(&call, V_L, 2);
	push_arg_imm(&call, V_L, 3); push_arg_imm(&call, V_L, 4);
	push_arg_imm(&call, V_L, 5);
	v_arg_pushb(&call, address, &integer_pair_layout);
	push_arg_imm(&call, V_L, 8);
	result = v_ccalll(&call, (v_lptr)gcc_integer_rollback);
	v_retl(result);
	function = v_end(NULL);
	return function.l();
}

static double generate_sse_rollback(void)
{
	static struct float_pair pair = { 8.0, 9.0 };
	v_code *code = v_code_alloc(4096);
	struct v_cstate call;
	v_reg_t address, value, result;
	union v_fp function;
	int i;
	if (!code) abort();
	v_lambda("sse_rollback", "", NULL, V_NLEAF, code, 4096);
	if (!v_getreg(&address, V_P, V_VAR) || !v_getreg(&value, V_D, V_VAR)) abort();
	v_setp(address, (uintptr_t)&pair);
	v_push_init(&call);
	for (i = 1; i <= 7; i++) { v_setd(value, (double)i); v_push_argd(&call, value); }
	v_arg_pushb(&call, address, &float_pair_layout);
	v_setd(value, 10.0); v_push_argd(&call, value);
	result = v_ccalld(&call, (v_dptr)gcc_sse_rollback);
	v_retd(result);
	function = v_end(NULL);
	return function.d();
}

static long generate_aligned_stack(void)
{
	static struct aligned_pair pair = { 8, 9 };
	v_code *code = v_code_alloc(4096);
	struct v_cstate call;
	v_reg_t address, result;
	union v_fp function;
	int i;
	if (!code) abort();
	v_lambda("aligned_stack", "", NULL, V_NLEAF, code, 4096);
	if (!v_getreg(&address, V_P, V_VAR)) abort();
	v_setp(address, (uintptr_t)&pair);
	v_push_init(&call);
	for (i = 1; i <= 7; i++) push_arg_imm(&call, V_L, (uintptr_t)i);
	v_arg_pushb(&call, address, &aligned_pair_layout);
	push_arg_imm(&call, V_L, 10);
	result = v_ccalll(&call, (v_lptr)gcc_aligned_stack);
	v_retl(result);
	function = v_end(NULL);
	return function.l();
}

#define CHECK(condition) do { if (!(condition)) { \
	fprintf(stderr, "check failed at line %d\n", __LINE__); return 1; \
} } while (0)

int main(void)
{
	union v_fp generated;
	enum v_abi_class classes[2];
	struct integer_pair (*integer_identity)(struct integer_pair);
	struct float_pair (*float_identity)(struct float_pair);
	struct mixed_pair (*mixed_identity)(struct mixed_pair);
	struct reverse_mixed_pair (*reverse_mixed_identity)(
		struct reverse_mixed_pair);
	struct large_value (*large_identity)(struct large_value);
	struct packed_value (*packed_identity)(struct packed_value);
	struct byte_triple (*byte_identity)(struct byte_triple);
	struct nested_value (*nested_round_trip)(void);
	struct union_value (*union_round_trip)(void);
	struct integer_pair (*integer_round_trip)(void);
	struct float_pair (*float_round_trip)(void);
	struct mixed_pair (*mixed_round_trip)(void);
	struct reverse_mixed_pair (*reverse_mixed_round_trip)(void);
	struct large_value (*large_round_trip)(void);
	struct packed_value (*packed_round_trip)(void);
	struct byte_triple (*byte_round_trip)(void);
	struct integer_pair i = { 1, 2 }, ir;
	struct float_pair f = { 1.0, 2.0 }, fr;
	struct mixed_pair m = { 3, 4.0 }, mr;
	struct reverse_mixed_pair rm = { 4.25, 5 }, rmr;
	struct large_value l = { 5, 6, 7 }, lr;
	struct packed_value p = { 8, 9 }, pr;
	struct byte_triple bytes = { 15, 16, 17 }, bytes_result;
	static const struct nested_value n = { { 10, 11 }, 12.0 };
	static const struct union_value u = { { 13 }, 14 };
	struct nested_value nr;
	struct union_value ur;

	CHECK(v_aggregate_classify(&integer_pair_layout, classes) == 2 &&
	      classes[0] == V_ABI_INTEGER && classes[1] == V_ABI_INTEGER);
	CHECK(v_aggregate_classify(&float_pair_layout, classes) == 2 &&
	      classes[0] == V_ABI_SSE && classes[1] == V_ABI_SSE);
	CHECK(v_aggregate_classify(&mixed_pair_layout, classes) == 2 &&
	      classes[0] == V_ABI_INTEGER && classes[1] == V_ABI_SSE);
	CHECK(v_aggregate_classify(&union_value_layout, classes) == 2 &&
	      classes[0] == V_ABI_INTEGER && classes[1] == V_ABI_INTEGER);
	CHECK(v_aggregate_classify(&large_value_layout, classes) == 1 &&
	      classes[0] == V_ABI_MEMORY);
	CHECK(v_aggregate_classify(&packed_value_layout, classes) == 1 &&
	      classes[0] == V_ABI_MEMORY);

	generated = generate_identity(&integer_pair_layout);
	memcpy(&integer_identity, &generated.v, sizeof integer_identity);
	ir = integer_identity(i); CHECK(ir.first == 1 && ir.second == 2);
	generated = generate_identity(&float_pair_layout);
	memcpy(&float_identity, &generated.v, sizeof float_identity);
	fr = float_identity(f); CHECK(fr.first == 1.0 && fr.second == 2.0);
	generated = generate_identity(&mixed_pair_layout);
	memcpy(&mixed_identity, &generated.v, sizeof mixed_identity);
	mr = mixed_identity(m); CHECK(mr.integer == 3 && mr.floating == 4.0);
	generated = generate_identity(&reverse_mixed_pair_layout);
	memcpy(&reverse_mixed_identity, &generated.v, sizeof reverse_mixed_identity);
	rmr = reverse_mixed_identity(rm);
	CHECK(rmr.floating == 4.25 && rmr.integer == 5);
	generated = generate_identity(&large_value_layout);
	memcpy(&large_identity, &generated.v, sizeof large_identity);
	lr = large_identity(l); CHECK(lr.first == 5 && lr.second == 6 && lr.third == 7);
	generated = generate_identity(&packed_value_layout);
	memcpy(&packed_identity, &generated.v, sizeof packed_identity);
	pr = packed_identity(p); CHECK(pr.tag == 8 && pr.value == 9);
	generated = generate_identity(&byte_triple_layout);
	memcpy(&byte_identity, &generated.v, sizeof byte_identity);
	bytes_result = byte_identity(bytes);
	CHECK(bytes_result.first == 15 && bytes_result.second == 16 &&
	      bytes_result.third == 17);

	generated = generate_gcc_round_trip(&integer_pair_layout, &i, gcc_integer_pair);
	memcpy(&integer_round_trip, &generated.v, sizeof integer_round_trip);
	ir = integer_round_trip(); CHECK(ir.first == 11 && ir.second == 22);
	generated = generate_gcc_round_trip(&float_pair_layout, &f, gcc_float_pair);
	memcpy(&float_round_trip, &generated.v, sizeof float_round_trip);
	fr = float_round_trip(); CHECK(fr.first == 2.25 && fr.second == 4.5);
	generated = generate_gcc_round_trip(&mixed_pair_layout, &m, gcc_mixed_pair);
	memcpy(&mixed_round_trip, &generated.v, sizeof mixed_round_trip);
	mr = mixed_round_trip(); CHECK(mr.integer == 10 && mr.floating == 4.5);
	generated = generate_gcc_round_trip(&reverse_mixed_pair_layout, &rm,
	                                    gcc_reverse_mixed_pair);
	memcpy(&reverse_mixed_round_trip, &generated.v,
	       sizeof reverse_mixed_round_trip);
	rmr = reverse_mixed_round_trip();
	CHECK(rmr.floating == 5.0 && rmr.integer == 13);
	generated = generate_gcc_round_trip(&large_value_layout, &l, gcc_large_value);
	memcpy(&large_round_trip, &generated.v, sizeof large_round_trip);
	lr = large_round_trip(); CHECK(lr.first == 6 && lr.second == 8 && lr.third == 10);
	generated = generate_gcc_round_trip(&packed_value_layout, &p, gcc_packed_value);
	memcpy(&packed_round_trip, &generated.v, sizeof packed_round_trip);
	pr = packed_round_trip(); CHECK(pr.tag == 9 && pr.value == 18);
	generated = generate_gcc_round_trip(&byte_triple_layout, &bytes,
	                                    gcc_byte_triple);
	memcpy(&byte_round_trip, &generated.v, sizeof byte_round_trip);
	bytes_result = byte_round_trip();
	CHECK(bytes_result.first == 16 && bytes_result.second == 18 &&
	      bytes_result.third == 20);

	generated = generate_gcc_round_trip(&nested_value_layout, &n, gcc_nested_value);
	memcpy(&nested_round_trip, &generated.v, sizeof nested_round_trip);
	nr = nested_round_trip();
	CHECK(nr.inner.first == 11 && nr.inner.second == 13 && nr.value == 15.0);
	generated = generate_gcc_round_trip(&union_value_layout, &u, gcc_union_value);
	memcpy(&union_round_trip, &generated.v, sizeof union_round_trip);
	ur = union_round_trip(); CHECK(ur.value.integer == 17 && ur.tag == 19);

	CHECK(generate_integer_rollback() == 8775);
	CHECK(generate_sse_rollback() == 11008.0);
	CHECK(generate_aligned_stack() == 11008);
	puts("phase6d aggregate ABI: ok");
	return 0;
}
