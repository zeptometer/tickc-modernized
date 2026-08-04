# Phase 1: archived SimpleScalar/SStrix baseline

Phase 1 established the first reproducible semantic baseline on
`sslittle-na-sstrix` under SimpleScalar 2.0. It restored the historical
compiler path and introduced the paper-derived tests that are still used by
the native x86-64 regression suite.

Once Phase 2 reproduced those semantic results through both `i386IIR` and
`i386VIR`, SStrix ceased to be an active project target. Its Docker
environment, backend, toolchain smoke test, and CI workflow were removed before
the modern compiler and Linux work began.

The complete merged Phase 1 implementation is preserved at Git commit
`edcd4d1`. That commit contains the pinned artifact URLs and hashes, build
patches, local test entry point, CI definition, known limitations, and the
SimpleScalar non-commercial license warning. The current tree intentionally
does not download, build, or redistribute those artifacts.

The paper-derived tests remain in `tst/phase1/paper/`. Their observable stdout
and exit status are checked by `env/linux-x86_64/scripts/run-tests.sh` through
the maintained native target; simulator counters and generated instruction
counts are not regression oracles.
