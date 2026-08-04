# Paper-derived baseline tests

These small programs exercise the language features introduced by the original
tcc papers without redistributing the papers themselves.

- `hello.tc` is the compile-and-call example from section 2.1 of the tcc
  tutorial and section 2.1 of the 1999 paper.
- `composition.tc` combines two `void cspec` values as shown in tutorial
  section 2.2.3.
- `plus1.tc` uses a `vspec` parameter as shown in tutorial section 2.2.2.
- `matrix-scale.tc` adapts the specialized matrix multiplication from Figure 1
  of the 1999 paper and tutorial section 2.3.2.
- `dynamic-sum.tc` adapts the dynamically sized parameter and argument lists
  from Figure 2 of the 1999 paper.
- `dot-product.tc` adapts the sparse dot-product generator from tutorial
  section 3.1.1.

Typography and argument order have been normalized to the syntax accepted by
tcc 1.0b9. Each program adds only the surrounding declarations and output
needed to make the fragment a standalone regression test.

## Release decision

These tests remain in the source release candidate with the adaptation
provenance above preserved. The project owner explicitly accepted retention
rather than an independent rewrite. This decision does not describe the
examples as independently authored and does not remove or replace their cited
origins.

Each test records its exact stdout. Successful compilation, a zero process exit
status, and stdout equality form the regression oracle. Instruction and memory
reference counts are not golden data because they vary with code-buffer
placement and include work outside the generated function.

The dynamic-sum baseline uses four arguments. Phase 5 adds separate x86-64 ABI
coverage for both register and stack arguments.
