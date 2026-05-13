# GHand SDK C++

[中文](README.zh.md)

C++ SDK for GHand dexterous hands, supporting EtherCAT and CANFD communication with joint control, tactile sensing, and real-time state feedback.

## Features

- EtherCAT and CANFD communication
- Independent per-joint control (position, velocity, torque)
- Push mode: real-time data callbacks
- Pull mode: on-demand cached data queries
- Automatic product detection via device name matching

## Directory Structure

```
ghand-sdk-cpp/
├── include/ghand/            # Public headers
│   ├── ghand.h               # Main API
│   ├── types.h               # Type definitions
│   ├── logging.h             # Logging
│   ├── export.h              # DLL export macros
│   └── version.h             # Version info
├── src/
│   ├── ghand.cc              # Public API (PIMPL bridge)
│   ├── logging.cc            # Logger implementation
│   ├── types.cc              # Type serialization
│   ├── comm/                 # Communication layer
│   │   ├── icomm.h           # Abstract interface
│   │   ├── ethercat_comm.h   # EtherCAT implementation
│   │   ├── ethercat_comm.cc
│   │   ├── canfd_comm.h      # CANFD implementation
│   │   ├── canfd_comm.cc
│   │   ├── canfd_protocol.h  # CANFD protocol
│   │   ├── canfd_protocol.cc
│   │   ├── canfd_driver.h    # CANFD driver abstraction
│   │   └── canfd_driver_zlg.cc  # ZLG driver
│   └── internal/             # Internal implementation
│       ├── ghand.h           # State machine
│       ├── ghand.cc
│       ├── dexhand_callback_manager.h  # Callback + data cache
│       ├── dexhand_callback_manager.cc
│       ├── product_config.h          # Config structures
│       ├── product_config_loader.h   # JSON config loader
│       ├── product_config_loader.cc
│       ├── file_lock.h       # Device file lock
│       └── file_lock.cc
├── config/                   # Product configuration
│   └── xiaoyao_hand.json
├── examples/
│   ├── tutorial/             # Getting started (8 examples)
│   └── demo/                 # Feature demos (10 examples)
└── third_party/              # Third-party libraries
    ├── include/
    │   ├── soem/             # SOEM (EtherCAT)
    │   ├── wpcap/            # WinPcap
    │   ├── zlgcan/           # ZLG CAN
    │   └── nlohmann/         # JSON parser
    └── lib/
        ├── windows/
        │   ├── soem.lib
        │   ├── wpcap.lib
        │   ├── Packet.lib
        │   ├── zlgcan.lib
        │   ├── zlgcan.dll
        │   └── kerneldlls/
        └── linux/
            └── libsoem.a
```

## Quick Start

```cpp
#include "ghand/ghand.h"

using namespace ghand;

int main() {
    // Explicit product type
    auto hand = DexHand::Create(ProductType::G5, CommType::ETHERCAT);

    // Or auto-detect (matches config by device name after connection)
    auto hand = DexHand::Create(ProductType::AUTO, CommType::ETHERCAT);

    if (!hand || !hand->Connect("auto")) {
        printf("Connection failed\n");
        return -1;
    }

    // Pull mode: query latest cached data
    auto state   = hand->GetHandData();     // Hand state
    auto joints  = hand->GetJointsData();   // Joint data
    auto tactile = hand->GetTactileData();  // Tactile data

    // Push mode: register callbacks
    hand->SetJointsCallback([](const std::vector<Joint>& joints) {
        for (const auto& j : joints) {
            printf("Joint %d: %.1f deg\n", (int)j.id, j.angle);
        }
    });

    // Control joints
    std::vector<JointCommand> cmds = {
        {JointId::THUMB_MCP, 45.0f, 50, 50},
        {JointId::FF_MCP,    30.0f, 50, 50},
    };
    hand->MoveJoints(cmds);

    hand->Disconnect();
}
```

## Logging

```cpp
#include "ghand/logging.h"

ghand::ConfigureConsole(ghand::LogLevel::INFO);
ghand::ConfigureFile("ghand.log");
```

## Build

### Windows
```bash
mkdir build && cd build
cmake ..
cmake --build . --config Release
.\examples\Release\basic_connection.exe
```

### Linux
```bash
sudo apt install -y cmake build-essential pkg-config libpcap-dev libssl-dev
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
sudo ./examples/tutorial/basic_connection
```

## System Requirements

| Platform | Arch  | Compiler       |
|----------|-------|----------------|
| Windows 7+ | x64 | MSVC 2017+     |
| Ubuntu 20.04+ | x64 | GCC 7.5+       |

## License

GHand SDK C++ is proprietary software of Glitech. See [LICENSE](LICENSE).

Copyright © 2025 Glitech. All rights reserved.
