CC ?= gcc
AR ?= ar
RANLIB ?= ranlib
CPPFLAGS ?= -I.
CFLAGS ?= -O2 -g -std=gnu99 -Wall -Wextra -Werror
LDFLAGS ?=

all: vcode.lib.a

vcode.lib.a: vcode.o code-memory.o
	$(AR) rcs $@ $^
	$(RANLIB) $@

vcode.o: vcode.c vcode.h x86_64-codegen.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ vcode.c

code-memory.o: code-memory.c vcode.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ code-memory.c

clean:
	rm -f *.o *.a

.PHONY: all clean
