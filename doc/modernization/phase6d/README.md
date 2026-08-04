# Phase 6D: SysV AMD64 aggregate ABI

Phase 6D was delivered as two independently testable changes. Phase 6D1
implemented and validated the machine ABI in direct vcode. Phase 6D2 connects
Tick-C aggregate parameters, arguments, calls, copies, and returns to that API
through both icode and the `-V` direct-vcode backend.

The reproducible local and CI entry point remains:

```sh
env/linux-x86_64/scripts/run-tests.sh
```

## Aggregate descriptions and classification

`struct v_aggregate` describes the size, alignment, and flattened scalar
leaves of a C structure, union, or array. `v_aggregate_classify` applies the
SysV AMD64 rules used by this backend:

- values larger than two eightbytes use the `MEMORY` class;
- an unaligned scalar field forces `MEMORY`;
- integer and pointer leaves use `INTEGER`;
- `float` and `double` leaves use `SSE`;
- `INTEGER` wins when union members or fields overlap an `SSE` eightbyte.

Nested structures, arrays, and unions are represented by flattening their
leaves while retaining byte offsets. The descriptor limit is 16 leaves.
Vectors, `long double`, and non-scalar leaves are not accepted.

## Calls, parameters, and returns

`v_arg_pushb` adds an aggregate value to a generated call. Register-class
eightbytes use the independent GPR and XMM banks. If either bank cannot hold
the complete value, allocation is rolled back and the whole aggregate is
placed on the stack. Stack values honor 8- or 16-byte alignment.

`v_lambda_aggregate` accepts scalar and aggregate parameters and reconstructs
register-class values in local storage. Memory-class parameters are copied
from their incoming stack slots. `v_retb`, `v_ccallb`, and `v_rccallb` cover
all four small-return register combinations and large returns through the
hidden structure-result pointer.

The Tick-C frontend emits the same descriptor for `param`, `compile`, `ARGB`,
`CALLB`, `ASGNB`, and aggregate returns. Icode keeps an aggregate as one typed
instruction operand instead of decomposing it into unrelated scalar
arguments; the x86-64 translator therefore retains the all-or-nothing GPR/XMM
allocation rule. `ASGNB` uses an explicit bounded block copy, and aggregate
locals are always backed by allocated stack storage before their addresses are
materialized.

## Coverage

The GCC interoperability test covers both call directions for:

- two-integer, two-SSE, and mixed INTEGER/SSE values;
- nested structures and an overlapping union;
- packed unaligned and 24-byte memory-class values;
- hidden-pointer returns;
- GPR and XMM exhaustion with all-or-nothing aggregate rollback;
- a 16-byte-aligned stack aggregate following an earlier stack argument.

The direct-vcode API is the ABI source of truth. Tick-C quotations exercise
that implementation through both icode and `-V`.
The Tick-C matrix covers GCC-to-generated identity calls and generated-to-GCC
round trips for INTEGER, SSE, both mixed register orders, nested structures,
overlapping unions, odd-sized values, and 24-byte memory-class values. It also
covers GPR and XMM exhaustion rollback and hidden structure-result pointers.
The lower-level direct-vcode regression additionally retains the packed and
16-byte-alignment boundary cases that the historical Tick-C parser cannot
spell as source-level alignment attributes.
