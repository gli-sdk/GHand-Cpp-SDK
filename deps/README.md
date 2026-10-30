# deps

This directory contains the bundled third-party dependencies required to build
the SDK. Release packages should keep the same layout, except Linux libmodbus is
a system dependency and is intentionally not bundled in this repository.

Expected layout:

```text
deps/
  include/
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
      libsoem.a
  qt/
    windows/
      msvc2022_64/    # Qt 6.9.1 runtime DLLs/plugins, Release only
```

The Windows dependency set is included in this repository. Linux builds use the
bundled SOEM static library and the system `libmodbus` development package
discovered through pkg-config.

The optional tactile display is built against a local Qt 6 Widgets development
installation (`-DGHAND_BUILD_TACTILE_DISPLAY=ON`, CMake 3.22+ and MSVC 2022);
pass `CMAKE_PREFIX_PATH` when Qt is not on the default search path. CMake copies
the found runtime DLLs and plugins next to the GUI examples and tests.
`deps/qt/` contains only the Qt 6.9.1 Windows x64 runtime (Core/Gui/Widgets DLLs,
window/offscreen/style plugins) for redistributing or running the examples on
machines without Qt; it is Release-only. Linux uses its own Qt development
installation.
See [qt/README.md](qt/README.md) for provenance and licensing.

## Third-Party Licensing

The files under `deps/` are third-party components and are not relicensed by
GHand.

- SOEM 2.0.0: GPLv3 / Commercial dual license; this distribution uses GPLv3.
- libmodbus: LGPL-2.1-or-later; Linux uses the system package, Windows
  distributions may include the DLL/import library with corresponding notices.
- nlohmann/json: MIT.
- WinPcap developer components: WinPcap license and incorporated notices.

See `../THIRD_PARTY_NOTICES.md` and `../LICENSES/` for the complete notices.
See `SOEM_BUILD_INFO.md` for the required SOEM binary provenance record.

Do not remove or replace upstream copyright/license notices.
