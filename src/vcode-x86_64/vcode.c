#define VCODE_LIBRARY
#include "vcode.h"
#include "x86_64-codegen.h"

#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { MAX_LABELS = 256, MAX_REFS = 128, SAVED_REG_BYTES = 40 };
enum { REF_REL32, REF_ABS64 };
#define FP_SCRATCH0 __XMM14
#define FP_SCRATCH1 __XMM15

struct label_record {
	v_code *address;
	void *references[MAX_REFS];
	unsigned char reference_types[MAX_REFS];
	int reference_count;
};

static struct label_record labels[MAX_LABELS];
static int label_count;
static v_label_t epilogue_label;
static v_code *code_start;
static v_code *code_limit;
static v_code *stack_size_patch;
static int next_virtual;
static int allocated[32];
static int register_class[32];
static v_reg_type *dynamic_params[MAX_ARGS];
static int dynamic_param_types[MAX_ARGS];
static int dynamic_param_count;
static int variadic_function;
static int variadic_gp_offset;
static int variadic_fp_offset;
static int variadic_overflow_offset;
static int variadic_save_base;
static struct v_aggregate function_result;
static int function_has_aggregate_result;
static v_reg_t function_sret_address;

static const int callee_registers[] = { __RBX, __R12, __R13, __R14, __R15 };
static const int temporary_registers[] = { __RAX, __R10, __R11 };
static const int argument_registers[] = { __RDI, __RSI, __RDX, __RCX, __R8, __R9 };
static const int floating_registers[] = {
	/* Start outside the incoming XMM0-XMM7 bank so copying parameters into
	   allocated registers cannot overwrite a later incoming argument. */
	__XMM8, __XMM9, __XMM10, __XMM11, __XMM12, __XMM13,
	__XMM2, __XMM3, __XMM4, __XMM5, __XMM6, __XMM7
};
static const int floating_argument_registers[] = {
	__XMM0, __XMM1, __XMM2, __XMM3, __XMM4, __XMM5, __XMM6, __XMM7
};

uintptr_t v_ip;

static int type_is_64(int type)
{
	return type == V_L || type == V_UL || type == V_P;
}

static int type_is_signed(int type)
{
	return type == V_C || type == V_S || type == V_I || type == V_L;
}

static int type_is_float(int type)
{
	return type == V_F || type == V_D;
}

static unsigned int scalar_size(int type)
{
	switch (type) {
	case V_C: case V_UC: return 1;
	case V_S: case V_US: return 2;
	case V_I: case V_U: case V_F: return 4;
	case V_L: case V_UL: case V_P: case V_D: return 8;
	default: return 0;
	}
}

static enum v_abi_class merge_abi_class(enum v_abi_class left,
                                         enum v_abi_class right)
{
	if (left == right || right == V_ABI_NO_CLASS) return left;
	if (left == V_ABI_NO_CLASS) return right;
	if (left == V_ABI_MEMORY || right == V_ABI_MEMORY) return V_ABI_MEMORY;
	if (left == V_ABI_INTEGER || right == V_ABI_INTEGER) return V_ABI_INTEGER;
	return V_ABI_SSE;
}

int v_aggregate_classify(const struct v_aggregate *aggregate,
                         enum v_abi_class classes[2])
{
	unsigned int i;
	if (!aggregate || aggregate->size == 0 || aggregate->alignment == 0 ||
	    aggregate->alignment > 16 ||
	    (aggregate->alignment & (aggregate->alignment - 1)) != 0 ||
	    aggregate->field_count > V_MAX_AGGREGATE_FIELDS)
		v_fatal("invalid x86-64 aggregate description");
	classes[0] = classes[1] = V_ABI_NO_CLASS;
	if (aggregate->size > 16) {
		classes[0] = V_ABI_MEMORY;
		return 1;
	}
	for (i = 0; i < aggregate->field_count; i++) {
		unsigned int size = scalar_size(aggregate->fields[i].type);
		unsigned int offset = aggregate->fields[i].offset;
		enum v_abi_class field_class;
		unsigned int first, last, word;
		if (!size || offset > aggregate->size ||
		    size > aggregate->size - offset)
			v_fatal("invalid x86-64 aggregate field");
		if (offset % (size > 8 ? 8 : size)) {
			classes[0] = V_ABI_MEMORY;
			return 1;
		}
		field_class = type_is_float(aggregate->fields[i].type) ?
		              V_ABI_SSE : V_ABI_INTEGER;
		first = offset / 8;
		last = (offset + size - 1) / 8;
		for (word = first; word <= last; word++)
			classes[word] = merge_abi_class(classes[word], field_class);
	}
	for (i = 0; i < (aggregate->size + 7) / 8; i++)
		if (classes[i] == V_ABI_NO_CLASS) classes[i] = V_ABI_INTEGER;
	return (int)((aggregate->size + 7) / 8);
}

static int xmm_number(int reg)
{
	if (reg < __XMM0 || reg > __XMM15)
		v_fatal("expected an x86-64 XMM register");
	return reg - __XMM0;
}

static void require_space(size_t count)
{
	if (code_limit && v_ip + count > (uintptr_t)code_limit)
		v_fatal("x86-64 vcode buffer exhausted: used %lu of %lu bytes",
		        (unsigned long)(v_ip - (uintptr_t)code_start),
		        (unsigned long)(code_limit - code_start));
}

void v_fatal(char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	if (!strchr(fmt, '\n'))
		fputc('\n', stderr);
	exit(1);
}

void v_emit_byte(unsigned int value)
{
	require_space(1);
	*(uint8_t *)v_ip = (uint8_t)value;
	v_ip++;
}

void v_emit_u32(uint32_t value)
{
	require_space(4);
	memcpy((void *)v_ip, &value, sizeof value);
	v_ip += sizeof value;
}

void v_emit_u64(uint64_t value)
{
	require_space(8);
	memcpy((void *)v_ip, &value, sizeof value);
	v_ip += sizeof value;
}

void v_skip_bytes(size_t count)
{
	require_space(count);
	v_ip += count;
}

static void emit_rex(int width64, int reg, int index, int base)
{
	unsigned int rex = 0x40;
	if (width64) rex |= 0x08;
	if (reg >= 8) rex |= 0x04;
	if (index >= 8) rex |= 0x02;
	if (base >= 8) rex |= 0x01;
	if (rex != 0x40)
		v_emit_byte(rex);
}

static void emit_modrm(unsigned int mod, int reg, int rm)
{
	v_emit_byte((mod << 6) | ((reg & 7) << 3) | (rm & 7));
}

static void emit_push(int reg)
{
	if (reg >= 8) v_emit_byte(0x41);
	v_emit_byte(0x50 + (reg & 7));
}

static void emit_pop(int reg)
{
	if (reg >= 8) v_emit_byte(0x41);
	v_emit_byte(0x58 + (reg & 7));
}

static void emit_mov_rr(int dst, int src, int width64)
{
	/* A 32-bit self move is not redundant on x86-64: it clears the upper
	   half and is used to canonicalize unsigned values before widening. */
	if (dst == src && width64) return;
	emit_rex(width64, src, 0, dst);
	v_emit_byte(0x89);
	emit_modrm(3, src, dst);
}

static void emit_extend_rr(int dst, int src, int bits, int signed_value,
                           int width64)
{
	unsigned int rex = 0x40;

	if (width64) rex |= 0x08;
	if (dst >= 8) rex |= 0x04;
	if (src >= 8) rex |= 0x01;
	/* A REX prefix selects SIL/DIL rather than the legacy high-byte
	   registers when an 8-bit source is numbered four through seven. */
	if (rex != 0x40 || (bits == 8 && src >= 4))
		v_emit_byte(rex);
	v_emit_byte(0x0f);
	v_emit_byte(signed_value ? (bits == 8 ? 0xbe : 0xbf) :
	                           (bits == 8 ? 0xb6 : 0xb7));
	emit_modrm(3, dst, src);
}

static void emit_mov_imm(int reg, uint64_t value, int width64)
{
	if (width64) {
		emit_rex(1, 0, 0, reg);
		v_emit_byte(0xb8 + (reg & 7));
		v_emit_u64(value);
	} else {
		emit_rex(0, 0, 0, reg);
		v_emit_byte(0xb8 + (reg & 7));
		v_emit_u32((uint32_t)value);
	}
}

static void emit_memory_address(int reg_field, int base, int index,
                                int32_t displacement)
{
	unsigned int mod;
	int needs_sib;

	if (base == v_zero) {
		emit_modrm(0, reg_field, 4);
		v_emit_byte(((index < 0 ? 4 : index) & 7) << 3 | 5);
		v_emit_u32((uint32_t)displacement);
		return;
	}

	needs_sib = index >= 0 || (base & 7) == 4;
	if (displacement == 0 && (base & 7) != 5)
		mod = 0;
	else if (displacement >= -128 && displacement <= 127)
		mod = 1;
	else
		mod = 2;

	emit_modrm(mod, reg_field, needs_sib ? 4 : base);
	if (needs_sib)
		v_emit_byte((((index < 0 ? 4 : index) & 7) << 3) | (base & 7));
	if (mod == 1)
		v_emit_byte((uint8_t)displacement);
	else if (mod == 2 || (mod == 0 && (base & 7) == 5))
		v_emit_u32((uint32_t)displacement);
}

static void emit_memory_operand(int reg_field, int base, int index,
                                int32_t displacement, int width64,
                                unsigned int opcode)
{
	emit_rex(width64, reg_field, index, base == v_zero ? 0 : base);
	v_emit_byte(opcode);
	emit_memory_address(reg_field, base, index, displacement);
}

static void emit_sse_prefix(int type)
{
	v_emit_byte(type == V_F ? 0xf3 : 0xf2);
}

