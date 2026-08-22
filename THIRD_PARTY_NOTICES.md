# Third-Party Notices

GHand C++ SDK includes or links against third-party software. Each third-party component remains subject to its own license terms.

## SOEM

- Name: Simple Open EtherCAT Master (SOEM)
- Version: 2.0.0
- Project: https://github.com/OpenEtherCATsociety/SOEM
- Release tag: `v2.0.0`
- Commit: `304d1c05eab77dc0d426f1a5cf09c8cc7dc03713`
- Release short SHA: `304d1c0`
- License: GPLv3 / Commercial dual license
- Usage in this distribution: GPLv3 option
- Integration: statically linked into `ghand` (`libsoem.a` / `soem.lib`)
- Local modifications for release builds: None

The original SOEM copyright notices and license terms are preserved. See
`LICENSES/SOEM-LICENSE.md` and `deps/SOEM_BUILD_INFO.md`.

When distributing binaries built with SOEM, provide the corresponding official
SOEM 2.0.0 source code identified above. Release binaries must be built from an
unmodified official SOEM v2.0.0 checkout.

## libmodbus

- Name: libmodbus
- Version: system package on Linux; Windows binary distribution version must be verified with the shipped DLL/import library
- Project: https://github.com/stephane/libmodbus
- License: LGPL-2.1-or-later
- Integration: dynamically linked
- Local modifications: None expected

Linux:
- libmodbus is a system dependency and is not redistributed as part of the
  GHand C++ SDK source repository.
- Debian/Ubuntu package: `libmodbus-dev`

Windows:
- Windows release packages may include `modbus.dll` and `libmodbus.lib`.
- If libmodbus binaries are included in a Windows binary distribution, the
  applicable license notice, verified version, and corresponding source
  information must be provided with that distribution.

libmodbus remains licensed under LGPL-2.1-or-later. See `LICENSES/LGPL-2.1-or-later.txt` and `LICENSES/libmodbus-NOTICE.txt`.

Users must be able to replace the dynamically linked libmodbus binary with an
ABI-compatible modified version. When distributing libmodbus binaries, provide
the corresponding libmodbus source code, including any local modifications if
present.

## nlohmann/json

- Name: JSON for Modern C++
- Project: https://github.com/nlohmann/json
- License: MIT
- Integration: header-only (`deps/include/nlohmann/json.hpp`)

See `LICENSES/nlohmann-json-MIT.txt`.

## WinPcap

- Name: WinPcap Developer Pack components
- Project: https://www.winpcap.org/
- License: WinPcap BSD-style license and incorporated component notices
- Integration: Windows EtherCAT build dependency

Distributed components include WinPcap-compatible headers and import libraries such as `wpcap.lib` and `Packet.lib`.

See `LICENSES/WinPcap.txt`.

## OpenSSL

On Linux, GHand uses the system-provided OpenSSL library for hashing support. OpenSSL is not currently bundled in the GHand SDK release archive.

If OpenSSL binaries are bundled in a future release, the corresponding OpenSSL license and notices must also be included in the distribution.
