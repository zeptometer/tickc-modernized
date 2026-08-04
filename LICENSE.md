# Licensing and distribution status

This repository does not have a single project-wide open-source license.

The historical Tick-C sources are distributed with the MIT permission notice
recorded in `LICENSES/LicenseRef-TCC-1996.txt`. Tick-C is substantially based
on lcc, so the lcc conditions in `LICENSES/LicenseRef-LCC-1994.txt` also apply.
The archive's original copy remains at `doc/original-lcc/COPYRIGHT.lcc` and
must be retained. Those conditions permit copying, modification, and
redistribution when their notice and attribution are kept, but restrict sale
and some profit-making uses. The project therefore must not be described as
MIT-licensed, GPL-licensed, or OSI-approved open source.

Some files have separate terms. In particular, `config/config.guess` and
`config/config.sub` contain GPL-2.0-or-later notices with an exception for
distribution as part of an Autoconf-configured program. Generated `configure`
scripts contain their own unlimited-permission notice. `config/install-sh`
comes from GNU Automake 1.18.1 and contains the X Consortium permission notice;
its Free Software Foundation changes are marked as public domain. Copyright
statements and permission notices in individual files remain controlling.

Standalone modernization files identified as `MIT` in `REUSE.toml` are
available under the MIT License in `LICENSES/MIT.txt`. Modernization changes to
historical files, and new architecture files derived from historical Tick-C or
lcc implementations, are contributed under the inherited Tick-C and lcc terms
instead. This gives each such file one coherent distribution boundary rather
than layering MIT over its modernization delta. It does not relicense any
historical or third-party code.

`REUSE.toml` associates every tracked source, test, documentation, and build
file with a machine-readable SPDX license expression. File-local notices remain
controlling when they provide more specific copyright or permission terms.

Public source distribution remains held at the release gate described in
`doc/modernization/release/README.md`. The project owner has accepted the lcc
conditions for free educational and research-oriented source distribution and
does not seek permission for commercial use or sale. Binary releases, CI
artifacts, and container images are outside the approved publication scope.

This file summarizes the repository's current status. It is not legal advice
and does not replace any file-specific notice.