static void emit_sse_rr(int type, unsigned int opcode, int dst, int src)
{
	int dst_number = xmm_number(dst);
	int src_number = xmm_number(src);
	emit_sse_prefix(type);
	emit_rex(0, dst_number, 0, src_number);
	v_emit_byte(0x0f);
	v_emit_byte(opcode);
	emit_modrm(3, dst_number, src_number);
}

static void emit_sse_memory(int type, unsigned int opcode, int xmm,
                            int base, int index, int32_t displacement)
{
	int number = xmm_number(xmm);
	emit_sse_prefix(type);
	emit_rex(0, number, index, base == v_zero ? 0 : base);
	v_emit_byte(0x0f);
	v_emit_byte(opcode);
	emit_memory_address(number, base, index, displacement);
}

static void emit_load_memory(int type, int dst, int base, int index,
                             int32_t displacement)
{
	if (type_is_float(type)) {
		emit_sse_memory(type, 0x10, dst, base, index, displacement);
		return;
	}
	switch (type) {
	case V_C:
		emit_rex(0, dst, index, base == v_zero ? 0 : base);
		v_emit_byte(0x0f);
		v_emit_byte(0xbe);
		emit_memory_address(dst, base, index, displacement);
		break;
	case V_UC:
		emit_rex(0, dst, index, base == v_zero ? 0 : base);
		v_emit_byte(0x0f);
		v_emit_byte(0xb6);
		emit_memory_address(dst, base, index, displacement);
		break;
	case V_S:
		emit_rex(0, dst, index, base == v_zero ? 0 : base);
		v_emit_byte(0x0f);
		v_emit_byte(0xbf);
		emit_memory_address(dst, base, index, displacement);
		break;
	case V_US:
		emit_rex(0, dst, index, base == v_zero ? 0 : base);
		v_emit_byte(0x0f);
		v_emit_byte(0xb7);
		emit_memory_address(dst, base, index, displacement);
		break;
	default:
		emit_memory_operand(dst, base, index, displacement,
		                    type_is_64(type), 0x8b);
		break;
	}
}

static void emit_store_memory(int type, int src, int base, int index,
                              int32_t displacement)
{
	if (type_is_float(type)) {
		emit_sse_memory(type, 0x11, src, base, index, displacement);
	} else if (type == V_C || type == V_UC) {
		emit_memory_operand(src, base, index, displacement, 0, 0x88);
	} else if (type == V_S || type == V_US) {
		v_emit_byte(0x66);
		emit_memory_operand(src, base, index, displacement, 0, 0x89);
	} else {
		emit_memory_operand(src, base, index, displacement,
		                    type_is_64(type), 0x89);
	}
}

static int integer_chunk_type(unsigned int size)
{
	if (size >= 8) return V_UL;
	if (size >= 4) return V_U;
	if (size >= 2) return V_US;
	return V_UC;
}

static void emit_group_binary(unsigned int opcode, int dst, int src,
                              int width64);
static void emit_group_immediate(int group, int reg, intptr_t immediate,
                                 int width64);

static void emit_shift_immediate(int reg, unsigned int bits, int right)
{
	emit_rex(1, 0, 0, reg);
	v_emit_byte(0xc1);
	emit_modrm(3, right ? 5 : 4, reg);
	v_emit_byte(bits & 0x3f);
}

static int scratch_register_avoiding(int first, int second)
{
	static const int candidates[] = { __R10, __R11, __RAX, __RCX };
	unsigned int i;
	for (i = 0; i < sizeof candidates / sizeof candidates[0]; i++)
		if (candidates[i] != first && candidates[i] != second)
			return candidates[i];
	v_fatal("cannot select an x86-64 aggregate scratch register");
	return __R10;
}

static void emit_copy_bytes(int destination, int source, unsigned int size)
{
	unsigned int offset = 0;
	int scratch = scratch_register_avoiding(destination, source);
	while (offset < size) {
		unsigned int remaining = size - offset;
		unsigned int chunk = remaining >= 8 ? 8 : remaining >= 4 ? 4 :
		                     remaining >= 2 ? 2 : 1;
		int type = integer_chunk_type(chunk);
		emit_load_memory(type, scratch, source, -1, (int32_t)offset);
		emit_store_memory(type, scratch, destination, -1, (int32_t)offset);
		offset += chunk;
	}
}

static void emit_load_aggregate_word(int target, int source,
                                     int32_t offset,
                                     unsigned int available,
                                     enum v_abi_class abi_class)
{
	if (abi_class == V_ABI_SSE) {
		if (available >= 8)
			emit_load_memory(V_D, target, source, -1, offset);
		else if (available >= 4)
			emit_load_memory(V_F, target, source, -1, offset);
		else
			v_fatal("unsupported partial SSE aggregate eightbyte");
	} else {
		unsigned int consumed = 0;
		unsigned int width = available > 8 ? 8 : available;
		int scratch = scratch_register_avoiding(target, source);
		emit_mov_imm(target, 0, 1);
		while (consumed < width) {
			unsigned int remaining = width - consumed;
			unsigned int chunk = remaining >= 4 ? 4 : remaining >= 2 ? 2 : 1;
			int part = consumed == 0 ? target : scratch;
			emit_load_memory(integer_chunk_type(chunk), part, source, -1,
			                 offset + (int32_t)consumed);
			if (consumed) {
				emit_shift_immediate(part, consumed * 8, 0);
				emit_group_binary(0x09, target, part, 1);
			}
			consumed += chunk;
		}
	}
}

static void emit_store_aggregate_word(int destination, int source,
                                      int32_t offset,
                                      unsigned int available,
                                      enum v_abi_class abi_class)
{
	if (abi_class == V_ABI_SSE) {
		if (available >= 8)
			emit_store_memory(V_D, source, destination, -1, offset);
		else if (available >= 4)
			emit_store_memory(V_F, source, destination, -1, offset);
		else
			v_fatal("unsupported partial SSE aggregate eightbyte");
	} else {
		unsigned int consumed = 0;
		unsigned int width = available > 8 ? 8 : available;
		int scratch = scratch_register_avoiding(destination, source);
		emit_mov_rr(scratch, source, 1);
		while (consumed < width) {
			unsigned int remaining = width - consumed;
			unsigned int chunk = remaining >= 4 ? 4 : remaining >= 2 ? 2 : 1;
			emit_store_memory(integer_chunk_type(chunk), scratch, destination, -1,
			                  offset + (int32_t)consumed);
			consumed += chunk;
			if (consumed < width)
				emit_shift_immediate(scratch, chunk * 8, 1);
		}
	}
}

static int load_register(v_reg_t reg, int type, int scratch)
{
	if (type_is_float(type)) {
		if (__NVIRT(reg)) {
			xmm_number(reg);
			return reg;
		}
		emit_load_memory(type, scratch, __RBP, -1, __ADDR(reg));
		return scratch;
	}
	if (reg == v_zero) {
		emit_mov_imm(scratch, 0, type_is_64(type));
		return scratch;
	}
	if (__NVIRT(reg))
		return reg;
	emit_load_memory(type, scratch, __RBP, -1, __ADDR(reg));
	return scratch;
}

static void store_register(v_reg_t reg, int type, int source)
{
	if (type_is_float(type)) {
		if (__NVIRT(reg))
			emit_sse_rr(type, 0x10, reg, source);
		else
			emit_store_memory(type, source, __RBP, -1, __ADDR(reg));
	} else if (__NVIRT(reg))
		emit_mov_rr(reg, source, type_is_64(type));
	else
		emit_store_memory(type, source, __RBP, -1, __ADDR(reg));
}

static void emit_group_binary(unsigned int opcode, int dst, int src, int width64)
{
	emit_rex(width64, src, 0, dst);
	v_emit_byte(opcode);
	emit_modrm(3, src, dst);
}

static void emit_group_immediate(int group, int reg, intptr_t immediate,
                                 int width64)
{
	emit_rex(width64, group, 0, reg);
	v_emit_byte(0x81);
	emit_modrm(3, group, reg);
	v_emit_u32((uint32_t)immediate);
}

void v_emit_move(int type, v_reg_t rd, v_reg_t rs)
{
	int source = load_register(rs, type, type_is_float(type) ? FP_SCRATCH0 : __R8);
	store_register(rd, type, source);
}

static void emit_integer_float_conversion(int destination_type, int source_type,
                                          int destination, int source)
{
	int destination_number = xmm_number(destination);
	emit_sse_prefix(destination_type);
	emit_rex(type_is_64(source_type), destination_number, 0, source);
	v_emit_byte(0x0f);
	v_emit_byte(0x2a);
	emit_modrm(3, destination_number, source);
}

static void emit_float_integer_conversion(int destination_type, int source_type,
                                          int destination, int source)
{
	int source_number = xmm_number(source);
	emit_sse_prefix(source_type);
	emit_rex(type_is_64(destination_type), destination, 0, source_number);
	v_emit_byte(0x0f);
	v_emit_byte(0x2c);
	emit_modrm(3, destination, source_number);
}

