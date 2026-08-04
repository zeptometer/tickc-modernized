# Phase 6C: variadic SysV AMD64 calls

Phase 6C implements the scalar variadic subset of the SysV AMD64 ABI. The
reproducible local and CI entry point remains:

```sh
env/linux-x86_64/scripts/run-tests.sh
```

## Variadic callers

Generated calls use the independent general-purpose and XMM argument banks
introduced in Phase 6B. Immediately before every call, the emitter sets `AL`
to the number of XMM registers used by the arguments. This is the metadata a
SysV variadic callee needs to locate floating-point arguments.

The C frontend applies the default argument promotions before it emits an
argument node. In particular, a variadic `float` argument becomes `double`,
and narrow integer arguments become `int` or `unsigned int`. The Phase 6C
Tick-C test verifies these promotions through both icode and direct vcode by
calling independently compiled GCC variadic functions. Integer and promoted
floating-point lists both cross their register-to-stack boundaries.

## Generated variadic callees

Direct vcode exposes the following callee-side interface:

```c
v_lambda_variadic(name, fixed_format, fixed_args, leaf, code, size);
list = v_localb(24);
v_va_start(list);
value = v_va_arg(list, V_L); /* or V_I, V_U, V_UL, V_P, V_D */
v_va_end(list);
```

The variadic prologue creates the 176-byte SysV register-save area, records
the fixed arguments' `gp_offset` and `fp_offset`, and positions
`overflow_arg_area` after any fixed stack arguments. `v_va_arg` selects the
register-save area or overflow stack at run time and advances the appropriate
cursor. Live integer and floating-point results continue to use the normal
vcode allocator.

Direct tests call generated variadic functions from ordinary GCC C. They
cover integer and double arguments, GP and XMM exhaustion, fixed arguments
that already cross the stack boundary, and generated calls back into a GCC
variadic function.

## Supported boundary

The callee API supports promoted scalar integer, pointer, and `double`
arguments. Requesting `float`, narrow integers, aggregates, vectors, or
`long double` is rejected rather than decoded with the wrong ABI class.

Tick-C quotations do not currently have syntax for declaring that the
generated function itself has an ellipsis. Callee-side consumption is
therefore exposed through direct vcode, while variadic calls are supported
through both Tick-C dynamic backends. Adding an ellipsis-bearing Tick-C
closure type would be a language/API extension rather than an ABI encoder
change; until such syntax exists, `va_start`/`va_arg` inside a quotation is
not treated as a supported Tick-C construct.

Aggregate variadic arguments remain coupled to Phase 6D's aggregate
classification work. All Phase 1 and Phase 5 through 6B regressions remain in
the same Docker suite.
