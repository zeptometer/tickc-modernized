# Modern Linux x86-64 environment

This container builds and tests the x86-64/LP64 port on Debian Trixie with
the distribution GCC and glibc. It covers the x86-64 vcode encoder and ABI,
the paper-derived programs, LP64 dynamic code, PIE, ASLR, and W^X mappings.

Run the assertion-enabled, unoptimized check with:

```sh
env/linux-x86_64/scripts/run-tests.sh debug
```

The same semantic and ABI suite is available in four reproducible profiles:

```sh
env/linux-x86_64/scripts/run-tests.sh optimized
env/linux-x86_64/scripts/run-tests.sh sanitizer
env/linux-x86_64/scripts/run-tests.sh valgrind
```

`debug` forces `-O0` and keeps assertions enabled. `optimized` forces `-O2`
and `NDEBUG`. `sanitizer` uses `-O1`, ASan, and UBSan with immediate failure.
These flags are applied after the historical per-component flags so the named
profile is authoritative. The required pull-request matrix runs these first
three profiles.

`valgrind` uses the debug build and runs compiler invocations and generated
test programs under Memcheck. It rejects definite leaks, invalid accesses, and
uses of uninitialized values. Because it is substantially slower, GitHub
Actions runs it only on the weekly schedule or by manual dispatch.

ASan, UBSan, and Valgrind instrument or observe the host compiler, runtime,
IR, allocator, encoder, and test harness. Dynamically emitted x86-64 code does
not contain compiler-inserted sanitizer checks. Its externally visible ABI
behavior remains covered by the tests, while the buffers and metadata used to
construct it are checked by the instrumented host code.

When a generated test program fails, the runner prints ELF metadata and the
first part of an `objdump` disassembly. Compiler output is retained in the
profile-specific directory `/tmp/tcc-modern-linux-PROFILE` for the lifetime of
the container and is printed immediately when a build or compile step fails.

The repository is mounted read-only. All configuration, build products, and
test output are written below `/tmp` in the container.