void v_emit_convert(int destination_type, int source_type,
                    v_reg_t rd, v_reg_t rs)
{
	if (type_is_float(destination_type) || type_is_float(source_type)) {
		if (type_is_float(destination_type) && type_is_float(source_type)) {
			int source = load_register(rs, source_type, FP_SCRATCH0);
			int target = __NVIRT(rd) ? rd : FP_SCRATCH1;
			if (destination_type == source_type)
				emit_sse_rr(destination_type, 0x10, target, source);
			else {
				v_emit_byte(destination_type == V_D ? 0xf3 : 0xf2);
				emit_rex(0, xmm_number(target), 0, xmm_number(source));
				v_emit_byte(0x0f); v_emit_byte(0x5a);
				emit_modrm(3, xmm_number(target), xmm_number(source));
			}
			if (!__NVIRT(rd)) store_register(rd, destination_type, target);
			return;
		}
		if (type_is_float(destination_type)) {
			int source = load_register(rs, source_type, __R8);
			int target = __NVIRT(rd) ? rd : FP_SCRATCH0;
			if (source_type != V_I && source_type != V_L)
				v_fatal("unsupported unsigned integer to floating-point conversion");
			emit_integer_float_conversion(destination_type, source_type, target, source);
			if (!__NVIRT(rd)) store_register(rd, destination_type, target);
			return;
		}
		{
			int source = load_register(rs, source_type, FP_SCRATCH0);
			int target = __NVIRT(rd) ? rd : __R8;
			if (destination_type != V_I && destination_type != V_L)
				v_fatal("unsupported floating-point to unsigned integer conversion");
			emit_float_integer_conversion(destination_type, source_type, target, source);
			if (!__NVIRT(rd)) store_register(rd, destination_type, target);
			return;
		}
	}
	int source = load_register(rs, source_type, __R8);
	int target = __NVIRT(rd) ? rd : __R9;

	/* Registers do not carry a type. Canonicalize every narrowing conversion
	   immediately so a later widening conversion observes the C value rather
	   than stale upper bits from the original register. */
	if (destination_type == V_C || destination_type == V_UC) {
		emit_extend_rr(target, source, 8, destination_type == V_C, 0);
	} else if (destination_type == V_S || destination_type == V_US) {
		emit_extend_rr(target, source, 16, destination_type == V_S, 0);
	} else if (!type_is_64(destination_type)) {
		if (source_type == V_C || source_type == V_UC)
			emit_extend_rr(target, source, 8, source_type == V_C, 0);
		else if (source_type == V_S || source_type == V_US)
			emit_extend_rr(target, source, 16, source_type == V_S, 0);
		else
			emit_mov_rr(target, source, 0);
	} else if (source_type == V_C || source_type == V_UC) {
		emit_extend_rr(target, source, 8, source_type == V_C,
		               source_type == V_C);
	} else if (source_type == V_S || source_type == V_US) {
		emit_extend_rr(target, source, 16, source_type == V_S,
		               source_type == V_S);
	} else if (source_type == V_I) {
		emit_rex(1, target, 0, source);
		v_emit_byte(0x63);
		emit_modrm(3, target, source);
	} else if (source_type == V_U) {
		emit_mov_rr(target, source, 0);
	} else {
		emit_mov_rr(target, source, 1);
	}
	if (!__NVIRT(rd))
		store_register(rd, destination_type, target);
}

void v_emit_set(int type, v_reg_t rd, uintptr_t immediate)
{
	int target = __NVIRT(rd) ? rd : __R8;
	emit_mov_imm(target, immediate, type_is_64(type));
	if (!__NVIRT(rd))
		store_register(rd, type, target);
}

void v_emit_set_float(int type, v_reg_t rd, double value)
{
	int target = __NVIRT(rd) ? rd : FP_SCRATCH0;
	if (type == V_F) {
		float narrowed = (float)value;
		uint32_t bits;
		memcpy(&bits, &narrowed, sizeof bits);
		emit_mov_imm(__R8, bits, 0);
		v_emit_byte(0x66); emit_rex(0, xmm_number(target), 0, __R8);
		v_emit_byte(0x0f); v_emit_byte(0x6e);
		emit_modrm(3, xmm_number(target), __R8);
	} else if (type == V_D) {
		uint64_t bits;
		memcpy(&bits, &value, sizeof bits);
		emit_mov_imm(__R8, bits, 1);
		v_emit_byte(0x66); emit_rex(1, xmm_number(target), 0, __R8);
		v_emit_byte(0x0f); v_emit_byte(0x6e);
		emit_modrm(3, xmm_number(target), __R8);
	} else {
		v_fatal("floating-point constant requires float or double type");
	}
	if (!__NVIRT(rd)) store_register(rd, type, target);
}

void v_emit_binary(int operation, int type, v_reg_t rd, v_reg_t rs1,
                   v_reg_t rs2)
{
	if (type_is_float(type)) {
		int lhs = load_register(rs1, type, FP_SCRATCH0);
		int rhs = load_register(rs2, type,
		                        lhs == FP_SCRATCH0 ? FP_SCRATCH1 : FP_SCRATCH0);
		int target = __NVIRT(rd) ? rd : FP_SCRATCH0;
		unsigned int opcode;
		switch (operation) {
		case V_X64_ADD: opcode = 0x58; break;
		case V_X64_SUB: opcode = 0x5c; break;
		case V_X64_MUL: opcode = 0x59; break;
		case V_X64_DIV: opcode = 0x5e; break;
		default: v_fatal("unsupported scalar SSE binary operation");
		}
		if (target != lhs) emit_sse_rr(type, 0x10, target, lhs);
		emit_sse_rr(type, opcode, target, rhs);
		if (!__NVIRT(rd)) store_register(rd, type, target);
		return;
	}
	int width64 = type_is_64(type);
	int lhs;
	int rhs;
	unsigned int opcode;

	if (operation == V_X64_DIV || operation == V_X64_MOD) {
		rhs = load_register(rs2, type, __R9);
		if (rhs != __R9) emit_mov_rr(__R9, rhs, width64);
		emit_push(__RAX);
		emit_push(__RDX);
		lhs = load_register(rs1, type, __RAX);
		if (lhs != __RAX) emit_mov_rr(__RAX, lhs, width64);
		if (type_is_signed(type)) {
			if (width64) {
				v_emit_byte(0x48); v_emit_byte(0x99);
			} else {
				v_emit_byte(0x99);
			}
		} else {
			emit_group_binary(0x31, __RDX, __RDX, width64);
		}
		emit_rex(width64, 7, 0, __R9);
		v_emit_byte(0xf7);
		emit_modrm(3, type_is_signed(type) ? 7 : 6, __R9);
		emit_mov_rr(__R8, operation == V_X64_MOD ? __RDX : __RAX, width64);
		emit_pop(__RDX);
		emit_pop(__RAX);
		store_register(rd, type, __R8);
		return;
	}

	lhs = load_register(rs1, type, __R8);
	if (lhs != __R8) emit_mov_rr(__R8, lhs, width64);
	rhs = load_register(rs2, type, __R9);
	if (rhs != __R9) emit_mov_rr(__R9, rhs, width64);

	switch (operation) {
	case V_X64_ADD: opcode = 0x01; break;
	case V_X64_SUB: opcode = 0x29; break;
	case V_X64_AND: opcode = 0x21; break;
	case V_X64_OR:  opcode = 0x09; break;
	case V_X64_XOR: opcode = 0x31; break;
	case V_X64_MUL:
		emit_rex(width64, __R8, 0, __R9);
		v_emit_byte(0x0f); v_emit_byte(0xaf);
		emit_modrm(3, __R8, __R9);
		store_register(rd, type, __R8);
		return;
	case V_X64_LSH:
	case V_X64_RSH:
		emit_mov_rr(__RCX, __R9, 0);
		emit_rex(width64, 0, 0, __R8);
		v_emit_byte(0xd3);
		emit_modrm(3, operation == V_X64_LSH ? 4 :
		           (type_is_signed(type) ? 7 : 5), __R8);
		store_register(rd, type, __R8);
		return;
	default: v_fatal("unsupported x86-64 binary operation");
	}
	emit_group_binary(opcode, __R8, __R9, width64);
	store_register(rd, type, __R8);
}

void v_emit_binary_imm(int operation, int type, v_reg_t rd, v_reg_t rs,
                       intptr_t immediate)
{
	int width64 = type_is_64(type);
	int source;
	int group;

	if (operation == V_X64_DIV || operation == V_X64_MOD) {
		v_emit_set(type, __R9, (uintptr_t)immediate);
		v_emit_binary(operation, type, rd, rs, __R9);
		return;
	}
	if (width64 && operation != V_X64_LSH && operation != V_X64_RSH &&
	    (immediate < INT32_MIN || immediate > INT32_MAX)) {
		v_emit_set(type, __R9, (uintptr_t)immediate);
		v_emit_binary(operation, type, rd, rs, __R9);
		return;
	}
	source = load_register(rs, type, __R8);
	if (source != __R8) emit_mov_rr(__R8, source, width64);
	if (operation == V_X64_MUL) {
		emit_rex(width64, __R8, 0, __R8);
		v_emit_byte(0x69);
		emit_modrm(3, __R8, __R8);
		v_emit_u32((uint32_t)immediate);
	} else if (operation == V_X64_LSH || operation == V_X64_RSH) {
		emit_rex(width64, 0, 0, __R8);
		v_emit_byte(0xc1);
		emit_modrm(3, operation == V_X64_LSH ? 4 :
		           (type_is_signed(type) ? 7 : 5), __R8);
		v_emit_byte((unsigned int)immediate & 0x3f);
	} else {
		switch (operation) {
		case V_X64_ADD: group = 0; break;
		case V_X64_OR: group = 1; break;
		case V_X64_AND: group = 4; break;
		case V_X64_SUB: group = 5; break;
		case V_X64_XOR: group = 6; break;
		default: v_fatal("unsupported x86-64 immediate operation");
		}
		emit_group_immediate(group, __R8, immediate, width64);
	}
	store_register(rd, type, __R8);
}

