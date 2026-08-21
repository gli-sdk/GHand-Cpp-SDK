# GHand SDK C++
[![Version](https://img.shields.io/badge/version-v2.2.0-blue.svg)](include/ghand/version.h)
[![License](https://img.shields.io/badge/license-GPL--3.0-blue.svg)](LICENSE)

[![C++](https://img.shields.io/badge/C%2B%2B-11-blue.svg)](https://en.cppreference.com/w/cpp/11)
[![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux-lightgrey.svg)]()
[![CMake](https://img.shields.io/badge/CMake-3.16+-green.svg)](https://cmake.org/)

[English](README.md)

GHand SDK C++ 是GHand灵巧手的 C++ 开发包，支持 EtherCAT 、CANFD 和 RS485 通信，提供关节控制、触觉数据采集和实时状态反馈。

## ✨ 功能特点

- 支持 EtherCAT、CANFD 和 RS485 通信
- 5 根手指独立关节控制（位置、速度、力矩）
- 关节状态与触觉数据实时回调
- 支持左手与右手设备
- 通过设备名称匹配自动识别产品类型
- 以 Windows 动态链接库 (DLL) 形式提供

## 📖 官方文档

详细技术规格和 API 参考请查看：[C++ SDK 开发者文档](https://fcnzogxju7xr.feishu.cn/docx/Ex2Gd2i5RoJZzcxtIyPcSAW8nVg)

## 📑 目录

- [功能特点](#-功能特点)
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
- [许可协议](#-许可协议)

## 💻 系统要求

### 支持的平台

| 平台 | 架构 | 状态 | 编译器 |
|------|------|------|--------|
| **Windows** | x64 | 稳定 | Visual Studio 2017+ |
| **Linux** | x64 | 稳定 | GCC 7.5+ (C++11) |

#### Windows
- Windows 7 或更高版本
- Visual Studio 2017 或更高版本
- CMake 3.16 或更高版本

#### Linux
- Ubuntu 20.04 LTS 或更高版本
- GCC 7.5+（支持 C++11）
- CMake 3.16 或更高版本
- libssl-dev

## 🔧 依赖说明

### 必需工具
- CMake 3.16 或更高版本
- 支持 C++11 的编译器

### 已集成的第三方库

源码构建使用 `deps/` 下预构建的依赖。仓库和发布包保持此布局，CMake 只搜索 `deps/`。
Linux 下 `libmodbus.so`、`libmodbus.so.5` 和 `libmodbus.so.5.1.0` 已随 `deps/lib/linux/` 提供，用户无需单独安装或编译 `libmodbus`。

| 依赖 | 版本 | 用途 | 许可证 | GHand 当前使用方式 |
| --- | --- | --- | --- | --- |
| [SOEM](https://github.com/OpenEtherCATsociety/SOEM) | 2.0.0 | EtherCAT Master | GPLv3 / Commercial 双授权 | 本开源版本采用 GPLv3 路线 |
| [libmodbus](https://github.com/stephane/libmodbus) | 3.1.12 | Modbus RTU/RS485 支持 | LGPL-2.1-or-later | 动态链接 |
| [nlohmann/json](https://github.com/nlohmann/json) | 随附头文件 | JSON 解析 | MIT | Header-only |
| WinPcap | Developer Pack components | Windows EtherCAT 数据包捕获 | WinPcap BSD-style license and incorporated notices | 头文件和导入库 |

### 系统库（仅 Linux）
- libssl-dev
- pthreads

## 📦 安装

### 预编译库

将以下文件复制到你的项目中：

| 文件 | 说明 |
|------|------|
| `include/ghand/` | 公共头文件 |
| `lib/ghand.dll` / `libghand.so` | 动态链接库 |
| `config/ghand5.json`, `config/ghandlite1.json` | 产品配置文件 |

链接 `ghand` 库，并确保运行时能访问 JSON 配置文件。

### CMake 安装

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --install build --prefix /path/to/install
```

然后在你的 `CMakeLists.txt` 中：

```cmake
find_library(GHAND_LIB ghand PATHS /path/to/install/lib)
target_link_libraries(your_target PRIVATE ${GHAND_LIB})
target_include_directories(your_target PRIVATE /path/to/install/include)
```

CMake 安装同时导出标准 package target：

```cmake
find_package(GHandSDK CONFIG REQUIRED)
target_link_libraries(your_target PRIVATE GHand::ghand)
```

## 🚀 快速开始

```cpp
#include "ghand/ghand.h"

int main() {
    // 指定产品类型
    auto hand = ghand::GHand::Create(ghand::ProductType::GHand5,
                                       ghand::CommType::ETHERCAT);

    // 或自动识别（连接后根据设备名匹配配置）
    // auto hand = ghand::GHand::Create(ghand::ProductType::AUTO,
    //                                    ghand::CommType::ETHERCAT);

    if (!hand || !hand->Connect("auto")) {
        printf("连接失败\n");
        return -1;
    }

    // 拉取模式：主动查询最新缓存
    auto state   = hand->GetHandData();     // 手部状态
    auto joints  = hand->GetJointsData();   // 关节数据
    auto tactile = hand->GetTactileData();  // 触觉数据

    // 推送模式：注册回调
    hand->SetJointsCallback([](const std::vector<ghand::Joint>& joints) {
        for (const auto& j : joints) {
            printf("关节 %d: %.1f°\n", static_cast<int>(j.id), j.angle);
        }
    });

    // 控制关节
    std::vector<ghand::JointCommand> cmds = {
        {ghand::JointId::THUMB_MCP, 45.0f, 50, 50},
        {ghand::JointId::FF_MCP,    30.0f, 50, 50},
    };
    hand->MoveJoints(cmds);

    hand->Disconnect();
}
```

### RS485 示例

```cpp
auto hand = ghand::GHand::Create(ghand::ProductType::GHand5,
                                   ghand::CommType::RS485);
auto adapters = hand->SearchAdapters();
if (!adapters.empty() && hand->Connect(adapters.begin()->first)) {
    auto info = hand->GetDeviceInfo();
    printf("设备: %s\n", info.device_name.c_str());
    hand->Disconnect();
}
```

## 🔨 源码编译

从包含 `deps/` 的仓库或发布包根目录构建。

### Windows

```bash
cmake -S . -B build
cmake --build build --config Release

# 运行示例
.\build\examples\Release\basic_connection.exe
```

> **Windows RS485 使用提示：** 构建时会将 `deps/lib/windows` 中的 `modbus.dll` 复制到 `ghand.dll` 所在目录。

### Linux

```bash
# 安装依赖
sudo apt install -y cmake build-essential pkg-config libssl-dev

mkdir -p build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# 为所有已编译示例授予原始套接字权限
find build/examples -maxdepth 1 -type f -executable \
  -exec sudo setcap cap_net_raw,cap_net_admin+eip {} +

# 运行 EtherCAT 示例
./build/examples/basic_connection
```

### Linux RS485 / CANFD 运行

RS485 和 CANFD 都走 USB 串口设备，不需要 `setcap`；只有 EtherCAT 需要原始套接字权限。

```bash
# 查看串口设备
ls /dev/ttyUSB* /dev/ttyACM* 2>/dev/null
ls -l /dev/serial/by-id 2>/dev/null

# 如果当前用户没有串口权限，加入 dialout 后重新登录
sudo usermod -aG dialout $USER

# 让当前终端会话生效，也可以直接重新登录
newgrp dialout
```

RS485 调试时建议显式指定 USB-RS485 转接器，常见设备名是 `/dev/ttyUSB0` 或 `/dev/serial/by-id/usb-1a86_USB_Serial-if00-port0`：

```cpp
auto rs485_hand = ghand::GHand::Create(ghand::ProductType::GHand5,
                                         ghand::CommType::RS485);
rs485_hand->Connect("/dev/ttyUSB0");
```

CANFD 使用 ZQWL USB-CDC 适配器，通道号写在设备名后面，例如 `:0`：

```cpp
auto canfd_hand = ghand::GHand::Create(ghand::ProductType::GHand5,
                                         ghand::CommType::CANFD);
canfd_hand->Connect("/dev/ttyACM0:0");
```

`Connect()` 接受可选的 RS485/CANFD 从站 ID 和波特率档位参数：

```cpp
// slave_id = 0 时扫描默认 ID 0x31 和 0x32。
hand->Connect("auto", 0, 0x05);

// 为本连接指定显式的从站 ID 和波特率档位。
hand->Connect("COM17:0", 0x31, 0x05);
```

`SetSlaveId()` 将新的 RS485/CANFD 从站 ID 写入保持寄存器 `0x0000`。
`SetBaudrateConfig()` 将波特率档位写入保持寄存器 `0x002C`；
设备会保存该配置，并在下次上电后生效。

RS485 波特率档位：

| 档位 | 波特率 |
| --- | --- |
| `0x00` | 57,600 bps |
| `0x01` | 115,200 bps |
| `0x02` | 230,400 bps |
| `0x03` | 460,800 bps |
| `0x04` | 921,600 bps |
| `0x05` | 1,000,000 bps |

CANFD 波特率档位：

| 档位 | 仲裁段 | 数据段 |
| --- | --- | --- |
| `0x00` | 500 Kbps，80% 采样点 | 1 Mbps，75% 采样点 |
| `0x01` | 500 Kbps，80% 采样点 | 2 Mbps，80% 采样点 |
| `0x02` | 500 Kbps，80% 采样点 | 4 Mbps，80% 采样点 |
| `0x03` | 500 Kbps，80% 采样点 | 5 Mbps，75% 采样点 |
| `0x04` | 1 Mbps，75% 采样点 | 4 Mbps，80% 采样点 |
| `0x05` | 1 Mbps，75% 采样点 | 5 Mbps，75% 采样点 |

## 📁 目录结构

```
ghand-sdk-cpp/
├── include/ghand/    # 公共 API 头文件
├── src/              # 源码实现
│   ├── comm/         # 通信层（EtherCAT、CANFD）
│   └── internal/     # 内部状态机与配置
├── config/           # 产品配置文件（JSON）
├── examples/         # 教程与示例程序
├── deps/             # 发布包预构建依赖
└── lib/              # 预编译库
```

## 🌐 开源与生态资源

- **GLI 开源中心**：[GLI GitHub 组织](https://github.com/gli-sdk)
- **官方文档**：[GHand 灵巧手文档](https://fcnzogxju7xr.feishu.cn/docx/AhZ6ds2iCoguaAxIzBxciYHinNo)
- **Python SDK**：[GHand Python SDK](https://github.com/gli-sdk/GHand-Python-SDK)

## 📋 更新日志

详见 [CHANGELOG.md](CHANGELOG.md) 了解详细的变更历史。

## 📞 支持与反馈

- 📋 **技术支持：** 如遇项目相关问题，请在本仓库提交 `Issue`。
- 📧 **一般咨询：** [support@glitech.com](mailto:support@glitech.com)

## 📄 许可协议

GHand C++ SDK 采用 GNU General Public License Version 3
（GPL-3.0-only）发布。

EtherCAT 通信后端使用 SOEM 2.0.0。SOEM 2.0.0 采用 GPLv3 / Commercial
双授权，本开源发布版本选择 GPLv3 授权路径。

所有第三方组件仍分别受其原始许可证约束。详细信息请参阅
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) 和 `LICENSES/` 目录。

Copyright (c) 2026 GLITech.
