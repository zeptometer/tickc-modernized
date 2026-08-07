# Native static backend cleanup

The maintained x86-64/Linux compiler has one static compilation path:

```text
Tick-C source -> rcc C emitter -> host C compiler -> assembler
```

Quoted dynamic code continues through the icode or direct-vcode backend and is
emitted as machine code at run time. These dynamic backends still use the
lcc-derived DAG construction, instruction selection, and register-allocation
infrastructure in `src/rcc/dag.c` and `src/rcc/gen.c`.

## Removed selection paths

The driver previously retained the original multi-backend bookkeeping even
though `cbackendp` was initialized true and no maintained native static backend
could make it false. The following paths were therefore unreachable in the
supported configuration and have been removed:

- discovery and selection of a C backend through the `cbackends` table;
- preprocessing without `__C2C__` for a native static backend;
- bypassing the second C compiler for native assembly output;
- `-C` as a backend-selection operation;
- installation of the native-backend-only `stdarg.real.h` parsing facade;
- unused rcc compile-time path macros left by the native second pass.

`-C` remains accepted as a compatibility no-op because the historical test
plans and user scripts may still pass it. `x86_64-linux` is the canonical rcc
target. `c-x86_64` remains a compatibility alias for direct rcc users.

The native-only second compilation pass for code-generator functions was also
removed from `rcc`. The C-to-C path already emits and finalizes those functions
as C. The `symbolic` and `null` targets remain useful frontend diagnostics, but
they are not supported compilation targets and no longer attempt that native
code-generator-function pass.

## Deliberately retained shared code

The following code is not obsolete native-static-backend code:

- `src/rcc/c-backend.c` and the `cbuf` machinery implement static C output;
- `src/rcc/dag.c` and `src/rcc/gen.c` serve the icode and vcode backends;
- `src/copt` optimizes generated icode/vcode call sequences;
- the parser, type checker, trees, and symbol tables implement the maintained
  Tick-C frontend;
- the `Interface` callbacks used by `icode.md` and `vcodex86.md` remain part of
  dynamic code generation.

Further separation of the static C emitter from the dynamic backend interface
would be a larger refactoring and is intentionally outside this cleanup.