void v_emit_unary(int operation, int type, v_reg_t rd, v_reg_t rs)
{
	if (type_is_float(type)) {
		int source;
		int target;
		if (operation != V_X64_NEG)
			v_fatal("unsupported scalar SSE unary operation");
		source = load_register(rs, type, FP_SCRATCH0);
		target = __NVIRT(rd) ? rd : FP_SCRATCH0;
		if (target != source) emit_sse_rr(type, 0x10, target, source);
		v_emit_set_float(type, FP_SCRATCH1, -0.0);
		if (type == V_D) v_emit_byte(0x66);
		emit_rex(0, xmm_number(target), 0, xmm_number(FP_SCRATCH1));
		v_emit_byte(0x0f); v_emit_byte(0x57);
		emit_modrm(3, xmm_number(target), xmm_number(FP_SCRATCH1));
		if (!__NVIRT(rd)) store_register(rd, type, target);
		return;
	}
	int width64 = type_is_64(type);
	int source = load_register(rs, type, __R9);
	if (source != __R9) emit_mov_rr(__R9, source, width64);
	if (operation == V_X64_NOT) {
		emit_mov_imm(__R8, 0, 0);
		emit_group_immediate(7, __R9, 0, width64);
		emit_rex(0, 0, 0, __R8);
		v_emit_byte(0x0f); v_emit_byte(0x94);
		emit_modrm(3, 0, __R8);
	} else {
		emit_mov_rr(__R8, __R9, width64);
		emit_rex(width64, operation == V_X64_COM ? 2 : 3, 0, __R8);
		v_emit_byte(0xf7);
		emit_modrm(3, operation == V_X64_COM ? 2 : 3, __R8);
	}
	store_register(rd, type, __R8);
}

void v_emit_load(int type, v_reg_t rd, v_reg_t base, intptr_t offset,
                 int offset_is_register)
{
	int base_register = base == v_zero ? v_zero : load_register(base, V_P, __R8);
	int index_register = -1;
	int32_t displacement = 0;
	int target = __NVIRT(rd) ? rd : (type_is_float(type) ? FP_SCRATCH0 : __R9);

	if (offset_is_register) {
		index_register = load_register((v_reg_t)offset, V_P, __R9);
	} else if (offset < INT32_MIN || offset > INT32_MAX) {
		if (base_register == v_zero) {
			emit_mov_imm(__R8, (uint64_t)offset, 1);
		} else {
			if (base_register != __R8)
				emit_mov_rr(__R8, base_register, 1);
			emit_mov_imm(__R9, (uint64_t)offset, 1);
			emit_group_binary(0x01, __R8, __R9, 1);
		}
		base_register = __R8;
	} else {
		displacement = (int32_t)offset;
	}
	emit_load_memory(type, target, base_register, index_register, displacement);
	if (!__NVIRT(rd)) store_register(rd, type, target);
}

void v_emit_store(int type, v_reg_t value, v_reg_t base, intptr_t offset,
                  int offset_is_register)
{
	int source;
	int base_register = base == v_zero ? v_zero : load_register(base, V_P, __R8);
	int index_register = -1;
	int32_t displacement = 0;

	if (offset_is_register) {
		source = load_register(value, type, type_is_float(type) ? FP_SCRATCH0 : __R9);
		index_register = load_register((v_reg_t)offset, V_P,
		                               base_register == __R8 ? __RCX : __R8);
	} else if (offset < INT32_MIN || offset > INT32_MAX) {
		if (base_register == v_zero) {
			emit_mov_imm(__R8, (uint64_t)offset, 1);
		} else {
			if (base_register != __R8)
				emit_mov_rr(__R8, base_register, 1);
			emit_mov_imm(__R9, (uint64_t)offset, 1);
			emit_group_binary(0x01, __R8, __R9, 1);
		}
		base_register = __R8;
		source = load_register(value, type, type_is_float(type) ? FP_SCRATCH0 : __R9);
	} else {
		displacement = (int32_t)offset;
		source = load_register(value, type, type_is_float(type) ? FP_SCRATCH0 : __R9);
	}
	emit_store_memory(type, source, base_register, index_register, displacement);
}

static unsigned int condition_opcode(int condition, int signed_type)
{
	switch (condition) {
	case V_X64_LT: return signed_type ? 0x8c : 0x82;
	case V_X64_LE: return signed_type ? 0x8e : 0x86;
	case V_X64_GT: return signed_type ? 0x8f : 0x87;
	case V_X64_GE: return signed_type ? 0x8d : 0x83;
	case V_X64_EQ: return 0x84;
	case V_X64_NE: return 0x85;
	default: v_fatal("unsupported x86-64 branch condition");
	}
	return 0;
}

void v_emit_branch(int condition, int type, v_reg_t lhs, intptr_t rhs,
                   int rhs_is_register, v_label_t label)
{
	if (type_is_float(type)) {
		int left;
		int right;
		unsigned int opcode;
		v_label_t ordered = 0;
		if (!rhs_is_register)
			v_fatal("floating-point immediate branch is unsupported");
		left = load_register(lhs, type, FP_SCRATCH0);
		right = load_register((v_reg_t)rhs, type,
		                      left == FP_SCRATCH0 ? FP_SCRATCH1 : FP_SCRATCH0);
		if (type == V_D) v_emit_byte(0x66);
		emit_rex(0, xmm_number(left), 0, xmm_number(right));
		v_emit_byte(0x0f); v_emit_byte(0x2e);
		emit_modrm(3, xmm_number(left), xmm_number(right));
		if (condition == V_X64_NE) {
			v_emit_byte(0x0f); v_emit_byte(0x8a);
			v_dmark((v_code *)v_ip, label); v_emit_u32(0);
			opcode = 0x85;
		} else {
			if (condition == V_X64_LT || condition == V_X64_LE ||
			    condition == V_X64_EQ) {
				ordered = v_genlabel();
				v_emit_byte(0x0f); v_emit_byte(0x8a);
				v_dmark((v_code *)v_ip, ordered); v_emit_u32(0);
			}
			switch (condition) {
			case V_X64_LT: opcode = 0x82; break;
			case V_X64_LE: opcode = 0x86; break;
			case V_X64_GT: opcode = 0x87; break;
			case V_X64_GE: opcode = 0x83; break;
			case V_X64_EQ: opcode = 0x84; break;
			default: v_fatal("unsupported scalar SSE branch condition");
			}
		}
		v_emit_byte(0x0f); v_emit_byte(opcode);
		v_dmark((v_code *)v_ip, label); v_emit_u32(0);
		if (ordered) v_label(ordered);
		return;
	}
	int width64 = type_is_64(type);
	int left = load_register(lhs, type, __R8);
	if (left != __R8) emit_mov_rr(__R8, left, width64);
	if (rhs_is_register) {
		int right = load_register((v_reg_t)rhs, type, __R9);
		if (right != __R9) emit_mov_rr(__R9, right, width64);
		emit_group_binary(0x39, __R8, __R9, width64);
	} else {
		emit_group_immediate(7, __R8, rhs, width64);
	}
	v_emit_byte(0x0f);
	v_emit_byte(condition_opcode(condition, type_is_signed(type)));
	v_dmark((v_code *)v_ip, label);
	v_emit_u32(0);
}

void v_emit_jump_reg(v_reg_t reg)
{
	int target = load_register(reg, V_P, __R8);
	emit_rex(0, 4, 0, target);
	v_emit_byte(0xff);
	emit_modrm(3, 4, target);
}

void v_emit_jump_imm(uintptr_t address)
{
	emit_mov_imm(__R11, address, 1);
	v_emit_jump_reg(__R11);
}

void v_emit_call_reg(v_reg_t reg)
{
	int target = load_register(reg, V_P, __R8);
	emit_rex(0, 2, 0, target);
	v_emit_byte(0xff);
	emit_modrm(3, 2, target);
}

void v_emit_call_imm(uintptr_t address)
{
	emit_mov_imm(__R11, address, 1);
	v_emit_call_reg(__R11);
}

void v_x64_emit_lea_rip(v_reg_t destination, int32_t displacement)
{
	if (!__NVIRT(destination))
		v_fatal("RIP-relative LEA requires a physical destination register");
	emit_rex(1, destination, 0, 0);
	v_emit_byte(0x8d);
	emit_modrm(0, destination, 5);
	v_emit_u32((uint32_t)displacement);
}

void v_emit_nop(void)
{
	v_emit_byte(0x90);
}

static void labels_start(void)
{
	memset(labels, 0, sizeof labels);
	label_count = 1;
}

static void ensure_label(v_label_t label)
{
	if (label <= 0 || label >= MAX_LABELS)
		v_fatal("x86-64 vcode label out of range");
	if (label >= label_count)
		label_count = label + 1;
}

v_label_t v_genlabel(void)
{
	if (label_count >= MAX_LABELS)
		v_fatal("too many x86-64 vcode labels");
	return label_count++;
}

void v_label(v_label_t label)
{
	ensure_label(label);
	labels[label].address = (v_code *)v_ip;
}

static void add_reference(void *address, v_label_t label, int type)
{
	struct label_record *record;
	ensure_label(label);
	record = &labels[label];
	if (record->reference_count >= MAX_REFS)
		v_fatal("too many references to x86-64 vcode label");
	record->references[record->reference_count] = address;
	record->reference_types[record->reference_count] = (unsigned char)type;
	record->reference_count++;
}

void v_dlabel(void *address, v_label_t label)
{
	add_reference(address, label, REF_ABS64);
}

void v_dmark(v_code *address, v_label_t label)
{
	add_reference(address, label, REF_REL32);
}

v_code *v_getlabel(v_label_t label)
{
	ensure_label(label);
	return labels[label].address;
}

static int labels_end(void)
{
	int label;
	for (label = 1; label < label_count; label++) {
		int reference;
		if (labels[label].reference_count && !labels[label].address)
			return -1;
		for (reference = 0; reference < labels[label].reference_count;
		     reference++) {
			void *patch = labels[label].references[reference];
			if (labels[label].reference_types[reference] == REF_ABS64) {
				uint64_t value = (uint64_t)(uintptr_t)labels[label].address;
				memcpy(patch, &value, sizeof value);
			} else {
				intptr_t distance = (v_code *)labels[label].address -
				                    ((v_code *)patch + 4);
				int32_t relative;
				if (distance < INT32_MIN || distance > INT32_MAX)
					v_fatal("x86-64 vcode branch exceeds rel32 range");
				relative = (int32_t)distance;
				memcpy(patch, &relative, sizeof relative);
			}
		}
	}
	return 0;
}

