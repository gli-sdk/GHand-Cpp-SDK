# deps

This directory contains the bundled third-party dependencies required to build
the SDK. Linux source builds use the system libmodbus development package.
Release executables and their runtime dependencies are provided under `dist/`.

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
```

The Windows dependency set is included in this repository. Linux builds use the
bundled SOEM static library and the system `libmodbus` development package
discovered through pkg-config.

The tactile display example is built only when CMake finds a local Qt 6 Widgets
development installation (MSVC 2022 x64 on Windows); pass `CMAKE_PREFIX_PATH`
when Qt is not on the default search path. CMake does not deploy Qt runtime
dependencies. The release package's `dist/` directory contains the executables
and their required runtime libraries and plugins.
Keep upstream license texts and notices with redistributed Qt components.
Qt 6.9.1 runtime files are deployed from the local MSVC 2022 x64 Qt installation
using `windeployqt --release`. See `../dist/README.md` for deployment details,
`../dist/windows/LICENSES/Qt/` for the retained upstream license texts, and
`../THIRD_PARTY_NOTICES.md` for Qt licensing and corresponding source links.
The project requires CMake 3.16 or newer; GUI builds must also meet the selected
Qt version's requirements (Qt 6.9 requires CMake 3.22 on most target platforms).

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
