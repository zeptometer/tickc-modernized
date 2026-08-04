# Test inventory

No historical compiler test is runnable end-to-end on the current host before
the reference toolchain is restored. The source and expected data are present,
and all 21 Perl scripts plus all 9 shipped configure scripts pass syntax checks
with the current host tools.

## Test suites

| Suite | Assets | Intended coverage | Current classification | Blocker or next action |
|---|---:|---|---|---|
| `tst/tcc0` | 18 C inputs, historical runtime and compile outputs | inherited lcc frontend/static backend regression | Repair required | historical `rcc` build and target runner are absent; README contains a stale expected-output path |
| `tst/tcc1` | generator source, 4 regression plans, 15 reference files | Tick-C language, diagnostics, icode/vcode modes | Environment missing | generate `x1` through `x17`; tests 7, 9, and 10 are intentionally absent from the plans but this must be confirmed |
| `tst/tcc2` | 18 `.tc` programs, 4 regression plans, 11 references | dynamic compilation benchmarks and applications | Environment missing | requires built tcc and target execution; benchmark scripts contain absolute historical paths |
| `src/vcode/tests` | 17 source/generator/reference files | low-level vcode calls, registers, branches, and generated procedures | Environment missing | requires generated headers, target vcode library, SimpleScalar compiler, `libbfd`, and `libopcodes` |
| `src/vcode-x86` | `regres.c` and `make test` | experimental i386 vcode backend | Repair required | backend is incomplete and assumes a 32-bit executable environment |
| `src/icode/test` | 16 files including allocator/IR tests | icode lists, register allocation, translation, and heap behavior | Environment missing | Makefile hard-codes a historical SPARC installation |
| `src/lburg/tst` | 3 machine-description inputs | lburg parser/code-generator smoke coverage | Harness missing | expected generated output and an automated comparison command are not recorded |
| `src/bpo/tst1..3` | optimizer rules, C inputs, and assembly fixtures | text/binary peephole optimizer generators | Harness repair required | several ad-hoc Makefiles and target-specific inputs; no single top-level test entry |

## Baseline test ordering

Later phases should enable tests in this order so that failures remain local:

1. Perl and shell syntax checks.
2. `lburg` and generator smoke tests.
3. SimpleScalar toolchain hello-world test.
4. vcode unit tests.
5. icode unit tests and translation.
6. runtime and compiler-driver tests.
7. `tst/tcc1` language characterization.
8. `tst/tcc2` dynamic compilation and benchmarks.
9. inherited `tst/tcc0` frontend/static-backend regression.

## Expected-result policy

Preserve historical expected files unchanged until the corresponding test can
run in the Phase 1 reference environment. Normalize addresses, timestamps,
temporary paths, and tool version banners in a separate test-harness commit.
Do not update an expected result merely to make a modern run pass.
