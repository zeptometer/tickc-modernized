# Phase 3: archived GCC migration ladder

Phase 3 moved the i386/ILP32 host-side sources through GCC 4.9, GCC 8.3, and
GCC 14.2 without changing the target ISA or ABI. After Phase 4 established GCC
14 on modern Linux as the supported environment, the intermediate Jessie and
Buster containers were removed. The complete ladder remains available at
merge commit `737df26`.

The migration fixed obsolete token-pasting and cast-lvalue extensions, missing
standard declarations, incompatible libc declarations, default argument
promotions, writable-string assumptions, and modern generator compatibility.
It also stopped parsing GCC diagnostic text to discover build tools.

The final `modern-linux-i386` job at `d6d848d` retained these compiler checks:

- project sources use GNU89 with implicit declarations and unsafe
  integer/pointer conversions treated as errors;
- the vcode regression and exact encoding baseline run at `-O0` and `-O2`;
- the complete compiler and runtime build with GCC 14 and current binutils;
- the Phase 1 and 2 semantic and IR baselines run on current glibc.

The i386 job and backend were retired before Phase 6. The temporary
executable-stack bridge used during the ladder no longer exists, and the
maintained x86-64 runtime uses an RW-to-RX allocator.
