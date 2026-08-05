# Tick-C 1.0 beta 9 modernization

This repository modernizes Tick-C 1.0 beta 9 for native x86-64 Linux. Tick-C
extends C with runtime code generation: a program can construct and compile
typed code quotations, then call the generated code in the same process.

Tick-C is based on lcc and includes a compiler driver, a C frontend and static
C backend, two dynamic compilation paths, runtime support, and a native
x86-64 machine-code emitter. The modernization preserves the historical
language while making the compiler buildable and testable with a current GCC,
glibc, and SysV AMD64 environment.

## Current scope

- Native x86-64 Linux with the LP64 data model is the only maintained target.
- Dynamic compilation is available through the icode and direct-vcode paths.
- The SysV AMD64 implementation covers integer, pointer, scalar floating-point,
  variadic scalar, and aggregate calling conventions.
- Generated code uses writable-to-executable memory transitions rather than
  writable and executable mappings.
- The reproducible test matrix provides debug, optimized, sanitizer, and
  Valgrind profiles on a pinned Debian container.

Historical i386, SPARC, MIPS, SStrix, Alpha, DOS, and OpenBSD targets are not
part of the maintained source tree. Their retirement points are documented in
`doc/modernization/` and remain available in Git history.

## Building and installing

See [`INSTALL`](INSTALL) for the supported native x86-64 Linux build and
installation procedure. The installed compiler is named `tcc`.

## Testing

Docker is required for the reproducible build and regression suite. Run the
debug profile from the repository root with:

```sh
env/linux-x86_64/scripts/run-tests.sh debug
```

The other profiles are `optimized`, `sanitizer`, and `valgrind`. See
[`env/linux-x86_64/README.md`](env/linux-x86_64/README.md) for their behavior
and coverage.

## Documentation and provenance

The modernization history and current implementation boundaries are recorded
under [`doc/modernization/`](doc/modernization/). The README shipped with
Tick-C 1.0 beta 9 is preserved as [`README.orig`](README.orig), and the
original lcc material is under [`doc/original-lcc/`](doc/original-lcc/).

## License and distribution

This repository does not have a single project-wide open-source license. The
historical Tick-C and lcc conditions restrict sale and some profit-making uses;
the repository must not be described as uniformly MIT-licensed, GPL-licensed,
or OSI-approved open source. Binary releases, packages, CI artifacts, and
container images are outside the approved publication scope.

Read [`LICENSE.md`](LICENSE.md) and [`NOTICE`](NOTICE) before copying,
redistributing, or using the source. File-specific notices and the assignments
in [`REUSE.toml`](REUSE.toml) remain controlling.
