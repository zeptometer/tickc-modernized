#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#include "vcode.h"

#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif

struct code_mapping {
	unsigned char *base;
	size_t size;
	int writable;
	struct code_mapping *next;
};

static struct code_mapping *code_mappings;

static struct code_mapping *find_mapping(void *address,
                                         struct code_mapping **previous)
{
	struct code_mapping *mapping;
	struct code_mapping *prior = 0;
	uintptr_t pointer = (uintptr_t)address;

	for (mapping = code_mappings; mapping; mapping = mapping->next) {
		uintptr_t base = (uintptr_t)mapping->base;
		if (pointer >= base && pointer < base + mapping->size) {
			if (previous)
				*previous = prior;
			return mapping;
		}
		prior = mapping;
	}
	return 0;
}

void *v_code_alloc(size_t requested_size)
{
	struct code_mapping *mapping;
	long system_page_size;
	size_t page_size;
	size_t mapped_size;
	void *base;

	if (!requested_size)
		return 0;
	system_page_size = sysconf(_SC_PAGESIZE);
	if (system_page_size <= 0)
		return 0;
	page_size = (size_t)system_page_size;
	if (requested_size > SIZE_MAX - (page_size - 1))
		return 0;
	mapped_size = (requested_size + page_size - 1) & ~(page_size - 1);
	base = mmap(0, mapped_size, PROT_READ | PROT_WRITE,
	            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (base == MAP_FAILED)
		return 0;

	mapping = malloc(sizeof *mapping);
	if (!mapping) {
		munmap(base, mapped_size);
		return 0;
	}
	mapping->base = base;
	mapping->size = mapped_size;
	mapping->writable = 1;
	mapping->next = code_mappings;
	code_mappings = mapping;
	return base;
}

int v_code_make_writable(void *address)
{
	struct code_mapping *mapping = find_mapping(address, 0);

	if (!mapping)
		return 0;
	if (mapping->writable)
		return 1;
	if (mprotect(mapping->base, mapping->size, PROT_READ | PROT_WRITE) < 0)
		return -1;
	mapping->writable = 1;
	return 1;
}

int v_code_finalize(void *address)
{
	struct code_mapping *mapping = find_mapping(address, 0);

	if (!mapping)
		return 0;
	if (!mapping->writable)
		return 1;
	__builtin___clear_cache((char *)mapping->base,
	                        (char *)mapping->base + mapping->size);
	if (mprotect(mapping->base, mapping->size, PROT_READ | PROT_EXEC) < 0)
		return -1;
	mapping->writable = 0;
	return 1;
}

int v_code_free(void *address)
{
	struct code_mapping *mapping;
	struct code_mapping *previous;

	mapping = find_mapping(address, &previous);
	if (!mapping) {
		errno = EINVAL;
		return -1;
	}
	if (munmap(mapping->base, mapping->size) < 0)
		return -1;
	if (previous)
		previous->next = mapping->next;
	else
		code_mappings = mapping->next;
	free(mapping);
	return 0;
}
