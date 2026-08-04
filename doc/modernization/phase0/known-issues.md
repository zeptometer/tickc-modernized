# Known baseline issues

These findings describe the sanitized baseline. They are not fixes and do not
assert that every item is a defect in the original supported environment.

## Build and environment

- The supported host/target combinations are historical SPARC, MIPS,
  SimpleScalar/SStrix, and an unfinished i386 port. There is no x86-64 target.
- The top-level build expects GNU make, old Autoconf behavior, GCC extensions,
  Bison, Flex, Perl, a standalone GNU preprocessor, target assemblers/linkers,
  and architecture-specific libraries.
- Several Makefiles and scripts contain absolute paths under `/home/tickc`,
  `/home/butt/maxp`, `/usr/uns`, and `/usr/local`.
- `src/copt/config.cache`, `config.log`, and `config.status` are committed
  residue from a historical configure run.
- Empty installation directories described by `README` (`bin`, `build`, and
  `lib`) are naturally absent from the tar archive and Git.

## Sanitization effects

- `src/vcode/cachectl.h` was removed because its embedded notice prohibited
  reproduction and disclosure. The top-level non-x86 header list and the
  Alpha, MIPS, and SimpleScalar config headers still reference it. Those builds
  are intentionally expected to fail until a clean replacement is added.
- `src/vcode/sparc-dis/tmp` was removed because its notice prohibited source
  and binary redistribution. No active build rule referencing that path was
  found.

## Generated and binary material

- `src/lburg/gram.tab.c` was produced by Bison 1.21 and embeds a GPL parser
  skeleton without the exception used by modern Bison.
- The tree contains 11 target object files whose source or exact build command
  is not present, plus a prebuilt target `m4` executable.
- Regenerating old Autoconf, Bison, Flex, lburg, vcode, or icode outputs with
  current tools is expected to produce large mechanical differences.

## Test assets

- There is no single test command that works from a clean source checkout.
- `src/icode/test/Makefile`, `src/vcode/tests/Makefile`, and benchmark scripts
  contain developer-specific paths.
- `tst/tcc2/Makefile.sample` names `binary2.tc`, but the archive contains
  `binary.tc`.
- Some expected-result sets are intentionally sparse or use names different
  from their source (`mshl2.tc` uses `mshl.0.out` and `umshl.0.out`). These
  cases require characterization before repair.

## Provenance and licensing

- The official distribution page supplies an MIT copyright/permission notice,
  but the archive has no standalone top-level file containing that notice.
- `COPYRIGHT.lcc` imposes attribution and sale restrictions and must remain.
- GPL, GNU Library GPL/LGPL, SimpleScalar noncommercial, CMU, and project
  default terms coexist in the tree.
- No complete GPL or GNU Library GPL license text is included even though
  multiple files refer to one.
- Many vendored OS/libc headers have no embedded license notice.
- `doc/tutorial.ps`, prebuilt objects, and the prebuilt `m4` need provenance
  or replacement before a public release.

The audit summary is in `license-inventory.md`. Final decisions remain open
until the release-gate issue is complete.

This document is a Phase 0 snapshot, not a description of the current tree.
Resolved and retired items are reconciled in `../release/README.md`.
