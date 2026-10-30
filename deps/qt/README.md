# Qt runtime bundle

`windows/msvc2022_64/` contains the Qt 6.9.1 Windows x64/MSVC 2022 **runtime**
subset used by the GHand tactile display, copied from the same Qt build that
GHand GUI deploys. It lets the GUI examples run on Windows machines without a
Qt installation.

Contents:

- `bin/`: `Qt6Core.dll`, `Qt6Gui.dll`, `Qt6Widgets.dll` (Release).
- `plugins/platforms/`: `qwindows.dll`, `qoffscreen.dll` (the latter for
  hardware-free tests).
- `plugins/styles/`: `qmodernwindowsstyle.dll`.
- `LICENSES/`: original upstream license texts.

The bundle does **not** contain headers, import libraries or Qt tools. Building
the tactile display requires a local Qt 6.2+ (6.9.1 recommended) Widgets
development installation; pass `CMAKE_PREFIX_PATH` when Qt is not on the default
search path:

```powershell
cmake -S . -B build -A x64 -DGHAND_BUILD_TACTILE_DISPLAY=ON -DCMAKE_PREFIX_PATH=C:/Qt/6.9.1/msvc2022_64
cmake --build build --config Release --target tactile_display
```

CMake copies the runtime DLLs and platform plugins of the found Qt installation
next to the examples. To distribute an example without Qt installed, copy
`bin/*.dll` and the `plugins/` directories above alongside the executable.

Upstream provenance and licensing:

- [Qt 6.9.1 source archive](https://download.qt.io/archive/qt/6.9/6.9.1/submodules/)
- [Qt licensing](https://doc.qt.io/qt-6/licensing.html)
- Original license texts: `windows/msvc2022_64/LICENSES/`.

Keep upstream copyright/license notices with redistributed Qt components.
GHand's license does not replace the licenses of Qt or its third-party
components.
