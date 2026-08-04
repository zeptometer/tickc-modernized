# Phase 6A: integer and pointer hardening

Phase 6A turns the integer and pointer subset introduced in Phase 5 into a
boundary-tested compatibility contract before additional SysV AMD64 ABI
classes are added.

The reproducible local and CI entry point remains:

```sh
env/linux-x86_64/scripts/run-tests.sh
```

## Covered behavior

- signed and unsigned `char`, `short`, `int`, and `long` conversion boundaries;
- minimum and maximum values, zero, minus one, and 64-bit immediates;
- pointer load, store, and round trips without truncation;
- zero through six register arguments and seven or eight arguments crossing
  the SysV AMD64 stack boundary;
- deep expressions, branches, loops, and register-indirect calls;
- repeated `compile()` / `decompile()` cycles through both icode and direct
  vcode;
- deterministic rejection of invalid code-memory and `decompile()` inputs;
- comparison with ordinary C functions compiled by the system GCC.

The direct-vcode test isolates encoder and ABI behavior. The Tick-C test is
built and run once through the icode backend and once with `-V`, and compares
generated results with C reference functions in the same executable.
Those reference functions live in a separate ordinary C translation unit and
are compiled directly by the system GCC, so they do not share the Tick-C
frontend or either dynamic backend.

## Implementation corrections

The boundary tests exposed assumptions inherited from the ILP32 frontend and
vcode implementation:

- conversion templates did not distinguish `int` from `long`, or signed from
  unsigned `char` and `short`;
- narrowing a register value did not canonicalize its upper bits before a
  later widening conversion;
- bitwise operations always used an `unsigned int` carrier, even when the
  usual arithmetic conversion selected `long` or `unsigned long`.

Conversion emission now carries source and destination width and signedness
through the frontend, icode, and direct-vcode templates. The x86-64 emitter
uses explicit byte/word sign or zero extension and preserves 64-bit width for
LP64 bitwise expressions.

## Supported boundary

Phase 6A covers integer and pointer values under the LP64 data model and the
integer classes of the SysV AMD64 ABI. Floating-point/XMM arguments and
returns, variadic calls, and aggregate classification remain intentionally
unsupported and belong to Phases 6B through 6D.

Calling `decompile(NULL)` remains a documented no-op for the historical
benchmarking interface. Passing any other pointer that was not returned by
`compile()`, including a pointer that has already been decompiled, terminates
with a diagnostic rather than silently accepting the invalid lifetime.
