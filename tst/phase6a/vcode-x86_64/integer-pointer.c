#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>

#include "vcode.h"

static long reference_sum(const long *values, int count)
{
	long result = 0;
	int i;

	for (i = 0; i < count; i++) result += values[i];
	return result;
}

static long external_mix(long value, long bias)
{
	return value * 3 + bias;
}

static int test_argument_boundaries(void)
{
	static const char *formats[] = {
		"", "%l", "%l%l", "%l%l%l", "%l%l%l%l",
		"%l%l%l%l%l", "%l%l%l%l%l%l",
		"%l%l%l%l%l%l%l", "%l%l%l%l%l%l%l%l"
	};
	long values[] = { 1, -2, 3, -4, 5, -6, 7, (1L << 40) + 8 };
	int count;

	for (count = 0; count <= 8; count++) {
		v_code *code = v_code_alloc(4096);
		v_reg_t args[8];
		union v_fp function;
		long actual;

		if (!code) return 1;
		v_lambda("argument_boundaries", (char *)formats[count], args,
		         V_LEAF, code, 4096);
		if (count == 0) {
			v_retli(0);
		} else {
			int i;
			for (i = 1; i < count; i++) v_addl(args[0], args[0], args[i]);
			v_retl(args[0]);
		}
		function = v_end(0);
		switch (count) {
		case 0: actual = function.l(); break;
		case 1: actual = function.l(values[0]); break;
		case 2: actual = function.l(values[0], values[1]); break;
		case 3: actual = function.l(values[0], values[1], values[2]); break;
		case 4: actual = function.l(values[0], values[1], values[2], values[3]); break;
		case 5: actual = function.l(values[0], values[1], values[2], values[3],
		                            values[4]); break;
		case 6: actual = function.l(values[0], values[1], values[2], values[3],
		                            values[4], values[5]); break;
		case 7: actual = function.l(values[0], values[1], values[2], values[3],
		                            values[4], values[5], values[6]); break;
		default: actual = function.l(values[0], values[1], values[2], values[3],
		                             values[4], values[5], values[6], values[7]);
		}
		if (actual != reference_sum(values, count)) {
			fprintf(stderr, "%d-argument result: %ld\n", count, actual);
			return 1;
		}
		if (v_code_free(code) < 0) return 1;
	}
	return 0;
}

static int test_narrow_conversions(void)
{
	v_code *code = v_code_alloc(4096);
	v_reg_t argument;
	v_reg_t narrow;
	v_reg_t result;
	union v_fp function;
	long (*generated)(long);
	long values[] = { LONG_MIN, -32769, -129, -128, -1, 0, 127, 128,
	                  255, 256, 32767, 32768, LONG_MAX };
	size_t i;

	if (!code) return 1;
	v_lambda("signed_char_conversion", "%l", &argument, V_LEAF, code, 4096);
	if (!v_getreg(&narrow, V_C, V_TEMP) ||
	    !v_getreg(&result, V_L, V_TEMP)) return 1;
	v_emit_convert(V_C, V_L, narrow, argument);
	v_emit_convert(V_L, V_C, result, narrow);
	v_retl(result);
	function = v_end(0);
	generated = (long (*)(long))function.l;
	for (i = 0; i < sizeof values / sizeof values[0]; i++) {
		long expected = (long)(signed char)values[i];
		long actual = generated(values[i]);
		if (actual != expected) {
			fprintf(stderr, "signed char conversion of %ld: %ld, expected %ld\n",
			        values[i], actual, expected);
			return 1;
		}
	}
	return v_code_free(code) < 0;
}

static int test_load_extensions(void)
{
	struct values {
		signed char c;
		unsigned char uc;
		short s;
		unsigned short us;
	} values = { SCHAR_MIN, UCHAR_MAX, SHRT_MIN, USHRT_MAX };
	v_code *code = v_code_alloc(4096);
	v_reg_t pointer;
	v_reg_t value;
	v_reg_t total;
	union v_fp function;
	long (*generated)(struct values *);
	long expected = (long)values.c + values.uc + values.s + values.us;

	if (!code) return 1;
	v_lambda("load_extensions", "%p", &pointer, V_LEAF, code, 4096);
	if (!v_getreg(&value, V_L, V_TEMP) ||
	    !v_getreg(&total, V_L, V_TEMP)) return 1;
	v_ldci(value, pointer, offsetof(struct values, c));
	v_cvi2l(total, value);
	v_lduci(value, pointer, offsetof(struct values, uc));
	v_cvu2l(value, value);
	v_addl(total, total, value);
	v_ldsi(value, pointer, offsetof(struct values, s));
	v_cvi2l(value, value);
	v_addl(total, total, value);
	v_ldusi(value, pointer, offsetof(struct values, us));
	v_cvu2l(value, value);
	v_addl(total, total, value);
	v_retl(total);
	function = v_end(0);
	generated = (long (*)(struct values *))function.l;
	if (generated(&values) != expected) return 1;
	return v_code_free(code) < 0;
}