static void registers_start(void)
{
	memset(allocated, 0, sizeof allocated);
	memset(register_class, 0, sizeof register_class);
	next_virtual = 0;
}

static int allocate_from(const int *registers, size_t count, int class_type,
                         v_reg_t *result)
{
	size_t i;
	for (i = 0; i < count; i++) {
		int reg = registers[i];
		if (!allocated[reg]) {
			allocated[reg] = 1;
			register_class[reg] = class_type;
			*result = reg;
			return 1;
		}
	}
	return 0;
}

int v_getreg(v_reg_t *reg, int type, int class_type)
{
	if (type_is_float(type))
		return allocate_from(floating_registers,
		                     sizeof floating_registers / sizeof floating_registers[0],
		                     class_type, reg);
	if (class_type == V_TEMP &&
	    allocate_from(temporary_registers,
	                  sizeof temporary_registers / sizeof temporary_registers[0],
	                  class_type, reg))
		return 1;
	if (allocate_from(callee_registers,
	                  sizeof callee_registers / sizeof callee_registers[0],
	                  class_type, reg))
		return 1;
	if (class_type != V_TEMP &&
	    allocate_from(temporary_registers,
	                  sizeof temporary_registers / sizeof temporary_registers[0],
	                  class_type, reg))
		return 1;
	return 0;
}

int v_putreg(v_reg_t reg, int type)
{
	(void)type;
	if (__VIRT(reg))
		return 0;
	if (reg >= 32 || !allocated[reg])
		v_fatal("attempt to release an unallocated x86-64 register");
	allocated[reg] = 0;
	register_class[reg] = V_FREE;
	return 0;
}

void v_pickreg(v_reg_type reg, int type)
{
	if (!__NVIRT(reg) || reg >= 32 ||
	    (type_is_float(type) != (reg >= __XMM0)))
		v_fatal("invalid x86-64 register selection");
	allocated[reg] = 1;
	register_class[reg] = V_VAR;
}

int v_local(int type)
{
	(void)type;
	next_virtual++;
	return __MKVIRT(SAVED_REG_BYTES + next_virtual * 8);
}

int v_localb(unsigned int size)
{
	int slots = size ? (int)((size + 7) / 8) : 1;
	next_virtual += slots;
	return __MKVIRT(SAVED_REG_BYTES + next_virtual * 8);
}

static int parse_format(char *format, int *types)
{
	int count = 0;
	char *p = format;
	while (*p) {
		int type;
		if (*p++ != '%') v_fatal("invalid x86-64 vcode argument format");
		if (*p == 'u' && p[1] == 'c') { type = V_UC; p += 2; }
		else if (*p == 'u' && p[1] == 's') { type = V_US; p += 2; }
		else if (*p == 'u' && p[1] == 'l') { type = V_UL; p += 2; }
		else {
			switch (*p++) {
			case 'c': type = V_C; break;
			case 's': type = V_S; break;
			case 'i': type = V_I; break;
			case 'u': type = V_U; break;
			case 'l': type = V_L; break;
			case 'p': type = V_P; break;
			case 'f': type = V_F; break;
			case 'd': type = V_D; break;
			default: v_fatal("unsupported x86-64 vcode argument type");
			}
		}
		if (count >= MAX_ARGS) v_fatal("too many x86-64 vcode arguments");
		types[count++] = type;
	}
	return count;
}

static void emit_stack_adjust(int subtract, uint32_t amount)
{
	v_emit_byte(0x48);
	v_emit_byte(0x81);
	v_emit_byte(subtract ? 0xec : 0xc4);
	v_emit_u32(amount);
}

static void emit_lea_memory(int destination, int base, int32_t displacement);

static unsigned int align_up(unsigned int value, unsigned int alignment)
{
	return (value + alignment - 1) & ~(alignment - 1);
}

static void procedure_prologue(char *format, v_reg_t *args, int variadic)
{
	int types[MAX_ARGS];
	int count = parse_format(format, types);
	int integer_argument = 0;
	int floating_argument = 0;
	int stack_argument = 0;
	int i;

	emit_push(__RBP);
	emit_mov_rr(__RBP, __RSP, 1);
	for (i = 0; i < (int)(sizeof callee_registers / sizeof callee_registers[0]); i++)
		emit_push(callee_registers[i]);
	v_emit_byte(0x48); v_emit_byte(0x81); v_emit_byte(0xec);
	stack_size_patch = (v_code *)v_ip;
	v_emit_u32(0);
	variadic_function = variadic;
	if (variadic) {
		int save_slots = 22;
		next_virtual += save_slots;
		variadic_save_base = __MKVIRT(SAVED_REG_BYTES + next_virtual * 8);
		for (i = 0; i < 6; i++)
			emit_store_memory(V_UL, argument_registers[i], __RBP, -1,
			                  variadic_save_base + i * 8);
		for (i = 0; i < 8; i++)
			emit_store_memory(V_D, floating_argument_registers[i], __RBP, -1,
			                  variadic_save_base + 48 + i * 16);
	}

	for (i = 0; i < count; i++) {
		if (!v_getreg(&args[i], types[i], V_VAR))
			v_fatal("not enough x86-64 registers for function arguments");
		if (type_is_float(types[i]) && floating_argument < 8) {
			emit_sse_rr(types[i], 0x10, args[i],
			            floating_argument_registers[floating_argument++]);
		} else if (!type_is_float(types[i]) && integer_argument < 6) {
			emit_mov_rr(args[i], argument_registers[integer_argument++],
			            type_is_64(types[i]));
		} else {
			emit_load_memory(types[i], args[i], __RBP, -1,
			                 16 + stack_argument++ * 8);
		}
	}
	if (variadic) {
		variadic_gp_offset = integer_argument * 8;
		variadic_fp_offset = 48 + floating_argument * 16;
		variadic_overflow_offset = 16 + stack_argument * 8;
	}
}

static void procedure_aggregate_prologue(const struct v_aggregate *result,
                                         const struct v_parameter *parameters,
                                         int count, v_reg_t *args)
{
	int integer_argument = 0;
	int floating_argument = 0;
	unsigned int stack_offset = 16;
	int i;

	emit_push(__RBP);
	emit_mov_rr(__RBP, __RSP, 1);
	for (i = 0; i < (int)(sizeof callee_registers / sizeof callee_registers[0]); i++)
		emit_push(callee_registers[i]);
	v_emit_byte(0x48); v_emit_byte(0x81); v_emit_byte(0xec);
	stack_size_patch = (v_code *)v_ip;
	v_emit_u32(0);
	variadic_function = 0;
	function_has_aggregate_result = result != NULL;
	if (result) {
		enum v_abi_class result_classes[2];
		v_aggregate_classify(result, result_classes);
		function_result = *result;
		if (result_classes[0] == V_ABI_MEMORY) {
			function_sret_address = v_local(V_P);
			emit_store_memory(V_P, __RDI, __RBP, -1,
			                  __ADDR(function_sret_address));
			integer_argument = 1;
		}
	}

	for (i = 0; i < count; i++) {
		if (parameters[i].type != V_B) {
			int floating = type_is_float(parameters[i].type);
			if (!v_getreg(&args[i], parameters[i].type, V_VAR))
				v_fatal("not enough x86-64 registers for function arguments");
			if (floating && floating_argument < 8)
				emit_sse_rr(parameters[i].type, 0x10, args[i],
				            floating_argument_registers[floating_argument++]);
			else if (!floating && integer_argument < 6)
				emit_mov_rr(args[i], argument_registers[integer_argument++],
				            type_is_64(parameters[i].type));
			else {
				emit_load_memory(parameters[i].type, args[i], __RBP, -1,
				                 (int32_t)stack_offset);
				stack_offset += 8;
			}
		} else {
			enum v_abi_class classes[2];
			const struct v_aggregate *aggregate = parameters[i].aggregate;
			int words = v_aggregate_classify(aggregate, classes);
			int needed_integer = 0, needed_sse = 0, word;
			int in_memory;
			v_reg_t block;
			for (word = 0; word < words && classes[0] != V_ABI_MEMORY; word++) {
				needed_integer += classes[word] == V_ABI_INTEGER;
				needed_sse += classes[word] == V_ABI_SSE;
			}
			in_memory = classes[0] == V_ABI_MEMORY ||
			            integer_argument + needed_integer > 6 ||
			            floating_argument + needed_sse > 8;
			block = v_localb(aggregate->size);
			if (in_memory) {
				unsigned int alignment = aggregate->alignment > 8 ? 16 : 8;
				stack_offset = align_up(stack_offset, alignment);
				emit_lea_memory(__R10, __RBP, __ADDR(block));
				emit_lea_memory(__R11, __RBP, (int32_t)stack_offset);
				emit_copy_bytes(__R10, __R11, aggregate->size);
				stack_offset += align_up(aggregate->size, 8);
			} else {
				for (word = 0; word < words; word++) {
					unsigned int offset = (unsigned int)word * 8;
					unsigned int available = aggregate->size - offset;
					int source = classes[word] == V_ABI_SSE ?
					             floating_argument_registers[floating_argument++] :
					             argument_registers[integer_argument++];
					emit_store_aggregate_word(__RBP, source,
					                          __ADDR(block) + (int32_t)offset,
					                          available, classes[word]);
				}
			}
			if (!v_getreg(&args[i], V_P, V_VAR)) args[i] = v_local(V_P);
			emit_lea_memory(__R10, __RBP, __ADDR(block));
			store_register(args[i], V_P, __R10);
		}
	}
}

