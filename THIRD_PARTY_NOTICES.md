# Third-Party Notices

GHand C++ SDK includes or links against third-party software. Each third-party component remains subject to its own license terms.

## SOEM

- Name: Simple Open EtherCAT Master (SOEM)
- Version: 2.0.0
- Project: https://github.com/OpenEtherCATsociety/SOEM
- License: GPLv3 / Commercial dual license
- Usage in this distribution: GPLv3 option
- Integration: statically linked into `ghand` (`libsoem.a` / `soem.lib`)

The original SOEM copyright notices and license terms are preserved. See `LICENSES/SOEM-LICENSE.md`.

When distributing binaries built with SOEM, provide the corresponding SOEM 2.0.0 source code and any local patches used to build the distributed binary.

## libmodbus

- Name: libmodbus
- Version: 3.1.12
- Project: https://github.com/stephane/libmodbus
- License: LGPL-2.1-or-later
- Integration: dynamically linked

Linux:
- `libmodbus.so.5.1.0`
- `libmodbus.so`
- `libmodbus.so.5`

Windows:
- `modbus.dll`
- `libmodbus.lib` (import library)

libmodbus remains licensed under LGPL-2.1-or-later. See `LICENSES/LGPL-2.1-or-later.txt` and `LICENSES/libmodbus-NOTICE.txt`.

Users must be able to replace the dynamically linked libmodbus binary with an ABI-compatible modified version. When distributing libmodbus binaries, provide the corresponding libmodbus 3.1.12 source code, including any local modifications if present.

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
