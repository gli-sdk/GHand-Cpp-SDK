# GHand SDK C++
[![Version](https://img.shields.io/badge/version-v3.0.0-blue.svg)](include/ghand/version.h)
[![License](https://img.shields.io/badge/license-GPL--3.0-blue.svg)](LICENSE)

[![C++](https://img.shields.io/badge/C%2B%2B-11-blue.svg)](https://en.cppreference.com/w/cpp/11)
[![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux-lightgrey.svg)]()
[![CMake](https://img.shields.io/badge/CMake-3.16+-green.svg)](https://cmake.org/)

[中文](README.zh.md)

C++ SDK for GHand dexterous hands, supporting EtherCAT, CANFD, and RS485 communication with joint control, tactile sensing, and real-time state feedback.

## ✨ Features

- EtherCAT, CANFD, and RS485 communication
- Independent per-joint control for all 5 fingers (position, velocity, torque)
- Real-time data callbacks for joint states and tactile data
- Left and right hand device support
- Automatic product detection via device name matching
- Provided as Windows dynamic-link library (DLL)

## 📖 Official Documentation

For detailed technical specifications and API references, visit: [C++ SDK Developer Documentation](https://fcnzogxju7xr.feishu.cn/docx/Ex2Gd2i5RoJZzcxtIyPcSAW8nVg)

## 📑 Table of Contents

- [Features](#-features)
- [Official Documentation](#-official-documentation)
- [System Requirements](#-system-requirements)
- [Dependencies](#-dependencies)
- [Installation](#-installation)
- [Quick Start](#-quick-start)
- [Build from Source](#-build-from-source)
- [Directory Structure](#-directory-structure)
- [Open Source & Ecosystem Resources](#-open-source--ecosystem-resources)
- [Changelog](#-changelog)
- [Support & Feedback](#-support--feedback)
- [License](#-license)

## 💻 System Requirements

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

## 🔧 Dependencies

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

## 📦 Installation

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
find_package(GHandSDK CONFIG REQUIRED)
target_link_libraries(your_target PRIVATE GHand::ghand)
```

## 🚀 Quick Start

```cpp
#include "ghand/ghand.h"

int main() {
    // One Master per bus; it owns the hands it finds.
    auto master = ghand::Master::Create(ghand::ProductType::GHand5,
                                        ghand::CommType::ETHERCAT);

    // Or auto-detect (matches config by device name after connection)
    // auto master = ghand::Master::Create(ghand::ProductType::AUTO,
    //                                     ghand::CommType::ETHERCAT);

    auto connected = master->Connect("auto");
    if (!connected) {
        printf("Connection failed: %s\n", connected.message.c_str());
        return -1;
    }
    ghand::GHand* hand = connected.value.front();

    // Pull mode: latest cached data. The value is only meaningful when the
    // call succeeded; message says what went wrong otherwise.
    auto state   = hand->GetHandData();     // Hand state
    auto joints  = hand->GetJointsData();   // Joint data
    auto tactile = hand->GetTactileData();  // Tactile data
    if (joints) {
        printf("%zu joints, first angle %.1f deg\n", joints.value.size(),
               joints.value.front().angle);
    }

    // Push mode: register a callback on the hand. It only receives that
    // hand's data, so no hand index is passed to it.
    hand->SetJointsCallback([](const std::vector<ghand::Joint>& joints) {
        for (const auto& j : joints) {
            printf("joint %d: %.1f deg\n", static_cast<int>(j.id), j.angle);
        }
    });

    // Control joints
    std::vector<ghand::JointCommand> cmds = {
        {ghand::JointId::FF_PIP, 45.0f, 50, 50},
        {ghand::JointId::FF_MCP,    30.0f, 50, 50},
    };
    const auto moved = hand->MoveJoints(cmds);
    if (!moved) {
        printf("MoveJoints failed: %s\n", moved.message.c_str());
    }

    master->Disconnect();
}
```

### RS485 Example

```cpp
auto master = ghand::Master::Create(ghand::ProductType::GHand5,
                                    ghand::CommType::RS485);
auto adapters = master->SearchAdapters();
if (!adapters.empty()) {
    auto connected = master->Connect(adapters.begin()->first);
    if (connected) {
        auto info = connected.value.front()->QueryDeviceInfo();
        if (info) {
            printf("Device: %s\n", info.value.device_name.c_str());
        }
        master->Disconnect();
    }
}
```

## 🔨 Build from Source

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
// RS485: slave_id = 0 scans the default IDs 0x31 and 0x32 and manages
// every hand it finds.
master->Connect("auto", 0, ghand::RS485BaudRate::RATE_1000000);

// CAN-FD: use a predefined bit timing profile.
master->Connect("COM17:0", 0x31, ghand::CANFDBitTiming::TIMING_1M_5M);
```

After connecting with `slave_id = 0`, `Connect()` returns every hand it found on
the bus as a `GHand*`. A hand carries its own identity — `slave_index()`,
`hand_type()` and `device_info()` — and every call on it addresses that hand
alone, so no API takes a `slave_index`. Passing an explicit `slave_id` connects
that slave only.

The `GHand*` stays valid until the next `Connect()`/`Disconnect()` on the same
`Master`, or until the `Master` is destroyed.

```cpp
// Two hands on one bus (1-in 2-out): 0x31 and 0x32.
auto connected = master->Connect("COM17:0", 0, ghand::CANFDBitTiming::TIMING_1M_5M);
std::vector<ghand::GHand*> hands = connected.value;

master->MoveJoints(pose);      // session-level: one broadcast frame for both
for (ghand::GHand* h : hands) {
  h->MoveJoints(pose_for(h));  // or drive each hand on its own
}
```

A session-level call is sent as a broadcast without a response on CAN-FD/RS485,
so it is best effort: the feedback callbacks, or a hand's `GetJointsData()`,
confirm what the hands actually did.

`SetSlaveId()` writes a new RS485/CANFD slave ID to holding register `0x0000`.
The ID is not persisted: the device returns to the default 0x31/0x32 after a
power cycle. `SetBaudrateConfig()` writes the enum-mapped protocol gear to
holding register `0x002C`; the device stores it and applies it after the next
power-up.

```cpp
hand->SetSlaveId(0x31);
hand->SetBaudrateConfig(ghand::RS485BaudRate::RATE_1000000);
other_hand->SetBaudrateConfig(ghand::CANFDBitTiming::TIMING_1M_5M);
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

## 📁 Directory Structure

```
ghand-sdk-cpp/
├── include/ghand/    # Public API headers
├── src/              # Source implementation
│   ├── comm/         # Communication layer (EtherCAT, CANFD)
│   └── internal/     # Internal state machine & config
├── config/           # Product configuration (JSON)
├── examples/         # Tutorial and demo programs
├── deps/             # Release-package prebuilt dependencies
```

## 🌐 Open Source & Ecosystem Resources

- **GLI Open Source Hub**: [GLI GitHub Organization](https://github.com/gli-sdk)
- **Official Documentation**: [GHand Dexterous Hand Docs](https://fcnzogxju7xr.feishu.cn/docx/AhZ6ds2iCoguaAxIzBxciYHinNo)
- **Python SDK**: [GHand Python SDK](https://github.com/gli-sdk/GHand-Python-SDK)

## 📋 Changelog

See [CHANGELOG.md](CHANGELOG.md) for a detailed history of changes.

## 📞 Support & Feedback

- 📋 **Technical Support:** For project-specific issues, open an `Issue` in this repository.
- 📧 **General Inquiries:** [support@glitech.com](mailto:support@glitech.com)

## 📄 License

GHand C++ SDK is distributed under the GNU General Public License Version 3
(GPL-3.0-only).

The EtherCAT backend uses SOEM 2.0.0. SOEM 2.0.0 is dual-licensed under
GPLv3 or a commercial license. This open-source distribution uses SOEM under
the GPLv3 option.

Third-party components remain subject to their respective licenses. See
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) and the `LICENSES/` directory
for details.

Copyright (c) 2026 GLITech.
