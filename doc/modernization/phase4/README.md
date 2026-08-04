# Phase 4: archived modern Linux i386 baseline

Phase 4 ran the compiler built by the modern-GCC work from Phase 3 on a
modern x86-64 Linux userland. The generated ISA and ABI remain i386/ILP32;
AMD64 code generation, LP64 types, and the SysV AMD64 ABI remain Phase 5 work.

The final local and CI entry point, preserved at `d6d848d`, was:

```sh
env/linux-i386/scripts/run-tests.sh
```

That environment, the backend, and its target-specific tests were removed
before Phase 6. This document records the verified historical scope; it is not
an active build instruction. Restore `d6d848d` to reproduce it.

The harness uses a digest-pinned Debian Trixie `linux/amd64` image with GCC
multilib. It proves that a 32-bit Tick-C compiler can be built and run on a
current x86-64 system. Every generated test program is run once through the
host kernel's IA32 compatibility mode and once through `qemu-i386`.

## Supported configuration

The modern Linux configuration is the only supported build path. It:

- obtains GCC's internal include directory through
  `gcc -print-file-name=include`, without parsing diagnostic output;
- builds the compiler and runtime against the system multilib glibc headers
  instead of the bundled historical libc header snapshot;
- invokes `gcc -m32` as the link driver instead of naming `ld`, `crt*.o`,
  `libgcc`, the dynamic loader, and libc directly;
- emits ELF32 PIE executables with a non-executable stack;
- passes explicit build, host, and target triplets, with the generated target
  fixed at `i386-pc-linux-gnu`.

The historical system-header snapshot is no longer distributed in the active
tree. The compiler-specific `stdarg.real.h` remains under `include/tickc/i386`
because it is a Tick-C language requirement rather than a libc snapshot.

## Executable memory

Runtime-generated code no longer uses `malloc`. `v_code_alloc` creates a
private anonymous RW mapping, `v_end` changes that mapping to RX before it can
be called, reuse changes it back to RW, and `v_code_free` releases it with
`munmap`. No transition requests write and execute permission together.

The Phase 4 test reads `/proc/self/maps` to verify the RW and RX states,
executes generated code after each finalization, and checks that independent
processes receive different mapping addresses while kernel ASLR is enabled.
The complete vcode regression also runs with this allocator and without an
executable stack.

## Behavioral and security checks

The harness verifies all of the following:

- the host process environment is x86-64, while `tcc` and its outputs are
  ELF32 PIE executables using `/lib/ld-linux.so.2`;
- `GNU_STACK` is not executable for the compiler, memory test, vcode
  regression, and generated programs;
- the installed target include directory does not contain the bundled
  `stdio.h` or `stdlib.h` snapshots;
- the broad vcode regression and exact instruction encoding baseline pass at
  both `-O0` and `-O2`;
- every paper-derived program retains its expected `i386IIR` and `i386VIR`
  operation signature;
- every Phase 1 paper-derived program passes through both `i386IIR` and
  `i386VIR`, under both IA32 and QEMU execution;
- the static backend, C backend, and GCC reference agree on the static i386
  fixture, and the supported native `i386linuxIR` integration tests retain
  their expected output.

The C-backed IIR and VIR executables are also required to contain no dynamic
`TEXTREL` entry.

## Known limitations

The 1998 rcc frontend does not understand all syntax and compiler built-ins in
current glibc headers, including `long long`, `restrict`, and GCC's modern
`va_list` representation. The compiler and runtime themselves build against
modern glibc, and generated programs link and run against it, but a Tick-C
source file cannot yet include arbitrary current libc headers directly. Such
programs must use declarations for the interfaces they call, as the historical
examples do. A future libc declaration facade or frontend-language extension
must preserve the i386 ABI; preprocessor substitutions that invent incompatible
types are deliberately not used here.

The retained native `i386linuxIR` backend predates PIE and emits absolute
references from its text section. Modern GNU ld can produce a PIE from this
assembly, but records `DT_TEXTREL` and warns while linking. The Phase 4 harness
still executes these binaries through both routes to preserve the native
backend baseline; the C-backed IIR and VIR paths, the compiler itself, and all
runtime-generated code do not depend on text relocations. Removing this final
static-backend limitation requires PIC code generation rather than a Linux
userland compatibility change.
