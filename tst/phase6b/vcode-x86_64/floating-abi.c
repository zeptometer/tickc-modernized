#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "vcode.h"

typedef double (*mixed_function)(long, double, float, double, float, double,
				 float, double, float, double, float, long);

static double reference_mixed(long first, double a, float b, double c, float d,
			      double e, float f, double g, float h, double i,
			      float j, long last)
{
	return first + a + b + c + d + e + f + g + h + i + j + last;
}

static int close_enough(double actual, double expected)
{
	return fabs(actual - expected) < 1e-9;
}

static int test_exact_encoder(void)
{
	static const unsigned char expected[] = {
		0xf2, 0x0f, 0x10, 0xd3,
		0xf3, 0x0f, 0x58, 0xe5,
		0xf3, 0x0f, 0x10, 0x70, 0x08,
		0xf2, 0x0f, 0x11, 0x7f, 0x10,
		0xf2, 0x0f, 0x2a, 0xd7,
		0xf2, 0x0f, 0x2c, 0xc2
	};
	v_code *code = v_code_alloc(4096);
	int count;

	if (!code) return 1;
	v_begin_incremental(code, 4096);
	v_movd(__XMM2, __XMM3);
	v_addf(__XMM4, __XMM4, __XMM5);
	v_ldfi(__XMM6, __RAX, 8);
	v_stdi(__XMM7, __RDI, 16);
	v_cvi2d(__XMM2, __RDI);
	v_cvd2i(__RAX, __XMM2);
	count = v_end_incremental();
	if (count != (int)sizeof expected || memcmp(code, expected, sizeof expected)) {
		fprintf(stderr, "scalar SSE encoder mismatch (%d bytes)\n", count);
		return 1;
	}
	return v_code_free(code) < 0;
}

static mixed_function build_mixed(int call_reference)
{
	v_code *code = v_code_alloc(4096);
	v_reg_t args[12];
	v_reg_t total;
	v_reg_t converted;
	union v_fp function;
	int i;

	if (!code) return 0;
	v_lambda(call_reference ? "call_reference" : "mixed_arithmetic",
	         "%l%d%f%d%f%d%f%d%f%d%f%l", args, V_NLEAF, code, 4096);
	if (call_reference) {
		struct v_cstate call;
		v_push_init(&call);
		v_push_argl(&call, args[0]);
		for (i = 1; i <= 10; i++)
			v_arg_push(&call, (i & 1) ? V_D : V_F, args[i]);
		v_push_argl(&call, args[11]);
		total = v_ccalld(&call, (v_dptr)reference_mixed);
		v_retd(total);
	} else {
		if (!v_getreg(&total, V_D, V_TEMP) ||
		    !v_getreg(&converted, V_D, V_TEMP)) return 0;
		v_cvl2d(total, args[0]);
		for (i = 1; i <= 10; i++) {
			if ((i & 1) == 0) v_cvf2d(converted, args[i]);
			else v_movd(converted, args[i]);
			v_addd(total, total, converted);
		}
		v_cvl2d(converted, args[11]);
		v_addd(total, total, converted);
		v_retd(total);
	}
	function = v_end(0);
	return (mixed_function)function.d;
}

static int test_mixed_abi(void)
{
	mixed_function arithmetic = build_mixed(0);
	mixed_function caller = build_mixed(1);
	double expected;
	double direct;
	double called;

	if (!arithmetic || !caller) return 1;
	expected = reference_mixed(1099511627776L, 1.25, -2.5f, 3.75, 4.5f,
	                           -5.25, 6.5f, 7.75, -8.25f, 9.5, 10.25f,
	                           -1099511627700L);
	direct = arithmetic(1099511627776L, 1.25, -2.5f, 3.75, 4.5f,
	                    -5.25, 6.5f, 7.75, -8.25f, 9.5, 10.25f,
	                    -1099511627700L);
	called = caller(1099511627776L, 1.25, -2.5f, 3.75, 4.5f,
	                -5.25, 6.5f, 7.75, -8.25f, 9.5, 10.25f,
	                -1099511627700L);
	if (!close_enough(direct, expected) || !close_enough(called, expected)) {
		fprintf(stderr, "mixed ABI mismatch: %.17g %.17g %.17g\n",
		        direct, called, expected);
		return 1;
	}
	return v_code_free((void *)arithmetic) < 0 ||
	       v_code_free((void *)caller) < 0;
}

static int test_load_store_and_comparison(void)
{
	v_code *code = v_code_alloc(4096);
	v_reg_t args[2];
	v_reg_t value;
	v_label_t less;
	union v_fp function;
	int (*generated)(double *, double);
	double storage = 3.5;

	if (!code) return 1;
	v_lambda("load_store_compare", "%p%d", args, V_LEAF, code, 4096);
	if (!v_getreg(&value, V_D, V_TEMP)) return 1;
	v_lddi(value, args[0], 0);
	less = v_genlabel();
	v_bltd(value, args[1], less);
	v_stdi(args[1], args[0], 0);
	v_retii(0);
	v_label(less);
	v_retii(1);
	function = v_end(0);
	generated = (int (*)(double *, double))function.i;
	if (generated(&storage, 4.5) != 1 || storage != 3.5) return 1;
	if (generated(&storage, 2.0) != 0 || storage != 2.0) return 1;
	if (generated(&storage, NAN) != 0 || !isnan(storage)) return 1;
	return v_code_free(code) < 0;
}

int main(void)
{
	if (test_exact_encoder()) return 1;
	if (test_mixed_abi()) return 1;
	if (test_load_store_and_comparison()) return 1;
	puts("Phase 6B direct-vcode tests passed");
	return 0;
}
