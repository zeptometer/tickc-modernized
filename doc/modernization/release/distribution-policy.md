# Distribution policy and public-history procedure

## Distribution decisions

| Form | Current decision | Reason |
|---|---|---|
| Private development repository | Continue | Provides a controlled place to complete the audit |
| Source-only public repository | Approved after metadata and fresh-history checks | Free educational/research source distribution inherits the Tick-C and lcc terms and includes all notices |
| Source release archive | Approved only from the reviewed public source tree | Must contain the same source and notices; no generated binaries or generated release payloads |
| Executables and shared/static libraries | Do not publish | Outside the project owner's source-only decision |
| GitHub Actions artifacts | Do not publish | Artifacts may contain binaries or generated parser/code-generator outputs and are outside the source-only decision |
| OCI/container images | Do not publish | Images combine project binaries with third-party operating-system packages and are outside the source-only decision |

CI may build and test source in the repository, but workflows must not upload
binary or generated release artifacts.

## Fresh public history

The public repository must be created from one reviewed source snapshot. It
must not be made by changing the visibility of the private repository or by
pushing its existing refs, because deleted files remain reachable in Git
history.

Before publication:

1. Export the approved tracked tree without `.git` or build output.
2. Verify that the explicit exclusion list is absent. At minimum it includes
   `doc/tutorial.ps`, `src/lburg/gram.tab.c`, `src/vcode/cachectl.h`,
   `src/vcode/sparc-dis/tmp`, historical target object files and executables,
   and `src/copt/config.cache`, `config.log`, and `config.status`.
3. Scan the snapshot for binaries, archives, credentials, absolute developer
   paths, and notices that prohibit copying or redistribution.
4. Initialize a new Git repository, make a single provenance-preserving import
   commit, and verify all reachable objects and refs before the first push.
5. Create a new public GitHub repository and push only the reviewed branch.
6. Keep a private record mapping the public import to the reviewed private
   commit and audit result.

The exact publication command and destination are deliberately omitted until
the release gate is approved. Creating a repository or changing visibility is
an external publication action, not an audit step.
