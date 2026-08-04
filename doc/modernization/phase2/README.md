# Phase 2: archived legacy i386 baseline

Phase 2 connected the unfinished `vcode-x86`, `i386IIR`, `i386VIR`, and
`i386linuxIR` paths in a pinned Debian Woody i386 environment. That historical
environment was removed after Phase 4 reproduced its supported behavior on
modern Linux. The final merged tree containing the maintained i386 backend,
fixtures, and modern harness is `d6d848d`.

The archived fixtures cover:

- exact bytes for representative integer, pointer, branch, and call code;
- stable operation signatures emitted by `i386IIR` and `i386VIR` for every
  paper-derived characterization program;
- static i386 arithmetic, pointers, loads, stores, control flow, and calls;
- the supported native Tick-C quote, closure, and `compile()` integration.

At `d6d848d`, the modern harness in `env/linux-i386` ran these checks with W^X
code memory, a non-executable stack, PIE, current GCC and glibc, and both IA32
and `qemu-i386` execution. The harness and fixtures are no longer copied in
the active tree; Git history is the archive.

## Retained backend limitations

- Floating-point vcode is unsupported.
- The i386 register interface supports at most six word-sized incoming
  arguments.
- Native `i386linuxIR` compilation rejects the outer `dot-product` and
  `dynamic-sum` programs; native `-V` also rejects `plus1`. Their dynamic-code
  semantics are covered through the C backend.
- The binary peephole optimizer has no i386/Linux rule set, so the static
  assembly test uses `-nsp`.
