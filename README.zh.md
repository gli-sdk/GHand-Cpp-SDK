# GHand SDK C++

[![Version](https://img.shields.io/badge/version-V3.1.0-blue.svg)](include/ghand/version.h)
[![License](https://img.shields.io/badge/license-GPL--3.0-blue.svg)](LICENSE)

[![C++](https://img.shields.io/badge/C%2B%2B-11-blue.svg)](https://en.cppreference.com/w/cpp/11)
[![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux-lightgrey.svg)]()
[![CMake](https://img.shields.io/badge/CMake-3.16+-green.svg)](https://cmake.org/)

[English](README.md)

GHand C++ SDK 用于控制 GHand 灵巧手，支持 EtherCAT、CANFD 和 RS485 通信，提供关节控制、触觉感知与实时状态反馈。

## ✨ 主要功能

- EtherCAT、CANFD 和 RS485 通信
- 全部 5 根手指支持逐关节独立控制（位置、速度、电流、MIT、自定义模式）
- MIT 与自定义模式教程，包含状态反馈与退出前显式禁用驱动
- 可选 Qt 6 触觉显示界面，对齐 Python 的整手视图、触点图和 60 秒三轴力曲线
- 关节状态与触觉数据的实时数据回调
- 支持左手和右手设备
- 通过设备名称匹配自动识别产品
- 支持同款左右手共用一条总线，控制与反馈按手隔离
- 提供 Windows DLL 与 Linux 共享库构建

V3.1.0 使用 `Master` / `GHand` 所有权模型与新协议命令类型。
从 2.x 升级的应用需要使用匹配的头文件和库重新编译；API 与协议迁移说明见 [CHANGELOG.md](CHANGELOG.md)。

## 📖 官方文档

详细技术规格与 API 参考，请访问：[C++ SDK 开发者文档](https://fcnzogxju7xr.feishu.cn/docx/Ex2Gd2i5RoJZzcxtIyPcSAW8nVg)

## 📑 目录

- [主要功能](#-主要功能)
- [官方文档](#-官方文档)
- [系统要求](#-系统要求)
- [依赖](#-依赖)
- [安装](#-安装)
- [快速开始](#-快速开始)
- [示例程序](#-示例程序)
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
| `ghand.dll` / `libghand.so` | 共享库 |
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
find_package(GHandSDK 3 CONFIG REQUIRED)
target_link_libraries(your_target PRIVATE GHand::ghand)
```

## 🚀 快速开始

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

### RS485 示例

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

### 公共类型迁移

公共类型参考 Python SDK：`HandData`、`JointData`、`HandControlMode`、
`JointControlMode`、`JointLoopType`、`HandRuntimeState` 和 `JointState`。
左右手使用 `HandType::LEFT_HAND` / `RIGHT_HAND`，速度模式使用 `HandControlMode::VELOCITY`。
`JointCommand` 使用 `control_mode`、`loop_type`、`control_state`，默认分别为
POSITION、CLOSED_LOOP、DISABLED；运动时需显式指定 RUNNING。
`HandData::joints_state` 替代 `state`。触觉回调接收按 `TactileSensorId` 索引的
`std::map<TactileSensorId, TactileData>`，每指 `TactileData` 的力数据为牛顿单位的展平三元组，空向量表示数据不可用。
手部错误标志支持 `|` 和 `&`，完整保留高于第 63 位的故障标志。
RS485 保留 `RATE_*` 命名，避免与 Windows 的 `BAUD_*` 宏冲突。

## 🧪 示例程序

教程源码位于 `examples/tutorial/`，按学习顺序编号。
可执行文件名不含数字前缀，由 `examples/CMakeLists.txt` 定义。

| 源码 | 可执行文件 | 内容 |
| --- | --- | --- |
| [01_basic_connection.cc](examples/tutorial/01_basic_connection.cc) | `basic_connection` | 连接与设备信息查询 |
| [02_move_joints.cc](examples/tutorial/02_move_joints.cc) | `move_joints` | 位置指令、参数单位、控制模式与关节反馈 |
| [03_speed_control.cc](examples/tutorial/03_speed_control.cc) | `speed_control` | 速度控制 |
| [04_current_control.cc](examples/tutorial/04_current_control.cc) | `current_control` | 电流控制 |
| [05_tactile_callback.cc](examples/tutorial/05_tactile_callback.cc) | `tactile_callback` | 触觉反馈回调 |
| [06_multi_hand.cc](examples/tutorial/06_multi_hand.cc) | `multi_hand` | 多手控制 |
| [07_glove_control.cc](examples/tutorial/07_glove_control.cc) | `glove_control` | 手套控制 |
| [08_interactive_joint_control.cc](examples/tutorial/08_interactive_joint_control.cc) | `interactive_joint_control` | 交互式关节指令与反馈 |
| [09_hand_state.cc](examples/tutorial/09_hand_state.cc) | `hand_state` | 手状态与关节反馈 |
| [10_switch_baud_rate.cc](examples/tutorial/10_switch_baud_rate.cc) | `switch_baud_rate` | CANFD 位时序配置 |
| [11_connect_hands.cc](examples/tutorial/11_connect_hands.cc) | `connect_hands` | 共总线发现、总线级与单手指令 |
| [13_mit_control.cc](examples/tutorial/13_mit_control.cc) | `mit_control` | MIT 柔顺闭合、触觉接触检测、保持和释放 |
| [14_custom_control.cc](examples/tutorial/14_custom_control.cc) | `custom_control` | 分指选择位置/电流模式、组内模式一致性和状态反馈 |
| [15_tactile_display.cc](examples/tutorial/15_tactile_display.cc) | `tactile_display` | 可选 Qt 6 五指触觉点图、切向力箭头与三轴力曲线 |

新增教程沿用 Python SDK 的 13/14/15 编号。MIT 的角度单位为度，速度/电流为百分比；
`kp=10..2000`、`kd=0..50` 且 `kp>kd`。MIT 电流是前馈目标。
CUSTOM 同一手指仲裁组内的控制模式和反馈环必须一致，禁用关节也应显式选择同组模式。
发送成功后应检查状态反馈；断开通信前显式调用 `DisableHand()` 释放驱动力。

### Qt 触觉显示

安装 Qt 6.2 或更高版本的 Widgets 开发组件后，启用可选构建（仅界面目标使用 C++17）：

```powershell
cmake -S . -B build -DGHAND_BUILD_TACTILE_DISPLAY=ON -DCMAKE_PREFIX_PATH=C:/Qt/6.9.1/msvc2022_64
cmake --build build --config Release --target tactile_display mit_control custom_control
C:/Qt/6.9.1/msvc2022_64/bin/windeployqt.exe build/examples/Release/tactile_display.exe
./build/examples/Release/tactile_display.exe --preview
```

Linux 将 `CMAKE_PREFIX_PATH` 换为本机 Qt 路径，使用 `./build/examples/tactile_display --preview`。
省略 `--preview` 会连接 GHand5 EtherCAT（`auto`）；其他通信方式请修改教程中的创建/连接参数。
窗口打开后使用 Tactile 开关开始采集，卸载传感器负载后点击 Zero Tactile 归零。
界面沿用 Python 的 1600×1000 黑色布局：左侧显示整手图与底部五指触点图，右侧纵向排列五组 Fx/Fy/Fz 曲线。
根据连接设备自动选择左右手图片；38/31/31/31/31 个触点的位置、图片叠加偏移、法向力渐变与光晕、
切向力箭头和整手 Fz 数值条均按 Python 实现。悬停触点可查看三轴力值。
曲线使用单调时钟记录 60 秒历史，50 Hz 时间桶合并回调，缺测超过 0.5 秒插入断点。
资源通过 Qt Resource 嵌入，无需另外复制图片，也不依赖 Python 仓库。

可复用窗口位于 [src/tactile_display/](src/tactile_display/)，链接目标为 `ghand_tactile_display`。
调用方拥有连接并保证 `GHand` 比窗口存活更久；窗口独占该手的触觉回调槽，关闭窗口会注销回调、
关闭触觉并回收工作线程，连接由调用方断开。界面不会发送运动指令。
默认构建不依赖 Qt。无硬件验证可启用 `GHAND_BUILD_TESTS` 后运行 `ctest --test-dir build -C Release --output-on-failure`；
Windows 运行测试前需将 Qt 的 `bin` 加入 `PATH` 并设置 `QT_QPA_PLATFORM_PLUGIN_PATH` 指向 Qt 的 `plugins`。

`preset_gesture`、`gesture_dance`、`clap_pose` 等动作示例位于
[`examples/demo/`](examples/demo/)。

构建后，可向 `connect_hands` 传入通信类型和可选设备名：

```bash
./build/examples/connect_hands ethcat auto
./build/examples/connect_hands canfd /dev/ttyACM0:0
./build/examples/connect_hands rs485 /dev/ttyUSB0
```

Windows 下使用 `build\examples\Release\connect_hands.exe`，设备名可使用
`COM17:0`（CANFD）或 `COM7`（RS485）。该示例会驱动已连接的手运动；
使用 `--help` 查看参数和支持的拓扑。

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
auto rs485_master = ghand::Master::Create(ghand::ProductType::GHand5,
                                         ghand::CommType::RS485);
rs485_master->Connect("/dev/ttyUSB0");
```

CANFD 请使用 ZQWL USB-CDC 适配器，并在设备名后追加通道号，例如 `:0`：

```cpp
auto canfd_master = ghand::Master::Create(ghand::ProductType::GHand5,
                                         ghand::CommType::CANFD);
canfd_master->Connect("/dev/ttyACM0:0");
```

`Connect()` 支持可选的 RS485/CANFD 从站 ID 与波特率（或位时序）参数：

```cpp
// RS485：slave_id = 0 时扫描默认 ID 0x31 和 0x32。
master->Connect("auto", 0, ghand::RS485BaudRate::RATE_1000000);

// CAN-FD：使用预定义位时序配置。
master->Connect("COM17:0", 0x31, ghand::CANFDBitTiming::TIMING_1M_5M);
```

`SetSlaveId()` 将新的 RS485/CANFD 从站 ID 写入保持寄存器 `0x0003`。
`SetBaudrateConfig()` 将枚举映射的协议挡位写入保持寄存器
`0x0004`；设备保存该值并在下次上电后生效。

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
│   ├── comm/         # 通信层（EtherCAT、CANFD、RS485）
│   └── internal/     # 内部状态机与配置
├── config/           # 产品配置（JSON）
├── cmake/            # CMake 包配置模板
├── examples/         # 教程与示例程序
└── deps/             # 发布包预构建依赖
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
