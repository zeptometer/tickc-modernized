# Public release gate

This directory tracks GitHub issue
[#9](https://github.com/zeptometer/tcc/issues/9). It records what may enter a
fresh public repository, the accepted license boundaries, and the remaining
mechanical release checks.

## Current conclusion

The technical cleanup is moderate because the retired SPARC, MIPS,
SimpleScalar, BFD, and i386 trees are already absent.

The project owner approved free, source-only publication for educational and
research use under the inherited Tick-C and lcc conditions. This decision
relies on their express source redistribution language, retains all notices,
accepts the lcc sale and profit-making restrictions, and does not seek a
commercial-use grant or external legal opinion. Executables, packages, CI
artifacts, release archives containing generated code, and OCI images are not
approved for publication.

## Completed cleanup

- The original lcc notice remains at `doc/original-lcc/COPYRIGHT.lcc`.
- The unused Bison 1.21 output `src/lburg/gram.tab.c` was removed. The build
  generates `gram.c` from `src/lburg/gram.y` with the selected host Bison.
- `doc/tutorial.ps` was removed because redistribution permission for that
  copy had not been established.
- Historical `src/copt/config.cache`, `config.log`, and `config.status` build
  residue was removed.
- Previously retired backends removed the bundled BFD, SimpleScalar,
  prebuilt-object, and target-binary concerns recorded during Phase 0.
- `LICENSE.md` and `NOTICE` now make the mixed licensing status visible at the
  repository root.
- Standalone modernization files are licensed under MIT. Modernization changes
  to historical files and architecture files derived from historical
  implementations inherit the Tick-C and lcc conditions, avoiding a separate
  license layer within those files.
- The old X11R5-derived `config/install-sh` was replaced with the copy from
  GNU Automake 1.18.1. The replacement embeds the X Consortium permission
  notice and marks the Free Software Foundation changes as public domain.
- The small tests in `tst/phase1/paper/` remain with their adaptation sources
  disclosed. The project owner explicitly accepted retaining them rather than
  requiring an independent rewrite for the source release candidate.
- The complete Tick-C and lcc terms are available under stable custom license
  references in `LICENSES/`. The original lcc notice remains in its historical
  archive location as required.
- `REUSE.toml` assigns machine-readable SPDX expressions to every tracked
  covered file. REUSE 6.2.0 reports 397/397 files with copyright and license
  information and full compliance with version 3.3 of the specification.

## Open release checks

1. The sanitized source snapshot and its fresh public Git history must pass the
   exclusion and reachability checks in `distribution-policy.md`.

Recheck the metadata after changing tracked files with:

```sh
reuse lint
```

The grouped inventory is in `source-inventory.md`. Distribution boundaries
and the fresh-history procedure are in `distribution-policy.md`.
