#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "vcode.h"

static int expect(v_code *code, int offset, const unsigned char *bytes,
                  size_t length, const char *name)
{
	if (memcmp(code + offset, bytes, length) == 0) return 0;
	fprintf(stderr, "%s encoding differs at offset %d\n", name, offset);
	return 1;
}

int main(void)
{
	static const unsigned char rex_registers[] = { 0x4d, 0x89, 0xcc };
	static const unsigned char sib_address[] = { 0x4f, 0x8b, 0x14, 0x0c };
	static const unsigned char stack_address[] = { 0x48, 0x8b, 0x45, 0xf8 };
	static const unsigned char rip_address[] = {
		0x4c, 0x8d, 0x15, 0x78, 0x56, 0x34, 0x12
	};
	v_code *code = v_code_alloc(4096);
	int nbytes;

	if (!code) return 1;
	v_begin_incremental(code, 4096);
	v_movp(__R12, __R9);
	v_ldp(__R10, __R12, __R9);
	v_ldpi(__RAX, __RBP, -8);
	v_x64_emit_lea_rip(__R10, 0x12345678);
	nbytes = v_end_incremental();

	if (nbytes != 18) return 1;
	if (expect(code, 0, rex_registers, sizeof rex_registers, "REX/R8-R15")) return 1;
	if (expect(code, 3, sib_address, sizeof sib_address, "ModRM/SIB")) return 1;
	if (expect(code, 7, stack_address, sizeof stack_address, "stack-relative")) return 1;
	if (expect(code, 11, rip_address, sizeof rip_address, "RIP-relative")) return 1;
	if (v_code_free(code) < 0) return 1;
	puts("x86-64 encoder tests passed");
	return 0;
}