static void procedure_epilogue(void)
{
	uint32_t local_bytes = (uint32_t)next_virtual * 8;
	uint32_t frame_bytes = ((local_bytes + 23) & ~15U) - 8;
	int i;

	memcpy(stack_size_patch, &frame_bytes, sizeof frame_bytes);
	v_label(epilogue_label);
	emit_stack_adjust(0, frame_bytes);
	for (i = (int)(sizeof callee_registers / sizeof callee_registers[0]) - 1;
	     i >= 0; i--)
		emit_pop(callee_registers[i]);
	emit_pop(__RBP);
	v_emit_byte(0xc3);
}

void v_lambda(char *name, char *format, v_reg_t *args, int leaf,
              v_code *code, int nbytes)
{
	(void)name;
	(void)leaf;
	if (v_code_make_writable(code) < 0)
		v_fatal("cannot make x86-64 code buffer writable");
	code_start = code;
	code_limit = code + nbytes;
	v_ip = (uintptr_t)code;
	registers_start();
	labels_start();
	epilogue_label = v_genlabel();
	function_has_aggregate_result = 0;
	procedure_prologue(format, args, 0);
}

void v_lambda_variadic(char *name, char *format, v_reg_t *args, int leaf,
                       v_code *code, int nbytes)
{
	(void)name;
	(void)leaf;
	if (v_code_make_writable(code) < 0)
		v_fatal("cannot make x86-64 code buffer writable");
	code_start = code;
	code_limit = code + nbytes;
	v_ip = (uintptr_t)code;
	registers_start();
	labels_start();
	epilogue_label = v_genlabel();
	function_has_aggregate_result = 0;
	procedure_prologue(format, args, 1);
}

void v_lambda_aggregate(char *name, const struct v_aggregate *result,
                        const struct v_parameter *parameters, int count,
                        v_reg_t *args, int leaf, v_code *code, int nbytes)
{
	(void)name;
	(void)leaf;
	if (count < 0 || count > MAX_ARGS)
		v_fatal("invalid x86-64 aggregate parameter count");
	if (v_code_make_writable(code) < 0)
		v_fatal("cannot make x86-64 code buffer writable");
	code_start = code;
	code_limit = code + nbytes;
	v_ip = (uintptr_t)code;
	registers_start();
	labels_start();
	epilogue_label = v_genlabel();
	procedure_aggregate_prologue(result, parameters, count, args);
}

void v_emit_return(int type, v_reg_t reg, uintptr_t immediate, int is_immediate)
{
	if (type != V_V) {
		if (type_is_float(type)) {
			if (is_immediate)
				v_fatal("floating-point immediate return requires a register");
			else {
				int source = load_register(reg, type, FP_SCRATCH0);
				emit_sse_rr(type, 0x10, __XMM0, source);
			}
		} else if (is_immediate)
			emit_mov_imm(__RAX, immediate, type_is_64(type));
		else {
			int source = load_register(reg, type, __R8);
			emit_mov_rr(__RAX, source, type_is_64(type));
		}
	}
	v_emit_byte(0xe9);
	v_dmark((v_code *)v_ip, epilogue_label);
	v_emit_u32(0);
}

static void emit_lea_memory(int destination, int base, int32_t displacement)
{
	emit_rex(1, destination, 0, base);
	v_emit_byte(0x8d);
	emit_memory_address(destination, base, -1, displacement);
}

void v_copyb(v_reg_t destination, v_reg_t source, unsigned int size)
{
	int source_physical = __NVIRT(source) ? source : -1;
	int destination_scratch = scratch_register_avoiding(source_physical, -1);
	int destination_register = load_register(destination, V_P,
	                                         destination_scratch);
	int source_scratch = scratch_register_avoiding(destination_register, -1);
	int source_register = load_register(source, V_P, source_scratch);
	emit_copy_bytes(destination_register, source_register, size);
}

void v_retb(v_reg_t address, const struct v_aggregate *aggregate)
{
	enum v_abi_class classes[2];
	int words = v_aggregate_classify(aggregate, classes);
	int source = load_register(address, V_P, __R10);
	int integer_result = 0;
	int sse_result = 0;
	int word;
	if (!function_has_aggregate_result ||
	    aggregate->size != function_result.size)
		v_fatal("aggregate return does not match the x86-64 function signature");
	/* The integer return registers are overwritten while their eightbytes are
	 * assembled.  Preserve an aggregate address allocated in either register. */
	if (source == __RAX || source == __RDX) {
		emit_mov_rr(__R10, source, 1);
		source = __R10;
	}
	if (classes[0] == V_ABI_MEMORY) {
		int destination = load_register(function_sret_address, V_P, __R11);
		emit_copy_bytes(destination, source, aggregate->size);
		emit_mov_rr(__RAX, destination, 1);
	} else {
		for (word = 0; word < words; word++) {
			unsigned int offset = (unsigned int)word * 8;
			unsigned int available = aggregate->size - offset;
			int target = classes[word] == V_ABI_SSE ?
			             floating_argument_registers[sse_result++] :
			             (integer_result++ == 0 ? __RAX : __RDX);
			emit_load_aggregate_word(target, source, (int32_t)offset,
			                         available, classes[word]);
		}
	}
	v_emit_byte(0xe9);
	v_dmark((v_code *)v_ip, epilogue_label);
	v_emit_u32(0);
}

static void require_variadic_list(v_reg_t list)
{
	if (!variadic_function)
		v_fatal("va_list operation requires a variadic x86-64 function");
	if (__NVIRT(list))
		v_fatal("x86-64 va_list must be a stack local");
}

void v_va_start(v_reg_t list)
{
	require_variadic_list(list);
	emit_mov_imm(__R8, (uint64_t)variadic_gp_offset, 0);
	emit_store_memory(V_U, __R8, __RBP, -1, __ADDR(list));
	emit_mov_imm(__R8, (uint64_t)variadic_fp_offset, 0);
	emit_store_memory(V_U, __R8, __RBP, -1, __ADDR(list) + 4);
	emit_lea_memory(__R8, __RBP, variadic_overflow_offset);
	emit_store_memory(V_P, __R8, __RBP, -1, __ADDR(list) + 8);
	emit_lea_memory(__R8, __RBP, variadic_save_base);
	emit_store_memory(V_P, __R8, __RBP, -1, __ADDR(list) + 16);
}

v_reg_t v_va_arg(v_reg_t list, int type)
{
	int floating = type_is_float(type);
	int offset_field = floating ? 4 : 0;
	int limit = floating ? 176 : 48;
	int stride = floating ? 16 : 8;
	int target;
	v_reg_t result;
	v_label_t overflow;
	v_label_t done;

	require_variadic_list(list);
	if (type == V_F)
		v_fatal("float is promoted to double in a variadic argument list");
	if (type != V_I && type != V_U && type != V_L && type != V_UL &&
	    type != V_P && type != V_D)
		v_fatal("unsupported x86-64 variadic argument type");
	if (!v_getreg(&result, type, V_VAR)) result = v_local(type);
	target = __NVIRT(result) ? result : (floating ? FP_SCRATCH0 : __R10);
	overflow = v_genlabel();
	done = v_genlabel();
	emit_load_memory(V_U, __R8, __RBP, -1, __ADDR(list) + offset_field);
	v_emit_branch(V_X64_GE, V_U, __R8, limit, 0, overflow);
	emit_load_memory(V_P, __R9, __RBP, -1, __ADDR(list) + 16);
	emit_load_memory(type, target, __R9, __R8, 0);
	emit_group_immediate(0, __R8, stride, 0);
	emit_store_memory(V_U, __R8, __RBP, -1, __ADDR(list) + offset_field);
	v_emit_byte(0xe9);
	v_dmark((v_code *)v_ip, done);
	v_emit_u32(0);
	v_label(overflow);
	emit_load_memory(V_P, __R9, __RBP, -1, __ADDR(list) + 8);
	emit_load_memory(type, target, __R9, -1, 0);
	emit_group_immediate(0, __R9, 8, 1);
	emit_store_memory(V_P, __R9, __RBP, -1, __ADDR(list) + 8);
	v_label(done);
	if (!__NVIRT(result)) store_register(result, type, target);
	return result;
}

void v_va_end(v_reg_t list)
{
	require_variadic_list(list);
}

union v_fp v_end(int *nbytes)
{
	union v_fp result;
	procedure_epilogue();
	if (labels_end() < 0)
		v_fatal("unresolved x86-64 vcode label");
	if (v_code_finalize(code_start) < 0)
		v_fatal("cannot make x86-64 code buffer executable");
	if (nbytes) *nbytes = (int)(v_ip - (uintptr_t)code_start);
	result.v = (v_vptr)code_start;
	return result;
}

void v_push_init(struct v_cstate *state)
{
	state->next_args = 0;
}

void push_arg_reg(struct v_cstate *state, int type, v_reg_t reg)
{
	int n = state->next_args;
	v_reg_t snapshot;
	if (n >= MAX_ARGS) v_fatal("too many x86-64 call arguments");
	/* Icode translation reuses its scratch registers while collecting a call.
	   Snapshot each value now so later arguments cannot overwrite it before
	   the actual SysV argument moves are emitted. */
	snapshot = v_local(type);
	v_emit_move(type, snapshot, reg);
	state->type[n] = CSTATE_REGISTER;
	state->value_type[n] = type;
	state->val[n] = (uintptr_t)(intptr_t)snapshot;
	state->next_args++;
}

void push_arg_imm(struct v_cstate *state, int type, uintptr_t immediate)
{
	int n = state->next_args;
	if (n >= MAX_ARGS) v_fatal("too many x86-64 call arguments");
	state->type[n] = CSTATE_IMMEDIATE;
	state->value_type[n] = type;
	state->val[n] = immediate;
	state->next_args++;
}

