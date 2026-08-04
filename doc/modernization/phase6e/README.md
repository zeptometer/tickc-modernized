# Phase 6E: quality configurations

Phase 6E adds reproducible optimized, memory-safety, and diagnostic checks for
the maintained Linux x86-64 target. It does not change the supported Tick-C
semantics.

## Configurations

The modern-Linux runner accepts one profile:

| Profile | Host flags | Assertions | Intended use |
|---|---|---|---|
| `debug` | `-O0 -g3` | enabled | Detect assertion and optimization-sensitive differences |
| `optimized` | `-O2 -g -DNDEBUG` | disabled | Exercise the production-style optimizer configuration |
| `sanitizer` | `-O1 -g3 -fsanitize=address,undefined` | enabled | Detect invalid memory access and undefined behavior |
| `valgrind` | `-O0 -g3` under Memcheck | enabled | Detect invalid access, definite leaks, and uninitialized reads |

Run a profile from the repository root, for example:

```sh
env/linux-x86_64/scripts/run-tests.sh sanitizer
```

Each profile rebuilds into a separate temporary directory and executes the
same semantic and ABI suite. This prevents differences in test selection from
being mistaken for differences caused by optimization or instrumentation.

## CI policy

The `debug`, `optimized`, and `sanitizer` profiles are required pull-request
checks. The slower `valgrind` profile runs on the weekly schedule and through
manual workflow dispatch. All profiles use the same pinned Debian Trixie
container definition.

## Coverage boundary

The checkers cover GCC-built host components: the frontend, generated icode
translator, IR and register allocator libraries, x86-64 encoder, runtime, and
test harnesses. ASan and UBSan do not add checks to x86-64 instructions emitted
at runtime by vcode. Generated code is instead checked through deterministic
results, GCC interoperability, ABI boundary tests, W^X, and ASLR checks.

Valgrind observes execution of generated code but cannot attach source-level
instrumentation to it. Reports originating in host-side allocation or encoder
code retain source locations because the Valgrind profile includes debug
information.

## Failure diagnostics

Compiler and linker output is captured per test and printed when the command
fails. A generated-program failure also prints ELF headers and the first 240
lines of its disassembly. Full logs and disassembly remain in
`/tmp/tcc-modern-linux-PROFILE` until the container exits.

No sanitizer suppressions or Valgrind suppression files are maintained. Any
accepted exclusion must be documented here and reduced to the smallest
affected component or test.
