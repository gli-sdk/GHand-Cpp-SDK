# GHand SDK C++
[![Version](https://img.shields.io/badge/version-v2.0.0-blue.svg)](include/ghand/version.h)
[![License](https://img.shields.io/badge/license-Proprietary-red.svg)](LICENSE)

[![C++](https://img.shields.io/badge/C%2B%2B-11-blue.svg)](https://en.cppreference.com/w/cpp/11)
[![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux-lightgrey.svg)]()
[![CMake](https://img.shields.io/badge/CMake-3.5+-green.svg)](https://cmake.org/)

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
- CMake 3.5 or higher

#### Linux
- Ubuntu 20.04 LTS or higher
- GCC 7.5+ (C++11 support)
- CMake 3.5 or higher
- libssl-dev

## 🔧 Dependencies

### Required Tools
- CMake 3.5 or higher
- C++11 compatible compiler

### Release Dependencies

Source builds use the prebuilt dependencies under `deps/`. The repository and release packages keep this layout, and CMake only searches `deps/`.

| Library | Purpose | License |
|---------|---------|---------|
| [nlohmann/json](https://github.com/nlohmann/json) | JSON parsing | MIT |
| [SOEM](https://github.com/OpenEtherCATsociety/SOEM) | EtherCAT master stack | GPL-2.0 |
| [libmodbus](https://github.com/stephane/libmodbus) | Modbus RTU/RS485 support | LGPL-2.1 |
| WinPcap | Packet capture (Windows EtherCAT) | BSD |

### System Libraries (Linux only)
- libssl-dev
- pthreads

## 📦 Installation

### Prebuilt Library

Copy the following artifacts into your project:

| Artifact | Description |
|----------|-------------|
| `include/ghand/` | Public headers |
| `lib/ghand.dll` / `libghand.so` | Shared library |
| `config/ghand5.json`, `config/ghandlite1.json` | Product configuration |

Link against `ghand` and ensure the JSON config is accessible at runtime.

### CMake Install

```bash
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
cmake --install . --prefix /path/to/install
```

Then in your `CMakeLists.txt`:

```cmake
find_library(GHAND_LIB ghand PATHS /path/to/install/lib)
target_link_libraries(your_target PRIVATE ${GHAND_LIB})
target_include_directories(your_target PRIVATE /path/to/install/include)
```

## 🚀 Quick Start

```cpp
#include "ghand/ghand.h"

int main() {
    // Explicit product type
    auto hand = ghand::DexHand::Create(ghand::ProductType::G5,
                                       ghand::CommType::ETHERCAT);

    // Or auto-detect (matches config by device name after connection)
    // auto hand = ghand::DexHand::Create(ghand::ProductType::AUTO,
    //                                    ghand::CommType::ETHERCAT);

    if (!hand || !hand->Connect("auto")) {
        printf("Connection failed\n");
        return -1;
    }

    // Pull mode: query latest cached data
    auto state   = hand->GetHandData();     // Hand state
    auto joints  = hand->GetJointsData();   // Joint data
    auto tactile = hand->GetTactileData();  // Tactile data

    // Push mode: register callbacks
    hand->SetJointsCallback([](const std::vector<ghand::Joint>& joints) {
        for (const auto& j : joints) {
            printf("Joint %d: %.1f deg\n", static_cast<int>(j.id), j.angle);
        }
    });

    // Control joints
    std::vector<ghand::JointCommand> cmds = {
        {ghand::JointId::THUMB_MCP, 45.0f, 50, 50},
        {ghand::JointId::FF_MCP,    30.0f, 50, 50},
    };
    hand->MoveJoints(cmds);

    hand->Disconnect();
}
```

### RS485 Example

```cpp
auto hand = ghand::DexHand::Create(ghand::ProductType::G5,
                                   ghand::CommType::RS485);
auto adapters = hand->SearchAdapters();
if (!adapters.empty() && hand->Connect(adapters.begin()->first)) {
    auto info = hand->GetDeviceInfo();
    printf("Device: %s\n", info.device_name.c_str());
    hand->Disconnect();
}
```

## 🔨 Build from Source

Build from the repository or release package root that contains `deps/`.

### Windows

```bash
mkdir build && cd build
cmake ..
cmake --build . --config Release

# Run example
.\examples\Release\basic_connection.exe
```

> **Windows RS485:** `modbus.dll` from `deps/lib/windows` is copied next to `ghand.dll` during the build when present.

### Linux

```bash
# Install dependencies
sudo apt install -y cmake build-essential pkg-config libssl-dev

mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# Run example (requires raw socket access)
sudo setcap cap_net_raw,cap_net_admin+eip ./examples/tutorial/basic_connection
./examples/tutorial/basic_connection
```

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
└── lib/              # Precompiled libraries
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

GHand SDK C++ is proprietary software of Glitech. See [LICENSE](LICENSE).

Copyright © 2025 Glitech. All rights reserved.
