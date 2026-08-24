# GHand SDK C++

[![Version](https://img.shields.io/badge/version-v2.2.1-blue.svg)](include/ghand/version.h)
[![License](https://img.shields.io/badge/license-GPL--3.0-blue.svg)](LICENSE)
[![C++](https://img.shields.io/badge/C%2B%2B-11-blue.svg)](https://en.cppreference.com/w/cpp/11)
[![CMake](https://img.shields.io/badge/CMake-3.16+-green.svg)](https://cmake.org/)

[English](README.md)

GHand C++ SDK 用于控制 GHand 灵巧手，支持 EtherCAT、CAN-FD 和 RS-485 通信，提供关节控制、触觉数据读取、设备信息查询和实时状态回调。

## 目录

- [主要功能](#主要功能)
- [系统要求](#系统要求)
- [依赖说明](#依赖说明)
- [安装](#安装)
- [快速开始](#快速开始)
- [源码构建](#源码构建)
- [Linux 运行说明](#linux-运行说明)
- [项目结构](#项目结构)
- [许可证](#许可证)

## 主要功能

- EtherCAT、CAN-FD、RS-485 通信接入。
- 支持 GHand5、GHandLite1 等产品配置。
- 支持位置、速度、力矩等关节控制模式。
- 支持关节状态、手部状态和触觉数据读取。
- 支持回调方式接收实时数据。
- 支持从设备名称自动匹配产品配置。

## 系统要求

| 平台 | 架构 | 编译器 |
| --- | --- | --- |
| Windows | x64 | Visual Studio 2017 或更高版本 |
| Linux | x86_64 | GCC 7.5 或更高版本 |

通用要求：

- CMake 3.16 或更高版本
- C++11 兼容编译器

Linux 依赖：

```bash
sudo apt install -y cmake build-essential pkg-config libssl-dev libmodbus-dev
```

## 依赖说明

源码仓库保留必要的第三方构建依赖，但 Linux `libmodbus.so*` 不再随源码仓库分发。

| 依赖 | 版本/来源 | 许可证 | 使用方式 |
| --- | --- | --- | --- |
| SOEM | 官方 v2.0.0 | GPLv3 / Commercial 双许可 | 静态链接，使用 GPLv3 路线 |
| libmodbus | Linux 系统包；Windows 发布包按实际 DLL 说明 | LGPL-2.1-or-later | 动态链接 |
| nlohmann/json | 随附头文件 | MIT | Header-only |
| WinPcap | Windows Developer Pack 组件 | WinPcap 许可证及附带声明 | Windows EtherCAT 构建依赖 |

SOEM 的构建来源记录见 [deps/SOEM_BUILD_INFO.md](deps/SOEM_BUILD_INFO.md)。第三方许可证和声明见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) 与 `LICENSES/` 目录。

## 安装

### CMake 安装

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --install build --prefix /path/to/install
```

在使用方项目中：

```cmake
find_package(GHandSDK CONFIG REQUIRED)
target_link_libraries(your_target PRIVATE GHand::ghand)
```

也可以手动链接：

```cmake
find_library(GHAND_LIB ghand PATHS /path/to/install/lib)
target_include_directories(your_target PRIVATE /path/to/install/include)
target_link_libraries(your_target PRIVATE ${GHAND_LIB})
```

## 快速开始

```cpp
#include "ghand/ghand.h"

int main() {
    auto hand = ghand::GHand::Create(ghand::ProductType::GHand5,
                                     ghand::CommType::ETHERCAT);

    if (!hand || !hand->Connect("auto")) {
        return -1;
    }

    auto info = hand->GetDeviceInfo();
    auto joints = hand->GetJointsData();

    hand->Disconnect();
    return 0;
}
```

RS-485 示例：

```cpp
auto hand = ghand::GHand::Create(ghand::ProductType::GHand5,
                                 ghand::CommType::RS485);
hand->Connect("/dev/ttyUSB0", 0x31,
              ghand::RS485BaudRate::RATE_1000000);
```

CAN-FD 示例：

```cpp
auto hand = ghand::GHand::Create(ghand::ProductType::GHand5,
                                 ghand::CommType::CANFD);
hand->Connect("/dev/ttyACM0:0", 0x31,
              ghand::CANFDBitTiming::TIMING_1M_5M);
```

## 源码构建

### Windows

```bash
cmake -S . -B build
cmake --build build --config Release
```

Windows 构建会在存在 `deps/lib/windows/modbus.dll` 时将其复制到 `ghand.dll` 所在目录。Windows 二进制发布包如果包含 libmodbus DLL，必须同时提供对应许可证、版本和源码获取信息。

### Linux

```bash
sudo apt install -y cmake build-essential pkg-config libssl-dev libmodbus-dev

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

Linux 下 CMake 通过 pkg-config 查找系统 `libmodbus`，不会从 `deps/lib/linux` 链接 `libmodbus.so*`。

## Linux 运行说明

EtherCAT 需要 raw socket 权限。可以为示例程序授予能力：

```bash
find build/examples -maxdepth 1 -type f -executable \
  -exec sudo setcap cap_net_raw,cap_net_admin+eip {} +
  -exec sudo setcap cap_net_raw,cap_net_admin=eip {} \;

# 第二步：运行 EtherCAT 示例
./build/examples/basic_connection
```

RS-485 和 CAN-FD 使用 USB 串口设备，不需要 raw socket 权限。如果当前用户无法打开串口，请加入 `dialout` 组后重新登录：

```bash
sudo usermod -aG dialout $USER
```

正式安装后可用 `ldd` 检查运行时依赖，确认 `libmodbus.so.*` 解析到系统路径且没有 `not found`。

## 项目结构

```text
GHand-Cpp-SDK/
|-- include/ghand/              # 公共 API 头文件
|-- src/                        # SDK 实现
|   |-- comm/                   # 通信层
|   `-- internal/               # 内部实现
|-- config/                     # 产品 JSON 配置
|-- examples/                   # 示例程序
|-- deps/                       # 第三方构建依赖与 SOEM 来源记录
|-- LICENSES/                   # 第三方许可证文本
|-- THIRD_PARTY_NOTICES.md      # 第三方依赖声明
|-- README.md                   # 英文说明
|-- README.zh.md                # 中文说明
`-- CHANGELOG.md                # 更新日志
```

## 许可证

GHand C++ SDK 基于 [GPL-3.0-only](LICENSE) 发布。

第三方组件保留各自许可证。SOEM 使用 GPLv3 路线静态链接；libmodbus 以 LGPL-2.1-or-later 动态链接，Linux 使用系统依赖，Windows 二进制包按实际分发内容提供对应声明。