void v_arg_push(struct v_cstate *state, int type, v_reg_type reg)
{
	push_arg_reg(state, type, reg);
}

void v_arg_pushb(struct v_cstate *state, v_reg_type address,
                 const struct v_aggregate *aggregate)
{
	int n = state->next_args;
	v_reg_t snapshot;
	enum v_abi_class classes[2];
	if (n >= MAX_ARGS) v_fatal("too many x86-64 call arguments");
	v_aggregate_classify(aggregate, classes);
	snapshot = v_local(V_P);
	v_emit_move(V_P, snapshot, address);
	state->type[n] = CSTATE_AGGREGATE;
	state->value_type[n] = V_B;
	state->val[n] = (uintptr_t)(intptr_t)snapshot;
	state->aggregates[n] = *aggregate;
	state->next_args++;
}

void v_push_argf(struct v_cstate *state, v_reg_type reg)
{
	push_arg_reg(state, V_F, reg);
}

void v_push_argd(struct v_cstate *state, v_reg_type reg)
{
	push_arg_reg(state, V_D, reg);
}

static void emit_call_argument(struct v_cstate *state, int argument, int target)
{
	int type = state->value_type[argument];
	if (state->type[argument] == CSTATE_IMMEDIATE) {
		if (type_is_float(type)) {
			double value;
			uint64_t bits = (uint64_t)state->val[argument];
			if (type == V_F) {
				uint32_t narrow = (uint32_t)bits;
				float f;
				memcpy(&f, &narrow, sizeof f);
				value = f;
			} else {
				memcpy(&value, &bits, sizeof value);
			}
			v_emit_set_float(type, target, value);
		} else
			emit_mov_imm(target, state->val[argument], type_is_64(type));
	}
	else {
		v_reg_t source_reg = (v_reg_t)(intptr_t)state->val[argument];
		int source = load_register(source_reg, type, target);
		if (type_is_float(type))
			emit_sse_rr(type, 0x10, target, source);
		else
			emit_mov_rr(target, source, type_is_64(type));
	}
}

static v_reg_t ccall_internal(struct v_cstate *state, uintptr_t address,
                              v_reg_t indirect_register, int indirect,
                              int result_type,
                              const struct v_aggregate *aggregate_result)
{
	int saved[3];
	int saved_count = 0;
	int argument_targets[MAX_ARGS][2];
	int argument_on_stack[MAX_ARGS];
	unsigned int argument_stack_offset[MAX_ARGS];
	int integer_argument = 0;
	int floating_argument = 0;
	unsigned int stack_bytes = 0;
	v_reg_t floating_saves[14];
	int floating_save_registers[14];
	int floating_save_count = 0;
	unsigned int padding;
	int i;
	v_reg_t result;
	v_reg_t aggregate_result_block = 0;
	v_reg_t indirect_snapshot = 0;
	enum v_abi_class result_classes[2];
	int aggregate_words = 0;
	int aggregate_memory_result = 0;

	if (indirect) {
		indirect_snapshot = v_local(V_P);
		v_emit_move(V_P, indirect_snapshot, indirect_register);
	}
	if (aggregate_result) {
		aggregate_words = v_aggregate_classify(aggregate_result, result_classes);
		aggregate_memory_result = result_classes[0] == V_ABI_MEMORY;
		aggregate_result_block = v_localb(aggregate_result->size);
		if (!v_getreg(&result, V_P, V_VAR)) result = v_local(V_P);
		if (aggregate_memory_result) integer_argument = 1;
	} else if (!v_getreg(&result, result_type == V_V ? V_UL : result_type, V_VAR))
		result = v_local(result_type == V_V ? V_UL : result_type);
	for (i = 0; i < state->next_args; i++) {
		if (state->type[i] == CSTATE_AGGREGATE) {
			enum v_abi_class classes[2];
			int words = v_aggregate_classify(&state->aggregates[i], classes);
			int needed_integer = 0, needed_sse = 0, word;
			for (word = 0; word < words && classes[0] != V_ABI_MEMORY; word++) {
				needed_integer += classes[word] == V_ABI_INTEGER;
				needed_sse += classes[word] == V_ABI_SSE;
			}
			argument_on_stack[i] = classes[0] == V_ABI_MEMORY ||
			                       integer_argument + needed_integer > 6 ||
			                       floating_argument + needed_sse > 8;
			if (!argument_on_stack[i]) {
				for (word = 0; word < words; word++)
					argument_targets[i][word] = classes[word] == V_ABI_SSE ?
						floating_argument_registers[floating_argument++] :
						argument_registers[integer_argument++];
			}
		} else {
			int floating = type_is_float(state->value_type[i]);
			if (floating && floating_argument < 8) {
				argument_targets[i][0] = floating_argument_registers[floating_argument++];
				argument_on_stack[i] = 0;
			} else if (!floating && integer_argument < 6) {
				argument_targets[i][0] = argument_registers[integer_argument++];
				argument_on_stack[i] = 0;
			} else
				argument_on_stack[i] = 1;
		}
		if (argument_on_stack[i]) {
			unsigned int alignment = state->type[i] == CSTATE_AGGREGATE &&
			                         state->aggregates[i].alignment > 8 ? 16 : 8;
			unsigned int size = state->type[i] == CSTATE_AGGREGATE ?
			                    align_up(state->aggregates[i].size, 8) : 8;
			stack_bytes = align_up(stack_bytes, alignment);
			argument_stack_offset[i] = stack_bytes;
			stack_bytes += size;
		}
	}
	for (i = 0; i < (int)(sizeof temporary_registers / sizeof temporary_registers[0]); i++) {
		int reg = temporary_registers[i];
		if (register_class[reg] == V_VAR && reg != result) {
			emit_push(reg);
			saved[saved_count++] = reg;
		}
	}
	for (i = __XMM0; i < FP_SCRATCH0; i++) {
		if (register_class[i] == V_VAR && i != result) {
			floating_saves[floating_save_count] = v_local(V_D);
			floating_save_registers[floating_save_count] = i;
			v_emit_move(V_D, floating_saves[floating_save_count], i);
			floating_save_count++;
		}
	}
	padding = (16 - (((unsigned int)saved_count * 8 + stack_bytes) & 15)) & 15;
	if (stack_bytes + padding) emit_stack_adjust(1, stack_bytes + padding);
	for (i = 0; i < state->next_args; i++) {
		if (!argument_on_stack[i]) continue;
		if (state->type[i] == CSTATE_AGGREGATE) {
			int source = load_register((v_reg_t)(intptr_t)state->val[i], V_P, __R8);
			emit_lea_memory(__R11, __RSP, (int32_t)argument_stack_offset[i]);
			emit_copy_bytes(__R11, source, state->aggregates[i].size);
		} else if (type_is_float(state->value_type[i])) {
			emit_call_argument(state, i, FP_SCRATCH0);
			emit_store_memory(state->value_type[i], FP_SCRATCH0, __RSP, -1,
			                  (int32_t)argument_stack_offset[i]);
		} else {
			emit_call_argument(state, i, __R8);
			emit_store_memory(V_UL, __R8, __RSP, -1,
			                  (int32_t)argument_stack_offset[i]);
		}
	}
	for (i = 0; i < state->next_args; i++)
		if (!argument_on_stack[i]) {
			if (state->type[i] == CSTATE_AGGREGATE) {
				enum v_abi_class classes[2];
				int words = v_aggregate_classify(&state->aggregates[i], classes);
				int source = load_register((v_reg_t)(intptr_t)state->val[i], V_P, __R11);
				int word;
				for (word = 0; word < words; word++) {
					unsigned int offset = (unsigned int)word * 8;
					emit_load_aggregate_word(argument_targets[i][word], source,
					                         (int32_t)offset,
					                         state->aggregates[i].size - offset,
					                         classes[word]);
				}
			} else
				emit_call_argument(state, i, argument_targets[i][0]);
		}
	if (aggregate_memory_result) {
		emit_lea_memory(__RDI, __RBP, __ADDR(aggregate_result_block));
	}
	/* Required by variadic callees; harmless for fixed-prototype calls. */
	emit_mov_imm(__RAX, (uint64_t)floating_argument, 0);
	if (indirect)
		v_emit_call_reg(indirect_snapshot);
	else
		v_emit_call_imm(address);
	if (stack_bytes || padding)
		emit_stack_adjust(0, stack_bytes + padding);
	if (aggregate_result && !aggregate_memory_result) {
		int integer_result = 0, sse_result = 0, word;
		for (word = 0; word < aggregate_words; word++) {
			unsigned int offset = (unsigned int)word * 8;
			int source = result_classes[word] == V_ABI_SSE ?
			             floating_argument_registers[sse_result++] :
			             (integer_result++ == 0 ? __RAX : __RDX);
		emit_store_aggregate_word(__RBP, source,
			                          __ADDR(aggregate_result_block) + (int32_t)offset,
			                          aggregate_result->size - offset,
			                          result_classes[word]);
		}
	} else if (aggregate_result) {
		/* The callee wrote directly into the local result block. */
	} else if (type_is_float(result_type))
		store_register(result, result_type, __XMM0);
	else
		emit_mov_rr(__R8, __RAX, 1);
	for (i = saved_count - 1; i >= 0; i--)
		emit_pop(saved[i]);
	for (i = floating_save_count - 1; i >= 0; i--)
		v_emit_move(V_D, floating_save_registers[i], floating_saves[i]);
	if (!aggregate_result && !type_is_float(result_type))
		store_register(result, V_UL, __R8);
	if (aggregate_result) {
		emit_lea_memory(__R10, __RBP, __ADDR(aggregate_result_block));
		store_register(result, V_P, __R10);
	}
	return result;
}

