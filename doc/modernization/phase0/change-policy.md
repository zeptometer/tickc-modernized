# Change and commit policy

Portability work must keep independent causes independently reviewable.

## Required commit boundaries

Use separate commits for each of the following categories:

1. Inventory or documentation only.
2. Restoration of a test or generated source without behavior changes.
3. Reference environment and dependency pinning.
4. i386 backend behavior.
5. C language or compiler compatibility.
6. libc, binutils, or Linux API compatibility.
7. Executable-memory and W^X behavior.
8. x86-64 instruction encoding.
9. SysV AMD64 ABI behavior.
10. License cleanup or replacement of third-party material.

Do not combine a baseline update with the implementation change that caused
it. First record the old observable result, then make the implementation
change, then update an expected result only when the difference is intentional
and explained in the commit message.

## Generated files

Changes to a generator and its checked-in output may share a commit when the
output is a purely mechanical consequence and the exact generator command is
recorded. Toolchain upgrades belong in a separate commit from semantic source
changes.

## Commit verification

Before committing:

- inspect `git diff --check`;
- run the narrowest relevant test layer;
- record unavailable tools instead of silently skipping them;
- confirm that no downloaded archive, generated binary, or restricted file is
  staged;
- cite the relevant Phase issue in the commit or pull request.
