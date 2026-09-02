# SOEM Build Information

This file records the provenance required for the SOEM headers and prebuilt
libraries bundled with the GHand C++ SDK.

## Upstream Source

- Upstream: https://github.com/OpenEtherCATsociety/SOEM
- Version: 2.0.0
- Release tag: `v2.0.0`
- Commit: `304d1c05eab77dc0d426f1a5cf09c8cc7dc03713`
- Release short SHA: `304d1c0`
- Local source modifications for release builds: None
- License route used by GHand C++ SDK: GPLv3

The SOEM headers and libraries distributed in a release package must be built
from the same unmodified official `v2.0.0` source checkout.

## Required Verification Before Release

Before publishing a GHand C++ SDK release, verify and fill in the exact build
details below from the clean SOEM checkout used to produce the bundled binaries:

```text
git clone https://github.com/OpenEtherCATsociety/SOEM.git
cd SOEM
git checkout v2.0.0
git status
git diff
git rev-parse HEAD
```

Expected source state:

- `git status`: clean
- `git diff`: no changes
- Full commit SHA: `304d1c05eab77dc0d426f1a5cf09c8cc7dc03713`

## Linux

- Architecture: x86_64
- Compiler: not recorded in this repository
- Compiler version: not recorded in this repository
- Build type: Release
- Output: `deps/lib/linux/libsoem.a`
- Status: prebuilt archive included in this repository. The exact Linux build
  environment was not recorded here; refresh this entry with the compiler and
  toolchain details whenever `deps/lib/linux/libsoem.a` is replaced.

## Windows

- Architecture: x64
- Compiler: MSVC
- Compiler version: 19.44.35215.0
- CMake generator: Ninja
- CMake version: 4.1.0
- Build type: Release
- Output: `deps/lib/windows/soem.lib`
- Status: rebuilt from the clean official SOEM v2.0.0 source in this workspace
  and copied to `deps/lib/windows/soem.lib`.

## Release Rule

Do not replace `deps/lib/linux/libsoem.a`, `deps/lib/windows/soem.lib`, or
`deps/include/soem/` without updating this file, `THIRD_PARTY_NOTICES.md`, and
the release validation notes.