v_reg_t ccall(struct v_cstate *state, v_vptr ptr)
{
	uintptr_t address = 0;
	memcpy(&address, &ptr, sizeof ptr);
	return ccall_internal(state, address, 0, 0, V_UL, NULL);
}

void v_ccallv(struct v_cstate *state, v_vptr ptr)
{
	v_reg_t result = ccall(state, ptr);
	v_putreg(result, V_UL);
}

#define DEFINE_CCALL(name, pointer_type, result_type) \
v_reg_t name(struct v_cstate *state, pointer_type ptr) \
{ uintptr_t address = 0; memcpy(&address, &ptr, sizeof ptr); \
  return ccall_internal(state, address, 0, 0, result_type, NULL); }
DEFINE_CCALL(v_ccalli, v_iptr, V_I)
DEFINE_CCALL(v_ccallu, v_uptr, V_U)
DEFINE_CCALL(v_ccallp, v_pptr, V_P)
DEFINE_CCALL(v_ccalll, v_lptr, V_L)
DEFINE_CCALL(v_ccallul, v_ulptr, V_UL)
DEFINE_CCALL(v_ccallf, v_fptr, V_F)
DEFINE_CCALL(v_ccalld, v_dptr, V_D)

v_reg_t v_ccallb(struct v_cstate *state, v_vptr ptr,
                 const struct v_aggregate *result)
{
	uintptr_t address = 0;
	memcpy(&address, &ptr, sizeof ptr);
	return ccall_internal(state, address, 0, 0, V_B, result);
}

void v_rccallv(struct v_cstate *state, v_reg_type reg)
{
	v_reg_t result = ccall_internal(state, 0, reg, 1, V_V, NULL);
	v_putreg(result, V_UL);
}
v_reg_t v_rccalli(struct v_cstate *state, v_reg_type reg)
{ return ccall_internal(state, 0, reg, 1, V_I, NULL); }
v_reg_t v_rccallu(struct v_cstate *state, v_reg_type reg)
{ return ccall_internal(state, 0, reg, 1, V_U, NULL); }
v_reg_t v_rccallp(struct v_cstate *state, v_reg_type reg)
{ return ccall_internal(state, 0, reg, 1, V_P, NULL); }
v_reg_t v_rccalll(struct v_cstate *state, v_reg_type reg)
{ return ccall_internal(state, 0, reg, 1, V_L, NULL); }
v_reg_t v_rccallul(struct v_cstate *state, v_reg_type reg)
{ return ccall_internal(state, 0, reg, 1, V_UL, NULL); }
v_reg_t v_rccallf(struct v_cstate *state, v_reg_type reg)
{ return ccall_internal(state, 0, reg, 1, V_F, NULL); }
v_reg_t v_rccalld(struct v_cstate *state, v_reg_type reg)
{ return ccall_internal(state, 0, reg, 1, V_D, NULL); }

v_reg_t v_rccallb(struct v_cstate *state, v_reg_type reg,
                  const struct v_aggregate *result)
{
	return ccall_internal(state, 0, reg, 1, V_B, result);
}

static v_reg_t scall_address(uintptr_t address, char *format, va_list arguments,
                             int result_type)
{
	struct v_cstate state;
	char *p = format;
	v_push_init(&state);
	while (*p) {
		int immediate;
		int type;
		if (*p++ != '%') v_fatal("invalid x86-64 call format");
		immediate = isupper((unsigned char)*p);
		if (immediate) {
			char first = (char)tolower((unsigned char)*p);
			if (first == 'u' && p[1] == 'l') { type = V_UL; p += 2; }
			else {
				p++;
				switch (first) {
				case 'i': type = V_I; break;
				case 'u': type = V_U; break;
				case 'l': type = V_L; break;
				case 'p': type = V_P; break;
				case 's': type = V_S; break;
				case 'c': type = V_C; break;
				case 'f': type = V_F; break;
				case 'd': type = V_D; break;
				default: v_fatal("unsupported x86-64 call argument format");
				}
			}
		} else if (*p == 'u' && p[1] == 'l') { type = V_UL; p += 2; }
		else {
			switch (*p++) {
			case 'i': type = V_I; break;
			case 'u': type = V_U; break;
			case 'l': type = V_L; break;
			case 'p': type = V_P; break;
			case 's': type = V_S; break;
			case 'c': type = V_C; break;
			case 'f': type = V_F; break;
			case 'd': type = V_D; break;
			default: v_fatal("unsupported x86-64 call argument format");
			}
		}
		if (immediate) {
			uintptr_t value = type_is_64(type) ? va_arg(arguments, uintptr_t) :
			                  (uintptr_t)va_arg(arguments, unsigned int);
			push_arg_imm(&state, type, value);
		} else {
			push_arg_reg(&state, type, va_arg(arguments, int));
		}
	}
	return ccall_internal(&state, address, 0, 0, result_type, NULL);
}

v_reg_t scall(v_vptr ptr, char *format, va_list arguments)
{
	uintptr_t address = 0;
	memcpy(&address, &ptr, sizeof ptr);
	return scall_address(address, format, arguments, V_UL);
}

void v_scallv(v_vptr ptr, char *format, ...)
{
	va_list ap;
	v_reg_t result;
	va_start(ap, format);
	result = scall(ptr, format, ap);
	va_end(ap);
	v_putreg(result, V_UL);
}

#define DEFINE_SCALL(name, pointer_type, result_type) \
v_reg_t name(pointer_type ptr, char *format, ...) \
{ va_list ap; v_reg_t result; uintptr_t address = 0; \
  memcpy(&address, &ptr, sizeof ptr); va_start(ap, format); \
  result = scall_address(address, format, ap, result_type); \
  va_end(ap); return result; }
DEFINE_SCALL(v_scalli, v_iptr, V_I)
DEFINE_SCALL(v_scallu, v_uptr, V_U)
DEFINE_SCALL(v_scallp, v_pptr, V_P)
DEFINE_SCALL(v_scalll, v_lptr, V_L)
DEFINE_SCALL(v_scallul, v_ulptr, V_UL)
DEFINE_SCALL(v_scallf, v_fptr, V_F)
DEFINE_SCALL(v_scalld, v_dptr, V_D)

void v_param_alloc(int argno, int type, v_reg_type *reg)
{
	if (argno < 0 || argno >= MAX_ARGS || dynamic_params[argno])
		v_fatal("invalid x86-64 dynamic parameter allocation");
	dynamic_params[argno] = reg;
	dynamic_param_types[argno] = type;
	if (argno >= dynamic_param_count) dynamic_param_count = argno + 1;
}

void v_clambda(char *name, int leaf, v_code *code, int nbytes)
{
	char format[MAX_ARGS * 3 + 1];
	char *p = format;
	v_reg_t args[MAX_ARGS];
	int i;
	for (i = 0; i < dynamic_param_count; i++) {
		if (!dynamic_params[i]) v_fatal("incomplete x86-64 dynamic parameters");
		*p++ = '%';
		switch (dynamic_param_types[i]) {
		case V_C: *p++ = 'c'; break;
		case V_UC: *p++ = 'u'; *p++ = 'c'; break;
		case V_S: *p++ = 's'; break;
		case V_US: *p++ = 'u'; *p++ = 's'; break;
		case V_I: *p++ = 'i'; break;
		case V_U: *p++ = 'u'; break;
		case V_L: *p++ = 'l'; break;
		case V_UL: *p++ = 'u'; *p++ = 'l'; break;
		case V_P: *p++ = 'p'; break;
		case V_F: *p++ = 'f'; break;
		case V_D: *p++ = 'd'; break;
		default: v_fatal("unsupported x86-64 dynamic parameter type");
		}
	}
	*p = 0;
	v_lambda(name, format, args, leaf, code, nbytes);
	for (i = 0; i < dynamic_param_count; i++) {
		v_reg_type slot = v_local(dynamic_param_types[i]);
		v_emit_store(dynamic_param_types[i], args[i], v_lp, slot, 0);
		*dynamic_params[i] = slot;
		v_putreg(args[i], dynamic_param_types[i]);
		dynamic_params[i] = 0;
	}
	dynamic_param_count = 0;
}

void v_reset_closure_state(void)
{
	memset(dynamic_params, 0, sizeof dynamic_params);
	dynamic_param_count = 0;
}

void v_bpo_on(void) {}
void v_bpo_off(void) {}

v_code *v_swapcp(v_code *address)
{
	v_code *old = (v_code *)v_ip;
	v_ip = (uintptr_t)address;
	return old;
}

void v_begin_incremental(v_code *code, int nbytes)
{
	if (v_code_make_writable(code) < 0)
		v_fatal("cannot make incremental x86-64 code writable");
	code_start = code;
	code_limit = code + nbytes;
	v_ip = (uintptr_t)code;
	labels_start();
}

int v_end_incremental(void)
{
	if (labels_end() < 0) v_fatal("unresolved incremental x86-64 label");
	if (v_code_finalize(code_start) < 0)
		v_fatal("cannot make incremental x86-64 code executable");
	return (int)(v_ip - (uintptr_t)code_start);
}

int v_getra(v_reg_t *reg)
{
	if (!v_getreg(reg, V_P, V_TEMP)) return 0;
	emit_load_memory(V_P, *reg, __RBP, -1, 8);
	return 1;
}

void v_cmuli(v_reg_type d, v_reg_type s, int v) { v_mulii(d, s, v); }
void v_cmulu(v_reg_type d, v_reg_type s, unsigned int v) { v_mului(d, s, v); }

void v_dump(void *start)
{
	v_code *p;
	v_code *first = start ? (v_code *)start : code_start;
	for (p = first; p < (v_code *)v_ip; p++)
		printf("%02x%s", *p, ((p - first + 1) % 16) ? " " : "\n");
	if (((v_code *)v_ip - first) % 16) putchar('\n');
}
