# Phase 5: native x86-64/Linux

Phase 5 added an LP64 target, an x86-64 vcode implementation, and SysV AMD64
integer calling conventions. Its merge commit `d6d848d` is also the final
revision that retained i386 as a tested regression configuration.

The reproducible local and CI entry point is:

```sh
env/linux-x86_64/scripts/run-tests.sh
```

It builds a digest-pinned Debian Trixie amd64 image, mounts the repository
read-only, and performs all configuration, compilation, and test work below
`/tmp` in the container.

## Implemented scope

- LP64 compiler metrics: 32-bit `int`, 64-bit `long`, and 64-bit pointers;
- pointer-sized closure, runtime-constant, hash, immediate, and offset storage;
- a separate `src/vcode-x86_64` backend with REX prefixes, 64-bit operands,
  R8-R15, ModRM/SIB, stack-relative and RIP-relative addressing;
- direct and indirect branches and calls, including a register-indirect
  fallback when a target is outside the signed rel32 range;
- SysV AMD64 integer and pointer arguments and returns, six argument
  registers, stack arguments, caller/callee-saved registers, and 16-byte call
  alignment;
- both the icode-to-vcode path and the direct `-V` vcode path;
- C output plus the system GCC for static compilation;
- RW-to-RX code mappings with no writable/executable state.

The frontend's arithmetic conversion and simplification rules now preserve
`long` and `unsigned long` instead of silently selecting the historical
32-bit `int` representation. Dynamic pointer returns use an unsigned-long
carrier on LP64 rather than truncating through `unsigned int`.

## Verification

The x86-64 harness checks:

- a clean compiler build with current GCC and glibc;
- ELF64 PIE output and a non-executable `GNU_STACK`;
- exact encoder samples for REX, extended registers, ModRM/SIB,
  stack-relative, and RIP-relative forms;
- generated-code calls in both directions with C, including eight integer
  arguments so that register and stack argument paths are both exercised;
- the Phase 1 paper-derived programs;
- values above 32 bits in generated `long` arithmetic and pointer round trips;
- both icode and direct-vcode dynamic backends;
- RW and RX mapping states and address variation across ASLR-enabled
  processes.

At the Phase 5 archive boundary, the complete Phase 4 i386 Docker suite also
passed. Later Phase 6 work maintains only native x86-64/Linux; the i386
backend and suite remain recoverable from `d6d848d`.

## Deferred work

The Phase 5 ABI is intentionally limited to integers and pointers. XMM
floating-point arguments and returns, variadic calls, and structure
classification are Phase 6 work. The static x86-64 path currently uses the C
backend and system GCC; a direct assembly backend is optional and remains
deferred until there is a demonstrated need.
