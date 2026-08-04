# Phase 0 baseline

This directory records the source, test, generated-file, and licensing state
before portability changes begin. It is the evidence bundle for GitHub issue
[#2](https://github.com/zeptometer/tcc/issues/2).

## Upstream source

| Field | Value |
|---|---|
| Project | Tick-C compiler 1.0 beta 9 |
| Official index | <https://pdos.csail.mit.edu/archive/tickc/source.html> |
| Archive URL | <https://pdos.csail.mit.edu/archive/tickc/dist/tcc-1.0b9.tgz> |
| Archive size | 1,083,333 bytes |
| Archive SHA-256 | `1afce0775edd3fd254e4fa8e8d30dc366cad4256a8d038a54bc0ee66f838bf7d` |
| Gzip timestamp | 1998-07-09 23:01:23 (stored without a time zone) |
| Regular files in archive | 591 |

The official archive is not committed. It contains two files with explicit
copying or redistribution restrictions. The sanitized baseline is:

| Field | Value |
|---|---|
| Git commit | `adf50eeda391f0fce35691ac8f861a8a29a834d7` |
| Git tree | `0b642511b496ccc7c588130b5daf850dc2bca22c` |
| Regular files | 589 |

The official archive was compared with the sanitized baseline. Their file
contents and executable modes matched after removing only:

- `src/vcode/cachectl.h`
- `src/vcode/sparc-dis/tmp`

The archive also carries empty `bin`, `build`, `lib`, and platform `tickc`
directories. Git does not represent empty directories, so they were omitted
from the comparison.

## Inventory files

- `license-inventory.md`: provisional license-risk groups and required release
  actions. It is an audit summary, not a final license conclusion.
- `upstream-license-notice.md`: the distribution notice published by MIT PDOS
  and its relationship to the bundled lcc notice.
- `generated-files.md`: shipped and build-time generated artifacts.
- `test-inventory.md`: test assets and their current execution status.
- `known-issues.md`: defects, environment dependencies, and provenance gaps
  observed without changing the implementation.
- `change-policy.md`: commit boundaries for later phases.

## Phase boundary

No compiler, runtime, backend, test input, or expected output is changed by
this inventory. Repairs discovered here must be made in later, separately
reviewable commits.
