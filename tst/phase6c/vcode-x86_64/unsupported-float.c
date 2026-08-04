#include "vcode.h"

int main(void)
{
	v_code *code = v_code_alloc(4096);
	v_reg_t fixed;
	v_reg_t list;

	if (!code) return 1;
	v_lambda_variadic("unsupported_float", "%i", &fixed,
	                  V_LEAF, code, 4096);
	list = v_localb(24);
	v_va_start(list);
	(void)v_va_arg(list, V_F);
	return 0;
}
