# Generated-file inventory

This inventory distinguishes artifacts shipped in the 1998 archive from files
that the historical build creates. Identification is based on embedded
notices, make rules, file formats, and dependency comments.

## Generated artifacts shipped upstream

| Path or pattern | Producer or origin | Current status |
|---|---|---|
| `configure` | Autoconf 2.12 from `config/configure.in` | Shipped; regeneration with Autoconf 2.73 is not assumed equivalent |
| `src/{bpo,copt,icode,rcc,sup}/configure` | Autoconf 2.12 | Shipped |
| `src/rts/configure` | Autoconf 2.10 | Shipped |
| `src/{lburg,tcc}/configure` | Autoconf 2.7 | Shipped |
| `config/config.guess`, `config/config.sub` | GNU config helpers from the mid-1990s | Shipped with an embedded GPL exception |
| `src/lburg/gram.tab.c` | GNU Bison 1.21 from `src/lburg/gram.y` | Shipped; embeds a GPL skeleton without the later Bison exception |
| `src/vcode/bfd.h` | BFD 2.5 header generator | Shipped; original generator inputs are absent |
| `src/copt/config.cache`, `config.log`, `config.status` | A historical configure run | Shipped build residue; do not use as a modern baseline |
| `src/vcode/alpha-dis/*.o` | Alpha/OSF toolchain | 2 prebuilt objects; source is absent |
| `src/vcode/sparc-dis/*.o` | SunOS/SPARC toolchain | 9 prebuilt objects; source is absent |
| `src/vcode/m4/m4` | Historical target build of GNU m4 | Prebuilt 307,384-byte executable; not runnable on the current host |
| `src/vcode/TAGS` | Etags | Shipped editor index |
| `tst/tcc0/*.{1bk,2}` and `tst/tcc{1,2}/ref/*` | Historical compiler/test runs | Shipped expected-output data |

The prebuilt objects and executable are evidence only. They must not be treated
as reproducible outputs or executed on the host.

## Files generated during a build

| Output | Inputs and producer |
|---|---|
| top-level and component `Makefile` files | `Makefile.in` plus the corresponding `configure` script |
| `src/vcode/bpp/bin.tab.c`, `bin.tab.h` | `bin.y` via Bison |
| `src/vcode/bpp/lex.yy.c` | `bin.l` via Flex |
| `src/vcode/binary.h` | target `*-bin` through `bpp`, or `ss.def` through `gen-ss.pl` for SimpleScalar |
| `src/vcode/vcode-macros.h` | target `.md`, `binary.h`, and `spec.pl` |
| `src/vcode/mult.h` | `booth-gen.c` via the built `booth-gen` utility |
| `src/icode/{macros-gen.h,op2class.h,opcode.h,pp.h,op2class.c}` | `op.def` through scripts in `src/icode/gen/` |
| `src/icode/icode-arch.{c,h}` | target-specific links into `src/icode/config/` |
| `src/rcc/{symbolic,null,mips,sparc,x86,linux,icode,vcode,vcodex86}.c` | matching `.md` files through `lburg` |
| `src/rcc/environ.h` | installed runtime/icode headers through `cpp` and `envgen.pl` |
| `src/bpo` intermediate `.c`, `.bog`, and target objects | `tog`, `bog`, target rule files, and the host/target compiler pair |
| `tst/tcc1/x*.c` | `tst/tcc1/src/test.c` through `tst/gen-tests.pl` |
| `src/vcode/tests/{call-test.c,regress.c}` | `call-gen` and `test-gen` |

Generated outputs must be written to a build directory where possible. A later
phase must record the generator version and command before replacing any
shipped generated file.

This is the Phase 0 inventory. `src/lburg/gram.tab.c`, the prebuilt objects and
executables, and other retired-backend artifacts are absent from the current
release candidate. See `../release/source-inventory.md` for the current tree.
