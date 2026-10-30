# GHand SDK C++
[![Version](https://img.shields.io/badge/version-V3.1.0-blue.svg)](include/ghand/version.h)
[![License](https://img.shields.io/badge/license-GPL--3.0-blue.svg)](LICENSE)

[![C++](https://img.shields.io/badge/C%2B%2B-11-blue.svg)](https://en.cppreference.com/w/cpp/11)
[![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux-lightgrey.svg)]()
[![CMake](https://img.shields.io/badge/CMake-3.16+-green.svg)](https://cmake.org/)

[中文](README.zh.md)

C++ SDK for GHand dexterous hands, supporting EtherCAT, CANFD, and RS485 communication with joint control, tactile sensing, and real-time state feedback.

## Features

- EtherCAT, CANFD, and RS485 communication
- Independent per-joint control for all 5 fingers (position, velocity, current, MIT and CUSTOM)
- MIT and CUSTOM control tutorials with command feedback and explicit hand disabling
- Optional Qt 6 tactile display aligned with Python: whole-hand view, finger maps and 60-second XYZ force plots
- Real-time data callbacks for joint states and tactile data
- Left and right hand device support
- Automatic product detection via device name matching
- Same-product left and right hands sharing a bus with independent control and feedback
- Windows DLL and Linux shared-library builds

V3.1.0 uses the `Master` / `GHand` ownership model and new-protocol command types.
Applications upgrading from 2.x must rebuild against matching headers and libraries;
see [CHANGELOG.md](CHANGELOG.md) for API and protocol migration details.

## Official Documentation

For detailed technical specifications and API references, visit: [C++ SDK Developer Documentation](https://fcnzogxju7xr.feishu.cn/docx/Ex2Gd2i5RoJZzcxtIyPcSAW8nVg)

## Table of Contents

- [Features](#features)
- [Official Documentation](#official-documentation)
- [System Requirements](#system-requirements)
- [Dependencies](#dependencies)
- [Installation](#installation)
- [Quick Start](#quick-start)
- [Examples](#examples)
- [Build from Source](#build-from-source)
- [Directory Structure](#directory-structure)
- [Open Source & Ecosystem Resources](#open-source--ecosystem-resources)
- [Changelog](#changelog)
- [Support & Feedback](#support--feedback)
- [License](#license)

## System Requirements

### Supported Platforms

| Platform | Arch | Status | Compiler |
|----------|------|--------|----------|
| **Windows** | x64 | Stable | Visual Studio 2017+ |
| **Linux** | x64 | Stable | GCC 7.5+ (C++11) |

#### Windows
- Windows 7 or higher
- Visual Studio 2017 or higher
- CMake 3.16 or higher

#### Linux
- Ubuntu 20.04 LTS or higher
- GCC 7.5+ (C++11 support)
- CMake 3.16 or higher
- libssl-dev
- libmodbus-dev

## Dependencies

### Required Tools
- CMake 3.16 or higher
- C++11 compatible compiler

### Release Dependencies

Source builds use the bundled SOEM and header-only dependencies under `deps/`.
Linux builds use the system `libmodbus` development package discovered through
pkg-config; `libmodbus.so*` is not bundled in the source repository. Windows
builds use the libmodbus import library/DLL supplied in the Windows release
dependency set.

| Library | Version | Purpose | License | Usage in GHand |
|---------|---------|---------|---------|----------------|
| [SOEM](https://github.com/OpenEtherCATsociety/SOEM) | 2.0.0 | EtherCAT master stack | GPL-3.0 / Commercial dual license | GPL-3.0 option is used by this open-source distribution |
| [libmodbus](https://github.com/stephane/libmodbus) | 3.1.12 | Modbus RTU/RS485 support | LGPL-2.1-or-later | Dynamically linked |
| [nlohmann/json](https://github.com/nlohmann/json) | bundled header | JSON parsing | MIT | Header-only |
| WinPcap | Developer Pack components | Packet capture (Windows EtherCAT) | WinPcap BSD-style license and incorporated notices | Headers and import libraries |

### System Libraries (Linux only)
- libssl-dev
- libmodbus-dev
- pkg-config
- pthreads

## Installation

### Prebuilt Library

Copy the following artifacts into your project:

| Artifact | Description |
|----------|-------------|
| `include/ghand/` | Public headers |
| `ghand.dll` / `libghand.so` | Shared library |
| `config/ghand5.json`, `config/ghandlite1.json` | Product configuration |

Link against `ghand` and ensure the JSON config is accessible at runtime.

### CMake Install

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --install build --prefix /path/to/install
```

Then in your `CMakeLists.txt`:

```cmake
find_library(GHAND_LIB ghand PATHS /path/to/install/lib)
target_link_libraries(your_target PRIVATE ${GHAND_LIB})
target_include_directories(your_target PRIVATE /path/to/install/include)
```

The install also exports a CMake package target:

```cmake
find_package(GHandSDK 3 CONFIG REQUIRED)
target_link_libraries(your_target PRIVATE GHand::ghand)
```

## Quick Start

```cpp
#include <iostream>
#include "ghand/ghand.h"

int main() {
    auto master = ghand::Master::Create(ghand::ProductType::GHand5,
                                      ghand::CommType::ETHERCAT);
    if (!master) return 1;
    auto connected = master->Connect("auto");
    if (!connected) { std::cerr << connected.message; return 1; }
    for (ghand::GHand* hand : connected.value) {
        auto id = hand->GetSlaveIndex();
        hand->SetJointsCallback([id](const std::vector<ghand::JointData>& joints) {
            std::cout << "Slave " << id << ": " << joints.size() << " joints\n";
        });
        auto state = hand->GetHandStateMachine();
        if (!state) std::cerr << state.message << '\n';
    }
    for (auto* hand : connected.value) {
        auto stopped = hand->Stop();
        if (!stopped) std::cerr << stopped.message << '\n';
    }
    return master->Disconnect() ? 0 : 1;
}
```

### RS485 Example

```cpp
auto master = ghand::Master::Create(ghand::ProductType::GHand5, ghand::CommType::RS485);
auto connected = master->Connect("COM7", 0);
if (connected) {
    if (auto left = master->hand(ghand::HandType::LEFT)) {
        auto info = left->GetDeviceInfo();
        if (info) std::cout << info.value.device_name << '\n';
    }
    master->Disconnect();
}
```

### Public type migration

Public types follow the Python SDK: `HandData`, `JointData`, `HandControlMode`,
`JointControlMode`, `JointLoopType`, `HandRuntimeState`, and `JointState`.
Use `HandType::LEFT_HAND` / `RIGHT_HAND` and `HandControlMode::VELOCITY`.
`JointCommand` uses `control_mode`, `loop_type`, and `control_state`; its defaults
are POSITION, CLOSED_LOOP, and DISABLED. Explicitly select RUNNING to move.
`HandData::joints_state` replaces `state`. Tactile callbacks receive a
`std::map<TactileSensorId, TactileData>` map keyed by `TactileSensorId`; each `TactileData` contains flattened
force triples in Newtons, with empty vectors indicating unavailable readings.
Hand error flags support `|` and `&`, including fault bits above 63.
RS485 retains `RATE_*` names because Windows defines conflicting `BAUD_*` macros.

## Examples

Tutorial sources are numbered in learning order under `examples/tutorial/`.
Executable names omit the numeric prefix and are defined in `examples/CMakeLists.txt`.

| Source | Executable | Purpose |
| --- | --- | --- |
| [01_basic_connection.cc](examples/tutorial/01_basic_connection.cc) | `basic_connection` | Connect and query device information |
| [02_move_joints.cc](examples/tutorial/02_move_joints.cc) | `move_joints` | Position commands, parameter units, control modes and joint feedback |
| [03_speed_control.cc](examples/tutorial/03_speed_control.cc) | `speed_control` | Velocity control |
| [04_current_control.cc](examples/tutorial/04_current_control.cc) | `current_control` | Current control |
| [05_tactile_callback.cc](examples/tutorial/05_tactile_callback.cc) | `tactile_callback` | Tactile feedback callbacks |
| [06_multi_hand.cc](examples/tutorial/06_multi_hand.cc) | `multi_hand` | Multi-hand control |
| [07_glove_control.cc](examples/tutorial/07_glove_control.cc) | `glove_control` | Glove control |
| [08_interactive_joint_control.cc](examples/tutorial/08_interactive_joint_control.cc) | `interactive_joint_control` | Interactive joint commands and feedback |
| [09_hand_state.cc](examples/tutorial/09_hand_state.cc) | `hand_state` | Hand state and joint feedback |
| [10_switch_baud_rate.cc](examples/tutorial/10_switch_baud_rate.cc) | `switch_baud_rate` | CANFD bit-timing configuration |
| [11_connect_hands.cc](examples/tutorial/11_connect_hands.cc) | `connect_hands` | Shared-bus discovery, bus-wide and per-hand commands |
| [13_mit_control.cc](examples/tutorial/13_mit_control.cc) | `mit_control` | MIT compliant grasp with tactile contact, hold and release |
| [14_custom_control.cc](examples/tutorial/14_custom_control.cc) | `custom_control` | Per-finger position/current modes and feedback |
| [15_tactile_display.cc](examples/tutorial/15_tactile_display.cc) | `tactile_display` | Optional Qt 6 tactile maps, force arrows and XYZ history |

The new tutorials retain the Python SDK's 13/14/15 numbering. MIT uses degrees and
velocity/current percentages, `kp=10..2000`, `kd=0..50`, `kp>kd`; current is feed-forward.
CUSTOM requires consistent mode/loop selection within each finger arbitration group,
including disabled joints. Check feedback after sending and disable the hand before disconnecting.

### Qt tactile display

Enable the optional Qt 6.2+ Widgets target (C++17 for GUI targets only):

```powershell
cmake -S . -B build -DGHAND_BUILD_TACTILE_DISPLAY=ON -DCMAKE_PREFIX_PATH=C:/Qt/6.9.1/msvc2022_64
cmake --build build --config Release --target tactile_display mit_control custom_control
C:/Qt/6.9.1/msvc2022_64/bin/windeployqt.exe build/examples/Release/tactile_display.exe
./build/examples/Release/tactile_display.exe --preview
```

On Linux, use your Qt prefix and `./build/examples/tactile_display --preview`.
Without `--preview`, the example connects GHand5 over EtherCAT (`auto`). Edit the
connection settings for other transports. The Tactile switch starts acquisition;
zero with unloaded sensors. The Python layout is retained: a 1600×1000 black window,
whole-hand view and five finger maps on the left, and five XYZ plots on the right.
Device identity selects the left/right hand image. GHand5's 38/31/31/31/31 point
order, image offsets, normal-force colors/glow, tangential arrows, Fz bars and
point tooltips follow Python. Plots retain 60 seconds of timestamped history,
coalesce callbacks into 50 Hz buckets and insert gaps after 0.5 seconds without data.
Images are embedded with Qt resources, without a runtime dependency on Python.

The reusable [display widget](src/tactile_display/) links as `ghand_tactile_display`.
The caller owns the connection and must keep the hand alive until the widget is
destroyed. The widget exclusively uses its tactile callback slot and closes tactile
acquisition and joins its worker on shutdown. It sends no motion commands.
Qt is not required by the default build. Enable `GHAND_BUILD_TESTS` for hardware-free
checks using `ctest --test-dir build -C Release --output-on-failure`; on Windows,
put Qt's `bin` on `PATH` and point `QT_QPA_PLATFORM_PLUGIN_PATH` to Qt's `plugins`.

Action demos, including `preset_gesture`, `gesture_dance` and `clap_pose`, are in
[`examples/demo/`](examples/demo/).

After building, run `connect_hands` with a transport and optional device:

```bash
./build/examples/connect_hands ethcat auto
./build/examples/connect_hands canfd /dev/ttyACM0:0
./build/examples/connect_hands rs485 /dev/ttyUSB0
```

On Windows, use `build\examples\Release\connect_hands.exe` with devices such as
`COM17:0` (CANFD) or `COM7` (RS485). The example moves the connected hands;
use `--help` to view its arguments and supported topologies.

## Build from Source

Build from the repository or release package root that contains `deps/`.

### Windows

```bash
cmake -S . -B build
cmake --build build --config Release

# Run example
.\build\examples\Release\basic_connection.exe
```

> **Windows RS485:** `modbus.dll` from `deps/lib/windows` is copied next to `ghand.dll` during the build when present.

### Linux

```bash
# Install dependencies
sudo apt install -y cmake build-essential pkg-config libssl-dev libmodbus-dev

mkdir -p build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# Grant raw socket permissions once to all built examples
find build/examples -maxdepth 1 -type f -executable \
  -exec sudo setcap cap_net_raw,cap_net_admin=eip {} \;

# Run the EtherCAT example
./build/examples/basic_connection
```

### Linux RS485 / CANFD

RS485 and CANFD both use USB serial devices and do not need `setcap`; raw socket permissions are only required for EtherCAT.

```bash
# Check serial devices
ls /dev/ttyUSB* /dev/ttyACM* 2>/dev/null
ls -l /dev/serial/by-id 2>/dev/null

# If the current user cannot open serial ports, add it to dialout and log in again
sudo usermod -aG dialout $USER

# Make the current terminal session pick up the group, or log in again
newgrp dialout
```

For RS485 debugging, specify the USB-RS485 adapter explicitly. Common device names are `/dev/ttyUSB0` or `/dev/serial/by-id/usb-1a86_USB_Serial-if00-port0`:

```cpp
auto rs485_master = ghand::Master::Create(ghand::ProductType::GHand5,
                                         ghand::CommType::RS485);
rs485_master->Connect("/dev/ttyUSB0");
```

For CANFD, use the ZQWL USB-CDC adapter and append the channel number to the device name, for example `:0`:

```cpp
auto canfd_master = ghand::Master::Create(ghand::ProductType::GHand5,
                                         ghand::CommType::CANFD);
canfd_master->Connect("/dev/ttyACM0:0");
```

`Connect()` accepts optional RS485/CANFD slave ID and typed baud-rate or bit
timing parameters:

```cpp
// RS485: slave_id = 0 scans the default IDs 0x31 and 0x32.
master->Connect("auto", 0, ghand::RS485BaudRate::RATE_1000000);

// CAN-FD: use a predefined bit timing profile.
master->Connect("COM17:0", 0x31, ghand::CANFDBitTiming::TIMING_1M_5M);
```

`SetSlaveId()` writes a new RS485/CANFD slave ID to holding register `0x0003`.
`SetBaudrateConfig()` writes the enum-mapped protocol gear to holding register
`0x0004`; the device stores it and applies it after the next power-up.

```cpp
hand->SetBaudrateConfig(ghand::RS485BaudRate::RATE_1000000);
hand->SetBaudrateConfig(ghand::CANFDBitTiming::TIMING_1M_5M);
```

RS485 baud-rate gears:

| Gear | Baud rate |
| --- | --- |
| `0x00` | 57,600 bps |
| `0x01` | 115,200 bps |
| `0x02` | 230,400 bps |
| `0x03` | 460,800 bps |
| `0x04` | 921,600 bps |
| `0x05` | 1,000,000 bps |

CANFD baud-rate gears:

| Gear | Arbitration phase | Data phase |
| --- | --- | --- |
| `0x00` | 500 Kbps, 80% sample point | 1 Mbps, 75% sample point |
| `0x01` | 500 Kbps, 80% sample point | 2 Mbps, 80% sample point |
| `0x02` | 500 Kbps, 80% sample point | 4 Mbps, 80% sample point |
| `0x03` | 500 Kbps, 80% sample point | 5 Mbps, 75% sample point |
| `0x04` | 1 Mbps, 75% sample point | 4 Mbps, 80% sample point |
| `0x05` | 1 Mbps, 75% sample point | 5 Mbps, 75% sample point |

## Directory Structure

```
ghand-sdk-cpp/
|-- include/ghand/    # Public API headers
|-- src/              # Source implementation
|   |-- comm/         # Communication layer (EtherCAT, CANFD, RS485)
|   `-- internal/     # Internal state machine & config
|-- config/           # Product configuration (JSON)
|-- cmake/            # CMake package config template
|-- examples/         # Tutorial and demo programs
`-- deps/             # Release-package prebuilt dependencies
```

## Open Source & Ecosystem Resources

- **GLI Open Source Hub**: [GLI GitHub Organization](https://github.com/gli-sdk)
- **Official Documentation**: [GHand Dexterous Hand Docs](https://fcnzogxju7xr.feishu.cn/docx/AhZ6ds2iCoguaAxIzBxciYHinNo)
- **Python SDK**: [GHand Python SDK](https://github.com/gli-sdk/GHand-Python-SDK)

## Changelog

See [CHANGELOG.md](CHANGELOG.md) for a detailed history of changes.

## Support & Feedback

- **Technical Support:** For project-specific issues, open an `Issue` in this repository.
- **General Inquiries:** [support@glitech.com](mailto:support@glitech.com)

## License

GHand C++ SDK is distributed under the GNU General Public License Version 3
(GPL-3.0-only).

The EtherCAT backend uses SOEM 2.0.0. SOEM 2.0.0 is dual-licensed under
GPLv3 or a commercial license. This open-source distribution uses SOEM under
the GPLv3 option.

Third-party components remain subject to their respective licenses. See
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) and the `LICENSES/` directory
for details.

Copyright (c) 2026 GLITech.
