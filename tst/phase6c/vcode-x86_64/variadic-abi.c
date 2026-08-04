#include <stdarg.h>
#include <stdio.h>

#include "vcode.h"

static double reference_variadic(int count, ...)
{
	va_list arguments;
	double total = 0.0;
	int i;
	va_start(arguments, count);
	for (i = 0; i < count; i++) total += va_arg(arguments, double);
	va_end(arguments);
	return total;
}

static int test_generated_integer_callee(void)
{
	v_code *code = v_code_alloc(4096);
	v_reg_t fixed;
	v_reg_t list;
	v_reg_t total;
	union v_fp function;
	long (*generated)(int, ...);
	int i;

	if (!code) return 1;
	v_lambda_variadic("integer_variadic", "%i", &fixed, V_LEAF, code, 4096);
	list = v_localb(24);
	if (!v_getreg(&total, V_L, V_TEMP)) return 1;
	v_setl(total, 0);
	v_va_start(list);
	for (i = 0; i < 8; i++) {
		v_reg_t value = v_va_arg(list, V_L);
		v_addl(total, total, value);
		v_putreg(value, V_L);
	}
	v_va_end(list);
	v_retl(total);
	function = v_end(0);
	generated = (long (*)(int, ...))function.l;
	if (generated(8, 1L, 2L, 3L, 4L, 5L, 6L, 7L, 8L) != 36)
		return 1;
	return v_code_free(code) < 0;
}

static int test_generated_floating_callee(void)
{
	v_code *code = v_code_alloc(4096);
	v_reg_t fixed;
	v_reg_t list;
	v_reg_t total;
	union v_fp function;
	double (*generated)(int, ...);
	int i;

	if (!code) return 1;
	v_lambda_variadic("floating_variadic", "%i", &fixed, V_LEAF, code, 4096);
	list = v_localb(24);
	if (!v_getreg(&total, V_D, V_TEMP)) return 1;
	v_setd(total, 0.0);
	v_va_start(list);
	for (i = 0; i < 10; i++) {
		v_reg_t value = v_va_arg(list, V_D);
		v_addd(total, total, value);
		v_putreg(value, V_D);
	}
	v_va_end(list);
	v_retd(total);
	function = v_end(0);
	generated = (double (*)(int, ...))function.d;
	if (generated(10, 1.0, 2.0, 3.0, 4.0, 5.0,
	              6.0, 7.0, 8.0, 9.0, 10.0) != 55.0)
		return 1;
	return v_code_free(code) < 0;
}

static int test_fixed_arguments_crossing_stack_boundary(void)
{
	v_code *code = v_code_alloc(4096);
	v_reg_t fixed[7];
	v_reg_t list;
	v_reg_t total;
	union v_fp function;
	long (*generated)(long, long, long, long, long, long, long, ...);
	int i;

	if (!code) return 1;
	v_lambda_variadic("fixed_stack_variadic", "%l%l%l%l%l%l%l",
	                  fixed, V_LEAF, code, 4096);
	list = v_localb(24);
	if (!v_getreg(&total, V_L, V_TEMP)) return 1;
	v_setl(total, 0);
	v_va_start(list);
	for (i = 0; i < 3; i++) {
		v_reg_t value = v_va_arg(list, V_L);
		v_addl(total, total, value);
		v_putreg(value, V_L);
	}
	v_retl(total);
	function = v_end(0);
	generated = (long (*)(long, long, long, long, long, long, long, ...))
	            function.l;
	if (generated(10, 20, 30, 40, 50, 60, 70, 1L, 2L, 3L) != 6)
		return 1;
	return v_code_free(code) < 0;
}

static int test_generated_variadic_caller(void)
{
	v_code *code = v_code_alloc(4096);
	struct v_cstate call;
	v_reg_t value;
	v_reg_t result;
	union v_fp function;
	double (*generated)(void);
	int i;

	if (!code) return 1;
	v_lambda("floating_variadic_caller", "", 0, V_NLEAF, code, 4096);
	v_push_init(&call);
	v_push_argii(&call, 10);
	if (!v_getreg(&value, V_D, V_TEMP)) return 1;
	for (i = 1; i <= 10; i++) {
		v_setd(value, (double)i);
		v_push_argd(&call, value);
	}
	result = v_ccalld(&call, (v_dptr)reference_variadic);
	v_retd(result);
	function = v_end(0);
	generated = (double (*)(void))function.d;
	if (generated() != 55.0) return 1;
	return v_code_free(code) < 0;
}

int main(void)
{
	if (test_generated_integer_callee()) return 1;
	if (test_generated_floating_callee()) return 1;
	if (test_fixed_arguments_crossing_stack_boundary()) return 1;
	if (test_generated_variadic_caller()) return 1;
	puts("Phase 6C direct-vcode tests passed");
	return 0;
}
