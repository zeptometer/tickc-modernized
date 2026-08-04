# Phase 0 provisional license inventory

This document groups the licensing and provenance risks found in the
sanitized baseline. It is deliberately conservative and must not be used as a
final legal conclusion.

## Project-level notices

The official MIT PDOS distribution page states that the tcc source and
supporting documentation may be used, copied, modified, and distributed when
the MIT copyright and permission notices are retained. It then incorporates
the lcc notice because tcc is heavily based on lcc.

The exact upstream text and source URL are preserved in
`upstream-license-notice.md`.

The archive itself contains `doc/original-lcc/COPYRIGHT.lcc`. That notice
allows copying, modification, and redistribution with attribution, but also
restricts sale and some profit-making use. The project therefore must not be
described as uniformly MIT-licensed, GPL-licensed, or OSI-approved open source.

## Special groups

| Provisional class | Principal paths | Required release action |
|---|---|---|
| Project default tcc+lcc terms | most original project source | retain both notices and verify third-party provenance |
| GPL-1.0-or-later | `src/lburg/gram.tab.c` | regenerate with modern Bison's output exception or omit the generated file |
| GPL-2.0-or-later | `src/vcode/{bfd.h,ansidecl.h}` | remove, replace, or isolate before distributing a combined program |
| LGPL-2.0-or-later | archived `src/vcode/obstack.h` and selected i386 headers | omitted from the maintained tree; retain notices if restored from history |
| SimpleScalar noncommercial terms | `src/vcode/ss.def` | retain notice; review source, binary, and container distribution separately |
| CMU permissive notice | archived `src/vcode-x86/dis.c` | omitted after the final i386 baseline at `d6d848d`; retain notice if restored |
| Unknown system-header provenance | most other `include/` files | establish upstream/version/license or replace with maintained system headers |
| Unknown prebuilt binary provenance | target `.o` files and `src/vcode/m4/m4` | do not publish; rebuild from reviewed source or remove |
| Unknown document permission | `doc/tutorial.ps` | exclude from public history unless redistribution permission is verified |

## Interpretation rules

- A file sharing a repository with GPL code is not automatically relicensed.
- A linked or otherwise combined program may be subject to GPL terms even when
  its source files retain different notices.
- The lcc sale restriction may be incompatible with GPL requirements for a
  combined distributed work.
- New portability code must carry its own explicit license without claiming to
  relicense inherited code.
- Source archives, executable releases, CI artifacts, and container images are
  separate distribution decisions.

Final cleanup and publication remain tracked by GitHub issue #9.

This file preserves the Phase 0 finding as historical evidence. Several paths
listed above were retired during later phases. The current-tree audit and
release decisions are maintained in `../release/`.
