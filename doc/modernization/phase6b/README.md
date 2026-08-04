# Phase 6B: scalar floating point and the SysV SSE ABI

Phase 6B adds the scalar SSE portion of the SysV AMD64 ABI to the native
x86-64 vcode backend. The reproducible local and CI entry point remains:

```sh
env/linux-x86_64/scripts/run-tests.sh
```

## Implemented behavior

- XMM0 through XMM15 have a register identity distinct from the general
  purpose registers.
- XMM14 and XMM15 are reserved for emitter scratch values. They are not
  exposed to either dynamic register allocator, so lowering a load or store
  cannot silently destroy a live floating-point temporary.
- Scalar `movss`/`movsd` loads, stores, and moves are supported.
- Scalar addition, subtraction, multiplication, division, negation, and
  ordered comparisons are supported for `float` and `double`.
- Conversions between `float` and `double`, and conversions between signed
  `int`/`long` and floating-point values, use the corresponding scalar SSE
  instructions.
- Float and double arguments use XMM0 through XMM7 independently of the six
  integer argument registers. Excess arguments use ordered eight-byte stack
  slots.
- Float and double results use XMM0.
- Generated calls preserve live floating-point variables around calls because
  the SysV ABI defines no callee-saved XMM registers.
- Both direct and register-indirect calls support floating-point results.
- The icode parameter limit now matches vcode's 32-argument limit.

Unsigned integer/floating-point conversions outside the signed range remain
explicitly unsupported by the low-level emitter. Variadic ABI details,
aggregate classification, vectors, `long double`, and non-scalar SSE/AVX are
outside Phase 6B and remain assigned to later phases.

## Tests

The direct-vcode test checks exact bytes for representative scalar SSE
encodings. Its semantic tests cover:

- mixed integer, `float`, and `double` arguments;
- ten floating-point arguments, crossing the XMM register boundary;
- scalar arithmetic, conversion, comparison, load, and store operations;
- calls from ordinary GCC C into generated code;
- calls from generated code into an independently compiled GCC C function.

The Tick-C test exercises the same ABI boundary through both the icode backend
and the direct-vcode (`-V`) backend. It compares arithmetic and call results
with an ordinary C reference object compiled directly by the system GCC, and
also covers floating-point comparison, conversion, and memory round trips.

The complete Debian Trixie amd64 Docker suite retains all Phase 1, Phase 5,
and Phase 6A regressions in addition to these Phase 6B tests.
