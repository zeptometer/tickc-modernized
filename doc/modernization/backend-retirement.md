# Legacy backend retirement

The modernization tree now retains only native x86-64/Linux. Historical
backends were useful as staged migration baselines, but keeping them buildable
would couple later ABI and quality work to platforms that are no longer
project targets.

The cleanup removes:

- the SPARC/SunOS and SPARC/Solaris static, icode, vcode, headers, and BPO data;
- the MIPS/Ultrix and MIPS/IRIX static, icode, vcode, headers, and BPO data;
- the SimpleScalar/SStrix environment, target variants, CI workflow, and
  toolchain smoke test;
- the unused generic vcode tree, including its dormant Alpha implementation
  and prebuilt architecture-specific objects;
- unsupported x86/DOS and i386/OpenBSD remnants; and
- obsolete target-specific benchmark data and build harnesses.

The paper-derived semantic tests remain and run through the x86-64 compiler.
Original lcc documentation remains under `doc/original-lcc/` as provenance;
it is not a statement of currently supported targets.

Git history is the archival boundary. Commit `edcd4d1` is the merged Phase 1
SStrix reference, and the Phase 2 i386 branch tip is `1867f65`. The final
merged i386 baseline is `d6d848d`; see `i386-retirement.md`. No generated
binaries or source hashes are copied into a second archive in the working
tree.
