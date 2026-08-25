# GHand SDK C++

[![Version](https://img.shields.io/badge/version-v2.2.1-blue.svg)](include/ghand/version.h)
[![License](https://img.shields.io/badge/license-GPL--3.0-blue.svg)](LICENSE)

[![C++](https://img.shields.io/badge/C%2B%2B-11-blue.svg)](https://en.cppreference.com/w/cpp/11)
[![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux-lightgrey.svg)]()
[![CMake](https://img.shields.io/badge/CMake-3.16+-green.svg)](https://cmake.org/)

[English](README.md)

GHand C++ SDK 用于控制 GHand 灵巧手，支持 EtherCAT、CANFD 和 RS485 通信，提供关节控制、触觉感知与实时状态反馈。

## ✨ 主要功能

- EtherCAT、CANFD 和 RS485 通信
- 全部 5 根手指支持逐关节独立控制（位置、速度、力矩）
- 关节状态与触觉数据的实时数据回调
- 支持左手和右手设备
- 通过设备名称匹配自动识别产品
- 以 Windows 动态链接库（DLL）形式提供

## 📖 官方文档

详细技术规格与 API 参考，请访问：[C++ SDK 开发者文档](https://fcnzogxju7xr.feishu.cn/docx/Ex2Gd2i5RoJZzcxtIyPcSAW8nVg)

## 📑 目录

- [主要功能](#-主要功能)
- [官方文档](#-官方文档)
- [系统要求](#-系统要求)
- [依赖](#-依赖)
- [安装](#-安装)
- [快速开始](#-快速开始)
- [源码构建](#-源码构建)
- [项目结构](#-项目结构)
- [开源与生态资源](#-开源与生态资源)
- [更新日志](#-更新日志)
- [支持与反馈](#-支持与反馈)
- [许可证](#-许可证)

## 💻 系统要求

### 支持平台

| 平台 | 架构 | 状态 | 编译器 |
|----------|------|--------|----------|
| **Windows** | x64 | 稳定 | Visual Studio 2017+ |
| **Linux** | x64 | 稳定 | GCC 7.5+（C++11） |

#### Windows
- Windows 7 或更高版本
- Visual Studio 2017 或更高版本
- CMake 3.16 或更高版本

#### Linux
- Ubuntu 20.04 LTS 或更高版本
- GCC 7.5+（C++11 支持）
- CMake 3.16 或更高版本
- libssl-dev
- libmodbus-dev

## 🔧 依赖

### 必需工具
- CMake 3.16 或更高版本
- C++11 兼容编译器

### 发布依赖

源码构建使用 `deps/` 下随附的 SOEM 和仅头文件依赖。
Linux 构建通过 pkg-config 使用系统 `libmodbus` 开发包；`libmodbus.so*` 不随源码仓库分发。Windows
构建使用 Windows 发布依赖集中提供的 libmodbus 导入库/DLL。

| 依赖库 | 版本 | 用途 | 许可证 | GHand 中的使用方式 |
|---------|---------|---------|---------|----------------|
| [SOEM](https://github.com/OpenEtherCATsociety/SOEM) | 2.0.0 | EtherCAT 主站协议栈 | GPL-3.0 / 商业双许可 | 本开源发行版使用 GPL-3.0 选项 |
| [libmodbus](https://github.com/stephane/libmodbus) | 3.1.12 | Modbus RTU/RS485 支持 | LGPL-2.1-or-later | 动态链接 |
| [nlohmann/json](https://github.com/nlohmann/json) | 随附头文件 | JSON 解析 | MIT | Header-only |
| WinPcap | Developer Pack 组件 | 数据包捕获（Windows EtherCAT） | WinPcap BSD 风格许可证及附带声明 | 头文件与导入库 |

### 系统库（仅 Linux）
- libssl-dev
- libmodbus-dev
- pkg-config
- pthreads

## 📦 安装

### 预构建库

将以下构件复制到你的项目中：

| 构件 | 说明 |
|----------|-------------|
| `include/ghand/` | 公共头文件 |
| `lib/ghand.dll` / `libghand.so` | 共享库 |
| `config/ghand5.json`、`config/ghandlite1.json` | 产品配置 |

链接 `ghand`，并确保运行时可以访问 JSON 配置。

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

安装还会导出一个 CMake 包目标：

```cmake
find_package(GHandSDK CONFIG REQUIRED)
target_link_libraries(your_target PRIVATE GHand::ghand)
```

## 🚀 快速开始

```cpp
#include "ghand/ghand.h"

int main() {
    // 显式指定产品类型
    auto hand = ghand::GHand::Create(ghand::ProductType::GHand5,
                                       ghand::CommType::ETHERCAT);

    // 或自动识别（连接后按设备名称匹配配置）
    // auto hand = ghand::GHand::Create(ghand::ProductType::AUTO,
    //                                    ghand::CommType::ETHERCAT);

    if (!hand || !hand->Connect("auto")) {
        printf("Connection failed\n");
        return -1;
    }

    // 拉取模式：查询最新缓存数据
    auto state   = hand->GetHandData();     // 手部状态
    auto joints  = hand->GetJointsData();   // 关节数据
    auto tactile = hand->GetTactileData();  // 触觉数据

    // 推送模式：注册回调
    hand->SetJointsCallback([](const std::vector<ghand::Joint>& joints) {
        for (const auto& j : joints) {
            printf("Joint %d: %.1f deg\n", static_cast<int>(j.id), j.angle);
        }
    });

    // 控制关节
    std::vector<ghand::JointCommand> cmds = {
        {ghand::JointId::FF_PIP, 45.0f, 50, 50},
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
    printf("Device: %s\n", info.device_name.c_str());
    hand->Disconnect();
}
```

## 🔨 源码构建

在包含 `deps/` 的仓库或发布包根目录下构建。

### Windows

```bash
cmake -S . -B build
cmake --build build --config Release

# 运行示例
.\build\examples\Release\basic_connection.exe
```

> **Windows RS485：** 构建时若存在 `deps/lib/windows` 下的 `modbus.dll`，会将其复制到 `ghand.dll` 旁边。

### Linux

```bash
# 安装依赖
sudo apt install -y cmake build-essential pkg-config libssl-dev libmodbus-dev

mkdir -p build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# 为所有构建出的示例授予一次 raw socket 权限
find build/examples -maxdepth 1 -type f -executable \
  -exec sudo setcap cap_net_raw,cap_net_admin=eip {} \;

# 运行 EtherCAT 示例
./build/examples/basic_connection
```

### Linux RS485 / CANFD

RS485 和 CANFD 都使用 USB 串口设备，无需 `setcap`；raw socket 权限仅 EtherCAT 需要。

```bash
# 查看串口设备
ls /dev/ttyUSB* /dev/ttyACM* 2>/dev/null
ls -l /dev/serial/by-id 2>/dev/null

# 如果当前用户无法打开串口，加入 dialout 组后重新登录
sudo usermod -aG dialout $USER

# 让当前终端会话立即生效，或重新登录
newgrp dialout
```

RS485 调试时请显式指定 USB-RS485 适配器。常见设备名为 `/dev/ttyUSB0` 或 `/dev/serial/by-id/usb-1a86_USB_Serial-if00-port0`：

```cpp
auto rs485_hand = ghand::GHand::Create(ghand::ProductType::GHand5,
                                         ghand::CommType::RS485);
rs485_hand->Connect("/dev/ttyUSB0");
```

CANFD 请使用 ZQWL USB-CDC 适配器，并在设备名后追加通道号，例如 `:0`：

```cpp
auto canfd_hand = ghand::GHand::Create(ghand::ProductType::GHand5,
                                         ghand::CommType::CANFD);
canfd_hand->Connect("/dev/ttyACM0:0");
```

`Connect()` 支持可选的 RS485/CANFD 从站 ID 与波特率（或位时序）参数：

```cpp
// RS485：slave_id = 0 时扫描默认 ID 0x31 和 0x32。
hand->Connect("auto", 0, ghand::RS485BaudRate::RATE_1000000);

// CAN-FD：使用预定义位时序配置。
hand->Connect("COM17:0", 0x31, ghand::CANFDBitTiming::TIMING_1M_5M);
```

`SetSlaveId()` 将新的 RS485/CANFD 从站 ID 写入保持寄存器 `0x0000`。
`SetBaudrateConfig()` 将枚举映射的协议挡位写入保持寄存器
`0x002C`；设备保存该值并在下次上电后生效。

```cpp
hand->SetBaudrateConfig(ghand::RS485BaudRate::RATE_1000000);
hand->SetBaudrateConfig(ghand::CANFDBitTiming::TIMING_1M_5M);
```

RS485 波特率挡位：

| 挡位 | 波特率 |
| --- | --- |
| `0x00` | 57,600 bps |
| `0x01` | 115,200 bps |
| `0x02` | 230,400 bps |
| `0x03` | 460,800 bps |
| `0x04` | 921,600 bps |
| `0x05` | 1,000,000 bps |

CANFD 波特率挡位：

| 挡位 | 仲裁段 | 数据段 |
| --- | --- | --- |
| `0x00` | 500 Kbps，80% 采样点 | 1 Mbps，75% 采样点 |
| `0x01` | 500 Kbps，80% 采样点 | 2 Mbps，80% 采样点 |
| `0x02` | 500 Kbps，80% 采样点 | 4 Mbps，80% 采样点 |
| `0x03` | 500 Kbps，80% 采样点 | 5 Mbps，75% 采样点 |
| `0x04` | 1 Mbps，75% 采样点 | 4 Mbps，80% 采样点 |
| `0x05` | 1 Mbps，75% 采样点 | 5 Mbps，75% 采样点 |

## 📁 项目结构

```
ghand-sdk-cpp/
├── include/ghand/    # 公共 API 头文件
├── src/              # 源码实现
│   ├── comm/         # 通信层（EtherCAT、CANFD）
│   └── internal/     # 内部状态机与配置
├── config/           # 产品配置（JSON）
├── examples/         # 教程与示例程序
├── deps/             # 发布包预构建依赖
└── lib/              # 预编译库
```

## 🌐 开源与生态资源

- **GLI 开源社区**：[GLI GitHub 组织](https://github.com/gli-sdk)
- **官方文档**：[GHand 灵巧手文档](https://fcnzogxju7xr.feishu.cn/docx/AhZ6ds2iCoguaAxIzBxciYHinNo)
- **Python SDK**：[GHand Python SDK](https://github.com/gli-sdk/GHand-Python-SDK)

## 📋 更新日志

详细变更历史请参阅 [CHANGELOG.md](CHANGELOG.md)。

## 📞 支持与反馈

- 📋 **技术支持：** 项目相关问题请在本仓库提交 `Issue`。
- 📧 **一般咨询：** [support@glitech.com](mailto:support@glitech.com)

## 📄 许可证

GHand C++ SDK 基于 GNU General Public License Version 3（GPL-3.0-only）发布。

EtherCAT 后端使用 SOEM 2.0.0。SOEM 2.0.0 采用 GPLv3 或商业许可证双许可。本开源发行版以 GPLv3 选项使用 SOEM。

第三方组件保留各自许可证。详见
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) 与 `LICENSES/` 目录。

Copyright (c) 2026 GLITech.