static int test_loop_and_branch(void)
{
	v_code *code = v_code_alloc(4096);
	v_reg_t argument;
	v_reg_t accumulator;
	v_label_t loop;
	v_label_t done;
	union v_fp function;
	long (*generated)(long);
	long n;

	if (!code) return 1;
	v_lambda("triangular", "%l", &argument, V_LEAF, code, 4096);
	if (!v_getreg(&accumulator, V_L, V_TEMP)) return 1;
	v_setl(accumulator, 0);
	loop = v_genlabel();
	done = v_genlabel();
	v_label(loop);
	v_bleli(argument, 0, done);
	v_addl(accumulator, accumulator, argument);
	v_subli(argument, argument, 1);
	v_jv(loop);
	v_label(done);
	v_retl(accumulator);
	function = v_end(0);
	generated = (long (*)(long))function.l;
	for (n = 0; n <= 100; n++) {
		if (generated(n) != n * (n + 1) / 2) return 1;
	}
	return v_code_free(code) < 0;
}

static int test_indirect_call(void)
{
	v_code *code = v_code_alloc(4096);
	v_reg_t args[3];
	v_reg_t result;
	struct v_cstate call;
	union v_fp function;
	long (*generated)(long, long, long (*)(long, long));

	if (!code) return 1;
	v_lambda("indirect_call", "%l%l%p", args, V_NLEAF, code, 4096);
	v_push_init(&call);
	v_push_argl(&call, args[0]);
	v_push_argl(&call, args[1]);
	result = v_rccalll(&call, args[2]);
	v_retl(result);
	function = v_end(0);
	generated = (long (*)(long, long, long (*)(long, long)))function.l;
	if (generated((1L << 40) + 3, -7, external_mix) !=
	    external_mix((1L << 40) + 3, -7)) return 1;
	return v_code_free(code) < 0;
}

static int test_recursive_indirect_call(void)
{
	typedef long (*recursive_function)(void *, long);
	v_code *code = v_code_alloc(4096);
	v_reg_t args[2];
	v_reg_t decremented;
	v_reg_t result;
	struct v_cstate call;
	v_label_t base_case;
	union v_fp function;
	recursive_function generated;

	if (!code) return 1;
	v_lambda("recursive_factorial", "%p%l", args, V_NLEAF, code, 4096);
	if (!v_getreg(&decremented, V_L, V_TEMP)) return 1;
	base_case = v_genlabel();
	v_bleli(args[1], 1, base_case);
	v_subli(decremented, args[1], 1);
	v_push_init(&call);
	v_push_argp(&call, args[0]);
	v_push_argl(&call, decremented);
	result = v_rccalll(&call, args[0]);
	v_mull(result, result, args[1]);
	v_retl(result);
	v_label(base_case);
	v_retli(1);
	function = v_end(0);
	generated = (recursive_function)function.l;
	if (generated((void *)generated, 10) != 3628800) return 1;
	return v_code_free(code) < 0;
}

static int test_allocator_failures(void)
{
	int local;

	if (v_code_alloc(0) != 0) return 1;
	if (v_code_finalize(&local) != 0) return 1;
	errno = 0;
	if (v_code_free(&local) != -1 || errno != EINVAL) return 1;
	return 0;
}

int main(void)
{
	if (test_argument_boundaries()) return 1;
	if (test_narrow_conversions()) return 1;
	if (test_load_extensions()) return 1;
	if (test_loop_and_branch()) return 1;
	if (test_indirect_call()) return 1;
	if (test_recursive_indirect_call()) return 1;
	if (test_allocator_failures()) return 1;
	puts("Phase 6A direct-vcode tests passed");
	return 0;
}
