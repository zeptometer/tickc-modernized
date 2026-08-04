# i386 target retirement

The modernization project used i386/ILP32 as an intermediate target to
separate historical compiler behavior, modern GCC compatibility, modern Linux
integration, and the later x86-64 port. It is not an intended deployment
target.

Merge commit `d6d848d` is the archive boundary. At that revision, PR #17 had
passed both the complete modern i386 suite and the new native x86-64 suite.
The i386 tests covered the static backend, both dynamic backends, exact IR and
encoder baselines, current GCC/glibc, PIE/NX, W^X, native IA32 execution, and
qemu-i386 execution.

Before Phase 6, the active tree removed:

- `src/vcode-x86` and the native i386 static backend;
- i386 system, icode, BPO, and target-header configuration;
- the `linux-i386` Docker environment and GitHub Actions workflow;
- i386-only IR, encoding, static-backend, and memory-mapping fixtures; and
- shared build switches, compatibility aliases, and `-m32` paths.

The paper-derived semantic tests remain and run on native x86-64. Phase 2-4
documentation remains as a record of the migration and verified scope. To
reproduce or inspect the final i386 implementation, check out `d6d848d` or an
earlier phase branch; this retirement does not delete remote archival branches
or rewrite Git history.

The maintained target is now x86-64/Linux with the LP64 data model and the
SysV AMD64 ABI. Unsupported target triplets fail during the build instead of
selecting an implicit fallback backend.
