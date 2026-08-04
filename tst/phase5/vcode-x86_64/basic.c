#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vcode.h"

static long external_sum8(long a, long b, long c, long d,
                          long e, long f, long g, long h)
{
	return a + b + c + d + e + f + g + h;
}

static int mapping_has_permissions(void *address, const char *permissions)
{
	FILE *maps = fopen("/proc/self/maps", "r");
	char line[256];
	unsigned long start;
	unsigned long end;
	char actual[5];
	unsigned long target = (unsigned long)(uintptr_t)address;

	if (!maps) return 0;
	while (fgets(line, sizeof line, maps)) {
		if (sscanf(line, "%lx-%lx %4s", &start, &end, actual) == 3 &&
		    target >= start && target < end) {
			fclose(maps);
			return strcmp(actual, permissions) == 0;
		}
	}
	fclose(maps);
	return 0;
}

static int test_integer_and_pointer(void)
{
	v_code *code = v_code_alloc(4096);
	v_reg_t args[3];
	v_reg_t temporary;
	union v_fp function;
	int nbytes;
	long (*generated)(long, long, long *);
	long value = 5;
	long result;

	if (!code) return 1;
	if (!mapping_has_permissions(code, "rw-p")) return 1;
	v_lambda("integer_and_pointer", "%l%l%p", args, V_LEAF, code, 4096);
	if (!v_getreg(&temporary, V_L, V_TEMP)) return 1;
	v_addl(temporary, args[0], args[1]);
	v_ldli(args[0], args[2], 0);
	v_mull(temporary, temporary, args[0]);
	v_retl(temporary);
	function = v_end(&nbytes);
	if (!mapping_has_permissions(code, "r-xp")) return 1;
	generated = (long (*)(long, long, long *))function.l;
	result = generated(7, 11, &value);
	if (result != 90 || nbytes <= 0) {
		fprintf(stderr, "integer/pointer result: %ld\n", result);
		return 1;
	}
	return v_code_free(code) < 0;
}

static int test_sysv_call(void)
{
	v_code *code = v_code_alloc(4096);
	v_reg_t args[8];
	v_reg_t result;
	struct v_cstate call;
	union v_fp function;
	long (*generated)(long, long, long, long, long, long, long, long);
	long value;
	int i;

	if (!code) return 1;
	v_lambda("sysv_call", "%l%l%l%l%l%l%l%l", args, V_NLEAF, code, 4096);
	v_push_init(&call);
	for (i = 0; i < 8; i++) v_push_argl(&call, args[i]);
	result = v_ccalll(&call, external_sum8);
	v_retl(result);
	function = v_end(0);
	generated = (long (*)(long, long, long, long, long, long, long, long))function.l;
	value = generated(1, 2, 3, 4, 5, 6, 7, 8);
	if (value != 36) {
		fprintf(stderr, "SysV call result: %ld\n", value);
		return 1;
	}
	return v_code_free(code) < 0;
}

static int test_signed_extension(void)
{
	v_code *code = v_code_alloc(4096);
	v_reg_t argument;
	v_reg_t result;
	union v_fp function;
	long (*generated)(int);

	if (!code) return 1;
	v_lambda("signed_extension", "%i", &argument, V_LEAF, code, 4096);
	if (!v_getreg(&result, V_L, V_TEMP)) return 1;
	v_cvi2l(result, argument);
	v_retl(result);
	function = v_end(0);
	generated = (long (*)(int))function.l;
	if (generated(-7) != -7L) return 1;
	return v_code_free(code) < 0;
}

static int test_wide_immediate(void)
{
	v_code *code = v_code_alloc(4096);
	v_reg_t argument;
	union v_fp function;
	long (*generated)(long);

	if (!code) return 1;
	v_lambda("wide_immediate", "%l", &argument, V_LEAF, code, 4096);
	v_addli(argument, argument, (intptr_t)1 << 40);
	v_retl(argument);
	function = v_end(0);
	generated = (long (*)(long))function.l;
	if (generated(9) != ((1L << 40) + 9)) return 1;
	return v_code_free(code) < 0;
}

int main(int argc, char **argv)
{
	if (argc == 2 && strcmp(argv[1], "--address") == 0) {
		v_code *code = v_code_alloc(4096);
		if (!code) return 1;
		printf("%p\n", (void *)code);
		return v_code_free(code) < 0;
	}
	if (sizeof(long) != 8 || sizeof(void *) != 8) return 1;
	if (test_integer_and_pointer()) return 1;
	if (test_sysv_call()) return 1;
	if (test_signed_extension()) return 1;
	if (test_wide_immediate()) return 1;
	puts("x86-64 vcode basic tests passed");
	return 0;
}
