# GHand SDK C++
[![Version](https://img.shields.io/badge/version-v2.0.0-blue.svg)](include/ghand/version.h)
[![License](https://img.shields.io/badge/license-Proprietary-red.svg)](LICENSE)

[![C++](https://img.shields.io/badge/C%2B%2B-11-blue.svg)](https://en.cppreference.com/w/cpp/11)
[![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux-lightgrey.svg)]()
[![CMake](https://img.shields.io/badge/CMake-3.5+-green.svg)](https://cmake.org/)

[English](README.md)

GHand SDK C++ 是GHand灵巧手的 C++ 开发包，支持 EtherCAT 、CANFD 和 RS485 通信，提供关节控制、触觉数据采集和实时状态反馈。

## ✨ 功能特性

- 支持 EtherCAT、CANFD 和 RS485 通信
- 5 根手指独立关节控制（位置、速度、扭矩）
- 关节状态与触觉数据实时回调
- 支持左手与右手设备
- 通过设备名称匹配自动识别产品类型
- Windows 动态链接库 (DLL) 形式提供

## 📖 官方文档

详细技术规格和 API 参考请查看：[C++ SDK 开发者文档](https://fcnzogxju7xr.feishu.cn/docx/Ex2Gd2i5RoJZzcxtIyPcSAW8nVg)

## 📑 目录

- [功能特性](#-功能特性)
- [官方文档](#-官方文档)
- [系统要求](#-系统要求)
- [依赖说明](#-依赖说明)
- [安装](#-安装)
- [快速开始](#-快速开始)
- [源码编译](#-源码编译)
- [目录结构](#-目录结构)
- [开源与生态资源](#-开源与生态资源)
- [更新日志](#-更新日志)
- [支持与反馈](#-支持与反馈)
- [许可证](#-许可证)

## 💻 系统要求

### 支持的平台

| 平台 | 架构 | 状态 | 编译器 |
|------|------|------|--------|
| **Windows** | x64 | ✅ 稳定 | Visual Studio 2017+ |
| **Linux** | x64 | ✅ 稳定 | GCC 7.5+ (C++11) |

#### Windows
- Windows 7 或更高版本
- Visual Studio 2017 或更高版本
- CMake 3.5 或更高版本

#### Linux
- Ubuntu 20.04 LTS 或更高版本
- GCC 7.5+（支持 C++11）
- CMake 3.5 或更高版本
- libpcap-dev, libssl-dev

## 🔧 依赖说明

### 必需工具
- CMake 3.5 或更高版本
- 支持 C++11 的编译器

### 已集成的第三方库
以下库已包含在 `third_party/` 目录中，会自动编译：

| 库 | 用途 | 许可证 |
|----|------|--------|
| [SOEM](https://github.com/OpenEtherCATsociety/SOEM) | EtherCAT 主站协议栈 | GPL-2.0 |
| [nlohmann/json](https://github.com/nlohmann/json) | JSON 解析 | MIT |
| [ZLG CAN](https://www.zlg.cn/) | CANFD 驱动（Windows） | 专有 |
| WinPcap | 数据包捕获（Windows） | BSD |
| [libmodbus](https://github.com/stephane/libmodbus) | Modbus RTU/RS485 支持 | LGPL-2.1 |

### 系统库（仅 Linux）
- libpcap-dev
- libssl-dev
- libmodbus-dev
- pthreads

## 📦 安装

### 预编译库

将以下文件复制到你的项目中：

| 文件 | 说明 |
|------|------|
| `include/ghand/` | 公共头文件 |
| `lib/ghand.dll` / `libghand.so` | 动态链接库 |
| `config/xiaoyao_hand.json` | 产品配置文件 |

链接 `ghand` 库，并确保运行时能访问 JSON 配置文件。

### CMake 安装

```bash
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
cmake --install . --prefix /path/to/install
```

然后在你的 `CMakeLists.txt` 中：

```cmake
find_library(GHAND_LIB ghand PATHS /path/to/install/lib)
target_link_libraries(your_target PRIVATE ${GHAND_LIB})
target_include_directories(your_target PRIVATE /path/to/install/include)
```

## 🚀 快速开始

```cpp
#include "ghand/ghand.h"

int main() {
    // 指定产品类型
    auto hand = ghand::DexHand::Create(ghand::ProductType::G5,
                                       ghand::CommType::ETHERCAT);

    // 或自动识别（连接后根据设备名匹配配置）
    // auto hand = ghand::DexHand::Create(ghand::ProductType::AUTO,
    //                                    ghand::CommType::ETHERCAT);

    if (!hand || !hand->Connect("auto")) {
        printf("连接失败\n");
        return -1;
    }

    // 拉模式：主动查询最新缓存
    auto state   = hand->GetHandData();     // 手部状态
    auto joints  = hand->GetJointsData();   // 关节数据
    auto tactile = hand->GetTactileData();  // 触觉数据

    // 推模式：注册回调
    hand->SetJointsCallback([](const std::vector<ghand::Joint>& joints) {
        for (const auto& j : joints) {
            printf("关节 %d: %.1f°\n", static_cast<int>(j.id), j.angle);
        }
    });

    // 控制关节
    std::vector<ghand::JointCommand> cmds = {
        {ghand::JointId::THUMB_TMC_FE, 45.0f, 50, 50},
        {ghand::JointId::FF_MCP,    30.0f, 50, 50},
    };
    hand->MoveJoints(cmds);

    hand->Disconnect();
}
```

### RS485 示例

```cpp
auto hand = ghand::DexHand::Create(ghand::ProductType::G5,
                                   ghand::CommType::RS485);
auto adapters = hand->SearchAdapters();
if (!adapters.empty() && hand->Connect(adapters.begin()->first)) {
    auto info = hand->GetDeviceInfo();
    printf("设备: %s\n", info.device_name.c_str());
    hand->Disconnect();
}
```

## 🔨 源码编译

### Windows

```bash
mkdir build && cd build
cmake ..
cmake --build . --config Release

# 运行示例
.\examples\Release\basic_connection.exe
```

> **Windows RS485 使用提示：** 如果使用 RS485 通信，请将 `libmodbus.dll` 与 `ghand.dll` 放在同一目录（或系统 `PATH` 中的目录）。当 `third_party/lib/windows/` 中存在该 DLL 时，构建系统会自动复制。

### Linux

```bash
# 安装依赖
sudo apt install -y cmake build-essential pkg-config libpcap-dev libssl-dev libmodbus-dev

mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# 运行示例（需要原始套接字权限）
sudo setcap cap_net_raw,cap_net_admin+eip ./examples/tutorial/basic_connection
./examples/tutorial/basic_connection
```

## 📁 目录结构

```
ghand-sdk-cpp/
├── include/ghand/    # 公共 API 头文件
├── src/              # 源码实现
│   ├── comm/         # 通信层（EtherCAT、CANFD）
│   └── internal/     # 内部状态机与配置
├── config/           # 产品配置文件（JSON）
├── examples/         # 教程与示例程序
├── third_party/      # 集成依赖库（SOEM、ZLG CAN 等）
└── lib/              # 预编译库
```

## 🌐 开源与生态资源

- **GLI 开源中心**：[GLI SDK GitHub 组织](https://github.com/gli-sdk)
- **官方文档**：[GHand 灵巧手文档](https://fcnzogxju7xr.feishu.cn/docx/AhZ6ds2iCoguaAxIzBxciYHinNo)
- **Python SDK**：[GHand Python SDK](https://github.com/gli-sdk/GHand-Python-SDK)

## 📋 更新日志

详见 [CHANGELOG.md](CHANGELOG.md)。

## 📞 支持与反馈

- 📋 **技术支持：** 如有项目相关问题，请在本仓库提交 `Issue`。
- 📧 **一般咨询：** [support@glitech.com](mailto:support@glitech.com)

## 📄 许可证

GHand SDK C++ 是 Glitech 的专有软件。使用本 SDK 前，请参阅 [LICENSE](LICENSE) 文件了解完整条款。

Copyright © 2025 Glitech. All rights reserved.
