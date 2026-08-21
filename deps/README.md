# deps

This directory contains the prebuilt third-party dependencies required to build
the SDK. Release packages should keep the same layout so users can build
without installing extra package managers or system development packages.

Expected layout:

```text
deps/
  include/
    modbus/
    nlohmann/
    soem/
    wpcap/
  lib/
    windows/
      libmodbus.lib
      modbus.dll
      soem.lib
      wpcap.lib
      Packet.lib
    linux/
      libmodbus.so
      libsoem.a
```

The Windows dependency set is included in this repository. Linux packages must
provide `libmodbus.so` and `libsoem.a` under `deps/lib/linux/`.

## Third-Party Licensing

The files under `deps/` are third-party components and are not relicensed by
GHand.

- SOEM 2.0.0: GPLv3 / Commercial dual license; this distribution uses GPLv3.
- libmodbus 3.1.12: LGPL-2.1-or-later.
- nlohmann/json: MIT.
- WinPcap developer components: WinPcap license and incorporated notices.

See `../THIRD_PARTY_NOTICES.md` and `../LICENSES/` for the complete notices.

Do not remove or replace upstream copyright/license notices.
