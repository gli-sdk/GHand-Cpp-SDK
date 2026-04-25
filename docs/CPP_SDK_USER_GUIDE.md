# G5-13A28D系列 C++ SDK 开发者文档

深圳果力智能科技有限公司

---

## 版本及变更记录

| 版本号 | 子版本 | 发布日期 | 更新内容 | 备注 |
|--------|--------|----------|----------|------|
| V1.0 | V1.0.0 | 2026/03/21 | 创建文档 | - |

---

## 一、灵巧手说明

### 1.1 产品概述

#### SDK 简介

Xiaoyao SDK C++ 是深圳果力智能科技有限公司为 G5-13A28D 系列灵巧手提供的 C++ 开发包，提供完整的机械手控制功能。SDK 采用现代 C++ 设计，支持 Windows 和 Linux 平台，提供类型安全的 API 和高性能的数据回调机制。

#### SDK 主要功能

- **手部整体控制**：支持完整的灵巧手连接、配置和控制
- **关节精细控制**：13 个可控关节，支持角度、速度、力矩参数化控制
- **触觉感知**：支持触觉传感器数据读取，包括合力和分布力
- **实时数据订阅**：基于回调的数据推送机制，实时获取关节状态和手部状态
- **日志系统**：完善的日志系统，支持控制台和文件输出，多级别日志控制
- **多手控制**：支持同时控制多台灵巧手设备

#### 枭尧灵巧手硬件特性

**硬件配置：**
- 5 个手指：拇指（Thumb）、食指（FF）、中指（MF）、无名指（RF）、小指（LF）
- 13 个可控关节（每个手指 2-3 个关节）
- 集成触觉传感器，支持合力和分布力测量

**安全保护：**
- 关节冲突检测
- 指尖冲突检测
- 温度保护（低温/高温）
- 电压保护（低压/高压）
- 电机过流保护

### 1.2 默认单位

C++ SDK 中使用以下单位：

| 参数类型 | 单位 | 说明 |
|---------|------|------|
| 角度（API 内部） | 弧度 | 所有 API 接口使用弧度作为角度单位 |
| 角度（示例代码） | 度 | 示例代码中使用度，方便理解（自动转换） |
| 速度 | 百分比 (0-100%) | 最大速度的百分比 |
| 力矩 | 百分比 (0-100%) | 最大力矩的百分比 |
| 温度 | 摄氏度 (°C) | 手部内部温度 |

### 1.3 关节定义与限位

G5-13A28D 灵巧手具有 13 个可控关节，分布在 5 个手指上：

| 手指 | 关节名称 | 关节 ID 枚举 | 角度范围（度） | 说明 |
|------|---------|-------------|---------------|------|
| **拇指** | | | | |
| | THUMB_DIP | JointId::THUMB_DIP | - | 不可控（自动跳过） |
| | THUMB_PIP | JointId::THUMB_PIP | 0° - 90° | 近端指间关节 |
| | THUMB_MCP | JointId::THUMB_MCP | 0° - 80° | 掌指关节 |
| | THUMB_SWING | JointId::THUMB_SWING | 0° - 60° | 摆动关节 |
| | THUMB_ROTATION | JointId::THUMB_ROTATION | -50° - 50° | 旋转关节 |
| **食指** | | | | |
| | FF_DIP | JointId::FF_DIP | - | 不可控（自动跳过） |
| | FF_PIP | JointId::FF_PIP | 0° - 100° | 近端指间关节 |
| | FF_MCP | JointId::FF_MCP | 0° - 90° | 掌指关节 |
| | FF_SWING | JointId::FF_SWING | -20° - 20° | 摆动关节 |
| **中指** | | | | |
| | MF_DIP | JointId::MF_DIP | - | 不可控（自动跳过） |
| | MF_PIP | JointId::MF_PIP | 0° - 100° | 近端指间关节 |
| | MF_MCP | JointId::MF_MCP | 0° - 90° | 掌指关节 |
| **无名指** | | | | |
| | RF_DIP | JointId::RF_DIP | - | 不可控（自动跳过） |
| | RF_PIP | JointId::RF_PIP | 0° - 100° | 近端指间关节 |
| | RF_MCP | JointId::RF_MCP | 0° - 90° | 掌指关节 |
| **小指** | | | | |
| | LF_DIP | JointId::LF_DIP | - | 不可控（自动跳过） |
| | LF_PIP | JointId::LF_PIP | 0° - 100° | 近端指间关节 |
| | LF_MCP | JointId::LF_MCP | 0° - 90° | 掌指关节 |

**注意：**
- DIP 关节（远端指间关节）不可控，SDK 内部会自动跳过
- 角度值为 0 表示手指完全伸直
- API 内部使用弧度，示例代码中使用度便于理解

### 1.4 状态码与错误代码

#### State（状态枚举）

| 状态值 | 枚举名 | 说明 |
|--------|--------|------|
| 0 | STOPPED | 停止状态 |
| 1 | RUNNING | 正常运行中 |
| 2 | ABNORMAL_RUNNING | 异常运行 |
| 3 | PROTECTIVE_STOPPED | 保护性停止 |

#### ErrorCode（错误码枚举）

| 错误码 | 枚举名 | 类别 | 说明 |
|--------|--------|------|------|
| **电机错误** | | | |
| 1 | MOTOR_OVERCURRENT | 电机 | 电机过流 |
| 2 | ENCODER_ERROR | 电机 | 编码器异常 |
| 3 | MOTOR_COMM_ERROR | 电机 | 电机通信错误 |
| **手指错误** | | | |
| 11 | JOINT_CONFLICT | 手指 | 关节冲突 |
| 12 | TIP_CONFLICT | 手指 | 指尖冲突 |
| **手部错误** | | | |
| 21 | LOW_TEMP | 手部 | 温度过低 |
| 22 | HIGH_TEMP | 手部 | 温度过高 |
| 23 | LOW_VOLTAGE | 手部 | 电压过低 |
| 24 | HIGH_VOLTAGE | 手部 | 电压过高 |
| **触觉传感器错误** | | | |
| 31 | TACTILE_ERROR | 触觉 | 触觉传感器错误 |
| **数据处理错误** | | | |
| 101 | PARAM_ERROR | 数据 | 参数错误 |
| 102 | TIMEOUT | 数据 | 超时 |
| **其他** | | | |
| 201 | UNKNOWN_ERROR | 其他 | 未知错误 |

### 1.5 手部类型与通信方式

#### HandType（手部类型）

| 类型值 | 枚举名 | 说明 |
|--------|--------|------|
| 1 | LEFT | 左手 |
| 2 | RIGHT | 右手 |

#### CommType（通信类型）

| 类型值 | 枚举名 | 说明 | 推荐场景 |
|--------|--------|------|----------|
| 0 | ETHERCAT | EtherCAT 工业以太网 | **推荐**：高实时性、多手控制 |
| 1 | CANFD | CAN FD 总线 | 需要 CAN 总线集成的场景 |
| 2 | RS485 | RS485 串口通信 | 简单应用、成本敏感场景 |

#### ControlMode（控制模式）

| 模式值 | 枚举名 | 说明 |
|--------|--------|------|
| 0 | POSITION | 位置控制模式（默认） |
| 1 | TORQUE | 力矩控制模式 |
| 2 | SPEED | 速度控制模式 |

---

## 二、SDK使用说明

### 2.1 环境准备

#### 2.1.1 Windows 环境配置

**系统要求：**
- Windows 7 或更高版本（推荐 Windows 10/11）
- Visual Studio 2017 或更高版本
- CMake 3.5 或更高版本

**安装步骤：**

1. **安装 Visual Studio**
   - 下载并安装 Visual Studio Community（免费）
   - 确保安装 "使用 C++ 的桌面开发" 工作负载

2. **安装 CMake**
   - 从 [cmake.org](https://cmake.org/download/) 下载并安装 CMake

3. **获取 SDK**
   ```bash
   git clone https://github.com/yourcompany/xiaoyao-sdk-cpp.git
   cd xiaoyao-sdk-cpp
   ```

4. **安装 Npcap（EtherCAT 通信需要）**
   - 下载 Npcap：https://npcap.com/
   - 安装时勾选 "Support raw 802.11 traffic" 选项

#### 2.1.2 Linux 环境配置

**系统要求：**
- Ubuntu 20.04 LTS 或更高版本
- GCC 7.5+（支持 C++11）
- CMake 3.5 或更高版本

**安装步骤：**

1. **安装系统依赖**
   ```bash
   sudo apt update
   sudo apt install -y cmake build-essential pkg-config
   sudo apt install -y libpcap-dev libssl-dev
   ```

2. **配置网络权限（重要！）**

   EtherCAT 通信需要访问网卡，需要 root 权限或配置网络权限：

   ```bash
   # 方法1：使用 sudo 运行程序（推荐用于开发）
   sudo ./your_program

   # 方法2：设置网卡权限（推荐用于部署）
   sudo setcap cap_net_raw,cap_net_admin=eip /path/to/your_program
   ```

3. **克隆项目**
   ```bash
   git clone https://github.com/yourcompany/xiaoyao-sdk-cpp.git
   cd xiaoyao-sdk-cpp
   ```

#### 2.1.3 依赖说明

SDK 依赖以下第三方库（已包含在 SDK 中）：

| 库名称 | 版本 | 用途 | 说明 |
|--------|------|------|------|
| SOEM | 最新 | EtherCAT 通信 | Simple OpenEtherCAT Master |
| WinPcap/libpcap | 最新 | 网络数据包捕获 | Windows 用 WinPcap，Linux 用 libpcap |
| OpenSSL | 最新 | 加密通信 | 可选，用于固件更新 |

用户无需单独安装这些依赖，SDK 已包含预编译版本。

### 2.2 安装流程

#### 步骤一：获取 SDK

```bash
git clone https://github.com/yourcompany/xiaoyao-sdk-cpp.git
cd xiaoyao-sdk-cpp
```

#### 步骤二：配置项目

**Windows:**
```bash
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
```

**Linux:**
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
```

#### 步骤三：编译示例

**Windows:**
```bash
cmake --build . --config Release
```

**Linux:**
```bash
make -j$(nproc)
```

#### 步骤四：验证安装

运行基础连接示例验证 SDK 是否正常工作：

**Windows:**
```bash
.\examples\Release\basic_connection.exe
```

**Linux:**
```bash
sudo ./examples/basic_connection
```

如果输出 "Successfully connected to the dexterous hand!"，说明安装成功。

### 2.3 快速开始

#### 最小可用代码示例

```cpp
#include "xiaoyao/xiaoyao.h"
#include <iostream>

int main() {
    using namespace xiaoyao;

    // 1. 创建灵巧手实例
    DexHand hand(CommType::ETHERCAT);

    // 2. 连接设备（自动搜索）
    if (!hand.AutoConnect()) {
        std::cerr << "连接失败！" << std::endl;
        return 1;
    }

    std::cout << "成功连接！" << std::endl;

    // 3. 控制关节
    std::vector<JointCommand> joints = {
        {JointId::FF_MCP, 45.0f * M_PI / 180.0f, 50, 50}  // 食指弯曲 45 度
    };
    hand.MoveJoints(joints);

    // 4. 断开连接
    hand.Disconnect();
    return 0;
}
```

#### 完整示例

```cpp
#include "xiaoyao/xiaoyao.h"
#include "xiaoyao/logging.h"
#include <iostream>
#include <thread>
#include <chrono>

using namespace xiaoyao;

int main() {
    // 配置日志级别
    ConfigureConsole(LogLevel::INFO);

    LOG_INFO("灵巧手 SDK 示例程序启动");

    // 创建实例
    DexHand hand(CommType::ETHERCAT);

    // 连接设备
    LOG_INFO("正在连接设备...");
    if (!hand.AutoConnect()) {
        LOG_ERROR("连接失败！");
        return 1;
    }

    LOG_INFO("连接成功");

    // 获取设备信息
    DeviceInfo info = hand.GetDeviceInfo();
    LOG_INFO("设备名称: " << info.device_name);
    LOG_INFO("硬件版本: " << info.hardware_version);
    LOG_INFO("软件版本: " << info.software_version);

    // 设置控制模式
    hand.SetControlMode(ControlMode::POSITION);

    // 注册关节数据回调
    hand.SetJointsCallback([](const std::vector<Joint>& joints) {
        static int count = 0;
        if (count++ % 10 == 0) {  // 每 10 次输出一次
            LOG_INFO("收到关节数据，共 " << joints.size() << " 个关节");
        }
    });

    // 控制关节
    std::vector<JointCommand> joints = {
        {JointId::FF_PIP, 45.0f * M_PI / 180.0f, 80, 80},
        {JointId::FF_MCP, 30.0f * M_PI / 180.0f, 80, 80}
    };

    LOG_INFO("发送关节命令...");
    hand.MoveJoints(joints);

    // 等待动作完成
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // 复位
    LOG_INFO("复位关节...");
    joints.clear();
    joints.push_back({JointId::FF_PIP, 0.0f, 80, 80});
    joints.push_back({JointId::FF_MCP, 0.0f, 80, 80});
    hand.MoveJoints(joints);

    std::this_thread::sleep_for(std::chrono::seconds(1));

    // 断开连接
    LOG_INFO("断开连接...");
    hand.Disconnect();

    LOG_INFO("程序结束");
    return 0;
}
```

### 2.4 接口说明

#### 2.4.1 设备管理

##### `Connect()` - 连接到指定设备

```cpp
bool Connect(const std::string& device_name = "auto");
```

**参数：**
- `device_name`: 设备名称，"auto" 表示自动搜索

**返回值：**
- 成功返回 `true`，失败返回 `false`

**示例：**
```cpp
DexHand hand(CommType::ETHERCAT);

// 自动连接
if (hand.Connect("auto")) {
    // 连接成功
}

// 指定网卡连接
if (hand.Connect("\\Device\\NPF_{GUID}")) {
    // 连接成功
}
```

---

##### `AutoConnect()` - 自动连接设备

```cpp
bool AutoConnect();
```

**参数：**
- 无

**返回值：**
- 成功返回 `true`，失败返回 `false`

**说明：** `AutoConnect()` 会自动搜索所有可用的网络适配器并尝试连接，是推荐的连接方式。

**示例：**
```cpp
DexHand hand(CommType::ETHERCAT);

if (hand.AutoConnect()) {
    std::cout << "自动连接成功！" << std::endl;
}
```

---

##### `Disconnect()` - 断开连接

```cpp
bool Disconnect();
```

**返回值：**
- 成功返回 `true`，失败返回 `false`

**示例：**
```cpp
hand.Disconnect();
```

---

##### `IsConnected()` - 检查连接状态

```cpp
bool IsConnected() const;
```

**返回值：**
- 已连接返回 `true`，否则返回 `false`

**示例：**
```cpp
if (hand.IsConnected()) {
    std::cout << "设备已连接" << std::endl;
}
```

---

##### `SearchAdapters()` - 搜索可用的通信适配器

```cpp
std::map<std::string, std::string> SearchAdapters() const;
```

**返回值：**
- 返回适配器名称到描述的映射表

**示例：**
```cpp
DexHand hand(CommType::ETHERCAT);
auto adapters = hand.SearchAdapters();

std::cout << "找到 " << adapters.size() << " 个适配器:" << std::endl;
for (const auto& pair : adapters) {
    std::cout << "  " << pair.first << ": " << pair.second << std::endl;
}
```

**输出示例：**
```
找到 2 个适配器:
  \Device\NPF_{GUID1}: Realtek PCIe GbE Family Controller
  \Device\NPF_{GUID2}: VirtualBox Host-Only Network Adapter
```

---

##### `GetHandType()` - 获取手部类型

```cpp
HandType GetHandType() const;
```

**返回值：**
- 返回手部类型（LEFT/RIGHT）

**示例：**
```cpp
HandType type = hand.GetHandType();
std::cout << "手部类型: " << ToString(type) << std::endl;
```

---

##### `GetDeviceInfo()` - 获取设备信息

```cpp
DeviceInfo GetDeviceInfo() const;
```

**返回值：**
- 返回设备信息结构体

**DeviceInfo 结构：**
```cpp
struct DeviceInfo {
    std::string device_name;       // 设备名称
    std::string hardware_version;  // 硬件版本
    std::string software_version;  // 软件版本（固件版本）
    std::string serial_number;     // 序列号
};
```

**示例：**
```cpp
DeviceInfo info = hand.GetDeviceInfo();
std::cout << "设备名称: " << info.device_name << std::endl;
std::cout << "硬件版本: " << info.hardware_version << std::endl;
std::cout << "软件版本: " << info.software_version << std::endl;
std::cout << "序列号: " << info.serial_number << std::endl;
```

---

#### 2.4.2 关节控制

##### `SetControlMode()` - 设置控制模式

```cpp
void SetControlMode(ControlMode mode);
```

**参数：**
- `mode`: 控制模式（POSITION/TORQUE/SPEED）

**示例：**
```cpp
// 设置为位置控制模式（最常用）
hand.SetControlMode(ControlMode::POSITION);

// 设置为力矩控制模式
hand.SetControlMode(ControlMode::TORQUE);
```

---

##### `MoveJoints()` - 控制关节运动

```cpp
bool MoveJoints(const std::vector<JointCommand>& joints);
```

**参数：**
- `joints`: 关节命令列表

**返回值：**
- 成功返回 `true`，失败返回 `false`

**JointCommand 结构：**
```cpp
struct JointCommand {
    JointId id;      // 关节 ID
    float angle;     // 目标角度（弧度）
    uint8_t velocity;   // 目标速度（0-100%）
    uint8_t torque;     // 目标力矩（0-100%）
};
```

**示例：**
```cpp
// 控制食指弯曲
std::vector<JointCommand> joints = {
    {JointId::FF_PIP, 45.0f * M_PI / 180.0f, 80, 80},  // 45 度
    {JointId::FF_MCP, 30.0f * M_PI / 180.0f, 80, 80}   // 30 度
};

hand.MoveJoints(joints);

// 控制多个手指
std::vector<JointCommand> multi_joints = {
    // 食指
    {JointId::FF_PIP, 60.0f * M_PI / 180.0f, 100, 100},
    {JointId::FF_MCP, 45.0f * M_PI / 180.0f, 100, 100},
    // 中指
    {JointId::MF_PIP, 60.0f * M_PI / 180.0f, 100, 100},
    {JointId::MF_MCP, 45.0f * M_PI / 180.0f, 100, 100},
    // 拇指
    {JointId::THUMB_PIP, 40.0f * M_PI / 180.0f, 80, 80},
    {JointId::THUMB_MCP, 30.0f * M_PI / 180.0f, 80, 80}
};

hand.MoveJoints(multi_joints);
```

**注意：**
- 角度使用弧度，需要将度转换为弧度：`弧度 = 度 × π / 180`
- 速度和力矩使用百分比（0-100）
- 不要发送 DIP 关节的命令（会被自动跳过）

---

##### `Stop()` - 立即停止所有运动

```cpp
void Stop();
```

**示例：**
```cpp
hand.Stop();  // 紧急停止
```

---

#### 2.4.3 手部状态

##### `ClearFault()` - 清除故障状态

```cpp
bool ClearFault();
```

**返回值：**
- 成功返回 `true`，失败返回 `false`

**说明：** 当手部进入错误状态时，可以尝试清除故障。如果无法清除，需要检查硬件连接。

**示例：**
```cpp
if (!hand.ClearFault()) {
    std::cerr << "清除故障失败，请检查硬件" << std::endl;
}
```

---

##### `InitJoint()` - 初始化关节

```cpp
bool InitJoint();
```

**返回值：**
- 成功返回 `true`，失败返回 `false`

**说明：** 初始化所有关节，通常在连接后调用一次。

**示例：**
```cpp
hand.AutoConnect();
hand.InitJoint();  // 初始化关节
```

---

#### 2.4.4 触觉传感器

##### `OpenTactile()` - 打开触觉传感器

```cpp
bool OpenTactile();
```

**返回值：**
- 成功返回 `true`，失败返回 `false`

**示例：**
```cpp
if (hand.OpenTactile()) {
    std::cout << "触觉传感器已打开" << std::endl;
}
```

---

##### `CloseTactile()` - 关闭触觉传感器

```cpp
bool CloseTactile();
```

**返回值：**
- 成功返回 `true`，失败返回 `false`

**示例：**
```cpp
hand.CloseTactile();
```

---

##### `ZeroTactile()` - 触觉传感器清零

```cpp
bool ZeroTactile();
```

**返回值：**
- 成功返回 `true`，失败返回 `false`

**说明：** 将触觉传感器读数归零，用于消除零点漂移。

**示例：**
```cpp
// 使用前清零
hand.ZeroTactile();
std::this_thread::sleep_for(std::chrono::milliseconds(100));
```

---

#### 2.4.5 数据回调订阅

C++ SDK 使用回调机制实现数据实时推送，避免轮询开销。

##### `SetJointsCallback()` - 注册关节数据回调

```cpp
void SetJointsCallback(JointsCallback cb);
```

**回调类型：**
```cpp
using JointsCallback = std::function<void(const std::vector<Joint>&)>;
```

**参数：**
- `cb`: 回调函数，接收关节状态列表

**Joint 结构：**
```cpp
struct Joint {
    JointId id;        // 关节 ID
    State state;       // 关节状态
    ErrorCode error;   // 错误码
    float angle;       // 当前角度（弧度）
    uint8_t velocity;  // 当前速度
    uint8_t torque;    // 当前力矩

    // 便利方法
    bool IsNormal() const;   // 是否正常
    bool HasError() const;   // 是否有错误
    std::string ToString() const;  // 转换为字符串
};
```

**示例：**
```cpp
hand.SetJointsCallback([](const std::vector<Joint>& joints) {
    for (const auto& joint : joints) {
        float angle_deg = joint.angle * 180.0f / M_PI;
        std::cout << ToString(joint.id) << ": "
                  << angle_deg << "°, "
                  << "状态: " << ToString(joint.state) << std::endl;
    }
});
```

---

##### `SetHandStateCallback()` - 注册手部状态回调

```cpp
void SetHandStateCallback(HandStateCallback cb);
```

**回调类型：**
```cpp
using HandStateCallback = std::function<void(const HandState&)>;
```

**参数：**
- `cb`: 回调函数，接收手部状态

**HandState 结构：**
```cpp
struct HandState {
    State state;           // 手部状态
    ErrorCode error;       // 错误码
    int16_t temperature;   // 温度（0.1°C 单位）

    // 便利方法
    bool IsNormal() const;   // 是否正常
    bool HasError() const;   // 是否有错误
    std::string ToString() const;  // 转换为字符串
};
```

**示例：**
```cpp
hand.SetHandStateCallback([](const HandState& state) {
    float temp = state.temperature / 10.0f;
    std::cout << "手部状态: " << ToString(state.state) << std::endl;
    std::cout << "温度: " << temp << "°C" << std::endl;

    if (state.HasError()) {
        std::cerr << "错误: " << ToString(state.error) << std::endl;
    }
});
```

---

##### `SetTactileDataCallback()` - 注册触觉数据回调

```cpp
void SetTactileDataCallback(TactileDataCallback cb);
```

**回调类型：**
```cpp
using TactileDataCallback = std::function<void(const TactileData&)>;
```

**参数：**
- `cb`: 回调函数，接收触觉数据

**TactileData 结构：**
```cpp
struct FingerTactileData {
    bool state;                          // 传感器状态
    Force resultant_force;               // 合力
    std::vector<Force> distributed_forces;  // 分布力
};

struct TactileData {
    uint8_t sensor_state;    // 传感器状态字节
    uint8_t sensor_error;    // 传感器错误码

    FingerTactileData thumb;   // 拇指触觉数据
    FingerTactileData index;   // 食指触觉数据
    FingerTactileData middle;  // 中指触觉数据
    FingerTactileData ring;    // 无名指触觉数据
    FingerTactileData pinky;   // 小指触觉数据
};

struct Force {
    float x;  // X 方向分量
    float y;  // Y 方向分量
    float z;  // Z 方向分量
};
```

**示例：**
```cpp
hand.SetTactileDataCallback([](const TactileData& data) {
    // 计算拇指合力大小
    const Force& f = data.thumb.resultant_force;
    float magnitude = std::sqrt(f.x*f.x + f.y*f.y + f.z*f.z);

    std::cout << "拇指合力: " << magnitude << " N" << std::endl;

    // 检查传感器状态
    if (!data.thumb.state) {
        std::cerr << "拇指传感器异常" << std::endl;
    }
});
```

---

#### 2.4.6 日志系统

C++ SDK 提供了完善的日志系统，支持控制台和文件双输出，多级别日志控制。

##### `ConfigureConsole()` - 配置控制台日志

```cpp
void ConfigureConsole(LogLevel level);
```

**参数：**
- `level`: 日志级别（INFO/DEBUG）

**说明：** 控制台日志默认只显示 WARNING 和 ERROR 级别。可以升级到 INFO 或 DEBUG 级别。

**示例：**
```cpp
#include "xiaoyao/logging.h"

// 升级到 INFO 级别（推荐用于生产环境）
xiaoyao::ConfigureConsole(xiaoyao::LogLevel::INFO);

// 升级到 DEBUG 级别（仅推荐用于开发调试）
xiaoyao::ConfigureConsole(xiaoyao::LogLevel::DEBUG);
```

---

##### `ConfigureFile()` - 配置文件日志

```cpp
void ConfigureFile(const std::string& filename, LogLevel level = LogLevel::DEBUG);
```

**参数：**
- `filename`: 日志文件路径
- `level`: 日志级别（默认 DEBUG）

**说明：** 文件日志与控制台日志独立，可以设置不同的级别。文件日志默认使用详细格式（包含时间戳、文件名、行号）。

**示例：**
```cpp
// 启用文件日志（DEBUG 级别）
xiaoyao::ConfigureFile("xiaoyao.log");

// 启用文件日志（INFO 级别）
xiaoyao::ConfigureFile("xiaoyao.log", xiaoyao::LogLevel::INFO);
```

---

##### 日志宏使用

在代码中使用日志宏记录信息：

```cpp
LOG_INFO("连接到设备: " << adapter_name);
LOG_ERROR("连接失败");
LOG_DEBUG("调试信息: 变量值 = " << variable);
LOG_WARNING("警告: 温度过高");
```

**日志级别说明：**

| 级别 | 宏名 | 用途 | 推荐场景 |
|------|------|------|----------|
| DEBUG | `LOG_DEBUG()` | 详细的调试信息 | 仅开发调试 |
| INFO | `LOG_INFO()` | 一般信息 | 生产环境 |
| WARNING | `LOG_WARNING()` | 警告信息 | 生产环境 |
| ERROR | `LOG_ERROR()` | 错误信息 | 生产环境 |

**完整示例：**
```cpp
#include "xiaoyao/xiaoyao.h"
#include "xiaoyao/logging.h"
#include <iostream>

using namespace xiaoyao;

int main() {
    // 配置日志
    ConfigureConsole(LogLevel::INFO);         // 控制台输出 INFO 及以上
    ConfigureFile("xiaoyao.log");             // 文件输出 DEBUG 及以上

    LOG_INFO("程序启动");

    DexHand hand(CommType::ETHERCAT);

    LOG_INFO("正在连接设备...");
    if (!hand.AutoConnect()) {
        LOG_ERROR("连接失败");
        return 1;
    }

    LOG_INFO("连接成功");

    DeviceInfo info = hand.GetDeviceInfo();
    LOG_DEBUG("设备信息: " << info.device_name);
    LOG_DEBUG("硬件版本: " << info.hardware_version);
    LOG_DEBUG("软件版本: " << info.software_version);

    // 注册回调
    hand.SetHandStateCallback([](const HandState& state) {
        float temp = state.temperature / 10.0f;

        if (temp > 50.0f) {
            LOG_WARNING("温度过高: " << temp << "°C");
        }

        if (state.HasError()) {
            LOG_ERROR("手部错误: " << state.ToString());
        }
    });

    // ... 其他操作 ...

    LOG_INFO("断开连接");
    hand.Disconnect();

    LOG_INFO("程序结束");
    return 0;
}
```

**注意事项：**
- `ToString()` 系列函数有字符串处理开销，**不应在实时控制循环中调用**
- 生产环境推荐使用 INFO 级别，避免过多日志影响性能
- 日志文件会自动创建，支持日志追加

### 2.5 使用示例

#### 2.5.1 基础操作

**示例文件：** `examples/basic_connection.cc`

```cpp
#include "xiaoyao/xiaoyao.h"
#include <iostream>

int main() {
    using namespace xiaoyao;

    DexHand hand(CommType::ETHERCAT);

    // 连接设备
    std::cout << "正在通过 EtherCAT 连接灵巧手..." << std::endl;
    bool success = hand.AutoConnect();

    if (!success) {
        std::cerr << "错误: 无法连接到灵巧手！" << std::endl;
        return 1;
    }

    std::cout << "✓ 成功连接到灵巧手！" << std::endl;

    // 获取手部类型
    HandType type = hand.GetHandType();
    std::cout << "手部类型: " << ToString(type) << std::endl;

    // 获取固件版本
    DeviceInfo info = hand.GetDeviceInfo();
    if (!info.software_version.empty()) {
        std::cout << "固件版本: " << info.software_version << std::endl;
    }

    // 断开连接
    hand.Disconnect();
    std::cout << "✓ 已断开连接" << std::endl;

    return 0;
}
```

**编译运行：**

**Windows:**
```bash
cd build
cmake --build . --config Release
.\examples\Release\basic_connection.exe
```

**Linux:**
```bash
cd build
make -j$(nproc)
sudo ./examples/basic_connection
```

**输出示例：**
```
正在通过 EtherCAT 连接灵巧手...
✓ 成功连接到灵巧手！
手部类型: RIGHT
固件版本: 1.0.2
✓ 已断开连接
```

---

#### 2.5.2 关节控制

**示例文件：** `examples/move_joints.cc`

```cpp
#include "xiaoyao/xiaoyao.h"
#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>

using namespace xiaoyao;

// 关节状态显示回调函数
void DisplayJoints(const std::vector<Joint>& joints) {
    std::cout << "\n========== 关节状态 ==========" << std::endl;
    std::cout << std::left << std::setw(20) << "关节"
              << std::setw(12) << "角度(度)"
              << "状态" << std::endl;
    std::cout << std::string(40, '-') << std::endl;

    for (const auto& joint : joints) {
        float angle_deg = joint.angle * 180.0f / M_PI;
        std::cout << std::left << std::setw(20) << ToString(joint.id)
                  << std::fixed << std::setprecision(1) << std::setw(12) << angle_deg
                  << ToString(joint.state) << std::endl;
    }
}

int main() {
    DexHand hand(CommType::ETHERCAT);

    // 连接设备
    std::cout << "正在连接设备..." << std::endl;
    if (!hand.AutoConnect()) {
        std::cerr << "连接失败！" << std::endl;
        return 1;
    }

    std::cout << "✓ 连接成功" << std::endl;

    // 注册关节状态回调
    hand.SetJointsCallback(DisplayJoints);

    // 设置控制模式为位置模式
    hand.SetControlMode(ControlMode::POSITION);

    // 定义关节命令
    std::vector<JointCommand> joints = {
        {JointId::FF_MCP, 30.0f * M_PI / 180.0f, 100, 100},
        {JointId::FF_PIP, 45.0f * M_PI / 180.0f, 100, 100},
    };

    std::cout << "\n发送关节命令..." << std::endl;
    hand.MoveJoints(joints);

    // 保持姿势 5 秒
    std::this_thread::sleep_for(std::chrono::seconds(5));

    // 复位
    std::cout << "\n复位关节..." << std::endl;
    std::vector<JointCommand> reset_joints = {
        {JointId::FF_MCP, 0.0f, 100, 100},
        {JointId::FF_PIP, 0.0f, 100, 100}
    };
    hand.MoveJoints(reset_joints);
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // 断开连接
    hand.Disconnect();
    std::cout << "✓ 已断开连接" << std::endl;

    return 0;
}
```

---

#### 2.5.3 触觉数据读取

**示例文件：** `examples/tactile_callback.cc`

```cpp
#include "xiaoyao/xiaoyao.h"
#include <iostream>
#include <cmath>
#include <thread>
#include <chrono>

using namespace xiaoyao;

int main() {
    DexHand hand(CommType::ETHERCAT);

    // 连接设备
    std::cout << "正在连接设备..." << std::endl;
    if (!hand.AutoConnect()) {
        std::cerr << "连接失败！" << std::endl;
        return 1;
    }

    std::cout << "✓ 连接成功" << std::endl;

    // 打开触觉传感器
    std::cout << "打开触觉传感器..." << std::endl;
    if (!hand.OpenTactile()) {
        std::cerr << "打开触觉传感器失败！" << std::endl;
        hand.Disconnect();
        return 1;
    }

    // 清零
    std::cout << "触觉传感器清零..." << std::endl;
    hand.ZeroTactile();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 注册触觉数据回调
    hand.SetTactileDataCallback([](const TactileData& data) {
        std::cout << "\n========== 触觉数据 ==========" << std::endl;

        // 计算各手指合力
        auto print_force = [](const std::string& name, const FingerTactileData& finger) {
            const Force& f = finger.resultant_force;
            float magnitude = std::sqrt(f.x*f.x + f.y*f.y + f.z*f.z);
            std::cout << name << ": " << magnitude << " N"
                      << " (状态: " << (finger.state ? "正常" : "异常") << ")" << std::endl;
        };

        print_force("拇指", data.thumb);
        print_force("食指", data.index);
        print_force("中指", data.middle);
        print_force("无名指", data.ring);
        print_force("小指", data.pinky);
    });

    std::cout << "\n按 Ctrl+C 退出程序" << std::endl;

    // 持续运行
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    // 断开连接
    hand.CloseTactile();
    hand.Disconnect();

    return 0;
}
```

---

#### 2.5.4 预制手势

**说明：** C++ SDK 不提供内置的预制手势功能，但提供了灵活的接口供用户自定义实现。示例代码 `preset_gesture.cc` 演示了如何实现常见的预设手势。

**示例文件：** `examples/preset_gesture.cc`

```cpp
#include "xiaoyao/xiaoyao.h"
#include <iostream>
#include <unordered_map>
#include <thread>
#include <chrono>

using namespace xiaoyao;

// 手势类型定义
enum class GestureType {
    OPEN_HAND,      // 张开手
    FIST,           // 握拳
    OK,             // OK手势
    THUMBS_UP,      // 竖大拇指
    SIX_SIGN        // 六手势
};

// 手势关节角度定义（单位：度）
const std::unordered_map<GestureType, std::unordered_map<JointId, float>> GESTURE_DEFINITIONS = {
    {
        GestureType::OPEN_HAND,
        {
            {JointId::THUMB_PIP, 0.0f}, {JointId::THUMB_MCP, 0.0f},
            {JointId::FF_PIP, 0.0f}, {JointId::FF_MCP, 0.0f},
            {JointId::MF_PIP, 0.0f}, {JointId::MF_MCP, 0.0f},
            {JointId::RF_PIP, 0.0f}, {JointId::RF_MCP, 0.0f},
            {JointId::LF_PIP, 0.0f}, {JointId::LF_MCP, 0.0f},
        }
    },
    {
        GestureType::FIST,
        {
            {JointId::THUMB_PIP, 40.0f}, {JointId::THUMB_MCP, 30.0f},
            {JointId::FF_PIP, 65.0f}, {JointId::FF_MCP, 55.0f},
            {JointId::MF_PIP, 65.0f}, {JointId::MF_MCP, 55.0f},
            {JointId::RF_PIP, 65.0f}, {JointId::RF_MCP, 55.0f},
            {JointId::LF_PIP, 65.0f}, {JointId::LF_MCP, 55.0f},
        }
    },
    // ... 其他手势定义 ...
};

// 执行手势
bool ExecuteGesture(DexHand& hand, GestureType gesture, uint8_t speed = 100, uint8_t torque = 100) {
    auto it = GESTURE_DEFINITIONS.find(gesture);
    if (it == GESTURE_DEFINITIONS.end()) {
        return false;
    }

    std::vector<JointCommand> joints;
    for (const auto& pair : it->second) {
        float angle_rad = pair.second * M_PI / 180.0f;
        joints.push_back({pair.first, angle_rad, speed, torque});
    }

    return hand.MoveJoints(joints);
}

int main() {
    DexHand hand(CommType::ETHERCAT);

    if (!hand.AutoConnect()) {
        std::cerr << "连接失败！" << std::endl;
        return 1;
    }

    hand.SetControlMode(ControlMode::POSITION);

    // 演示手势序列
    std::vector<GestureType> gestures = {
        GestureType::OPEN_HAND,
        GestureType::FIST,
        GestureType::OK,
        GestureType::THUMBS_UP,
        GestureType::SIX_SIGN,
    };

    for (auto gesture : gestures) {
        ExecuteGesture(hand, gesture);
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    }

    hand.Disconnect();
    return 0;
}
```

**扩展手势：**
用户可以根据需要扩展手势类型和角度配置：

```cpp
// 添加新手势
enum class GestureType {
    // ... 现有手势 ...
    VICTORY,        // 剪刀手
    POINTING        // 指向
};

// 定义新手势的关节角度
GESTURE_DEFINITIONS[GestureType::VICTORY] = {
    {JointId::FF_PIP, 0.0f}, {JointId::FF_MCP, 0.0f},  // 食指伸直
    {JointId::MF_PIP, 0.0f}, {JointId::MF_MCP, 0.0f},  // 中指伸直
    {JointId::THUMB_PIP, 40.0f}, {JointId::THUMB_MCP, 30.0f},  // 拇指弯曲
    {JointId::RF_PIP, 90.0f}, {JointId::RF_MCP, 80.0f},  // 无名指弯曲
    {JointId::LF_PIP, 90.0f}, {JointId::LF_MCP, 80.0f},  // 小指弯曲
};
```

---

#### 2.5.5 多手控制

**示例文件：** `examples/multi_hand.cc`

C++ SDK 原生支持多手控制，可以同时控制多台灵巧手设备。

```cpp
#include "xiaoyao/xiaoyao.h"
#include <iostream>
#include <memory>
#include <vector>
#include <thread>
#include <chrono>

using namespace xiaoyao;

struct HandInstance {
    std::unique_ptr<DexHand> hand;
    std::string name;
    DeviceInfo info;
    std::vector<Joint> joints_cache;
    bool connected;
};

class MultiDexHandController {
public:
    bool Initialize() {
        DexHand temp_hand(CommType::ETHERCAT);
        auto adapters = temp_hand.SearchAdapters();

        std::cout << "找到 " << adapters.size() << " 个适配器" << std::endl;

        for (const auto& adapter : adapters) {
            auto instance = std::make_unique<HandInstance>();
            instance->hand = std::make_unique<DexHand>(CommType::ETHERCAT);

            if (instance->hand->Connect(adapter.first)) {
                instance->name = "hand_" + std::to_string(hands_.size());
                instance->connected = true;
                instance->info = instance->hand->GetDeviceInfo();

                hands_.push_back(std::move(instance));
            }
        }

        return !hands_.empty();
    }

    size_t GetHandCount() const { return hands_.size(); }

    bool MoveHand(size_t index, const std::vector<JointCommand>& joints) {
        if (index >= hands_.size()) return false;
        return hands_[index]->hand->MoveJoints(joints);
    }

    bool MoveAllHands(const std::vector<std::vector<JointCommand>>& joints_list) {
        if (joints_list.size() != hands_.size()) return false;

        bool all_success = true;
        for (size_t i = 0; i < hands_.size(); ++i) {
            if (!MoveHand(i, joints_list[i])) {
                all_success = false;
            }
        }
        return all_success;
    }

    void CloseAll() {
        for (auto& instance : hands_) {
            if (instance->connected) {
                instance->hand->Disconnect();
            }
        }
        hands_.clear();
    }

private:
    std::vector<std::unique_ptr<HandInstance>> hands_;
};

int main() {
    MultiDexHandController controller;

    if (!controller.Initialize()) {
        std::cerr << "初始化失败！" << std::endl;
        return 1;
    }

    std::cout << "✓ 成功连接 " << controller.GetHandCount() << " 台设备" << std::endl;

    // 分别控制每只手
    for (size_t i = 0; i < controller.GetHandCount(); ++i) {
        std::vector<JointCommand> joints = {
            {JointId::FF_PIP, 45.0f * M_PI / 180.0f, 100, 100}
        };
        controller.MoveHand(i, joints);
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    // 同时控制所有手
    std::vector<std::vector<JointCommand>> all_joints;
    for (size_t i = 0; i < controller.GetHandCount(); ++i) {
        all_joints.push_back({
            {JointId::FF_PIP, 0.0f, 100, 100},
            {JointId::FF_MCP, 0.0f, 100, 100}
        });
    }
    controller.MoveAllHands(all_joints);

    controller.CloseAll();
    return 0;
}
```

**多手控制的优势：**
- 可以独立控制每只手
- 可以同步控制所有手
- 适用于双手机器人、多工位应用等场景

---

#### 2.5.6 交互式控制

**示例文件：** `examples/interactive_joint_control.cc`

提供交互式命令行界面，用户可以输入关节参数实时控制灵巧手。

```cpp
#include "xiaoyao/xiaoyao.h"
#include <iostream>
#include <unordered_map>
#include <string>

using namespace xiaoyao;

struct JointParams {
    float angle;
    int speed;
    int torque;
};

void DisplayJointIdList() {
    std::cout << "\n关节ID列表:" << std::endl;
    std::cout << "  1: THUMB_PIP,      2: THUMB_MCP,      3: THUMB_SWING,     4: THUMB_ROTATION" << std::endl;
    std::cout << "  6: FF_PIP,         7: FF_MCP,         8: FF_SWING" << std::endl;
    std::cout << " 10: MF_PIP,        11: MF_MCP" << std::endl;
    std::cout << " 13: RF_PIP,        14: RF_MCP" << std::endl;
    std::cout << " 16: LF_PIP,        17: LF_MCP" << std::endl;
}

int main() {
    DexHand hand(CommType::ETHERCAT);

    if (!hand.AutoConnect()) {
        std::cerr << "连接失败！" << std::endl;
        return 1;
    }

    std::cout << "✓ 连接成功" << std::endl;
    hand.SetControlMode(ControlMode::POSITION);

    std::cout << "\n交互式控制模式已启动" << std::endl;
    std::cout << "按 Ctrl+C 退出\n" << std::endl;

    while (true) {
        std::unordered_map<int, JointParams> joint_params;

        DisplayJointIdList();
        std::cout << "\n请输入关节ID和参数（输入空行结束）:\n" << std::endl;

        // 交互式输入
        while (true) {
            std::cout << "关节ID: ";
            std::string input;
            std::getline(std::cin, input);

            if (input.empty()) break;

            int joint_id = std::stoi(input);
            JointParams& params = joint_params[joint_id];

            std::cout << "  角度(度) [" << params.angle << "]: ";
            std::getline(std::cin, input);
            if (!input.empty()) params.angle = std::stof(input);

            std::cout << "  速度(0-100) [" << params.speed << "]: ";
            std::getline(std::cin, input);
            if (!input.empty()) params.speed = std::stoi(input);

            std::cout << "  力矩(0-100) [" << params.torque << "]: ";
            std::getline(std::cin, input);
            if (!input.empty()) params.torque = std::stoi(input);
        }

        // 构建关节命令
        std::vector<JointCommand> joints;
        for (const auto& pair : joint_params) {
            float angle_rad = pair.second.angle * M_PI / 180.0f;
            joints.push_back({
                static_cast<JointId>(pair.first),
                angle_rad,
                static_cast<uint8_t>(pair.second.speed),
                static_cast<uint8_t>(pair.second.torque)
            });
        }

        // 发送命令
        if (hand.MoveJoints(joints)) {
            std::cout << "✓ 命令发送成功" << std::endl;
        } else {
            std::cerr << "✗ 命令发送失败" << std::endl;
        }

        std::cout << "\n按回车继续..." << std::endl;
        std::cin.ignore();
    }

    hand.Disconnect();
    return 0;
}
```

---

## 附录

### A. 完整的 API 参考

详细的 API 文档请参考头文件中的注释：
- `include/xiaoyao/xiaoyao.h` - 主要 API
- `include/xiaoyao/types.h` - 类型定义
- `include/xiaoyao/logging.h` - 日志系统

### B. 常见问题

**Q1: 连接失败怎么办？**

A: 检查以下几点：
1. 确认灵巧手已上电
2. 确认网线连接正确
3. Windows 用户确认已安装 Npcap
4. Linux 用户确认有 root 权限或已配置网卡权限
5. 使用 `SearchAdapters()` 查看可用适配器

**Q2: 关节不动？**

A: 检查：
1. 是否已调用 `SetControlMode(ControlMode::POSITION)`
2. 关节命令中的角度值是否正确（注意使用弧度）
3. 检查回调数据中的错误码

**Q3: 如何获取实时数据？**

A: 使用回调机制：
```cpp
hand.SetJointsCallback([](const std::vector<Joint>& joints) {
    // 处理关节数据
});

hand.SetTactileDataCallback([](const TactileData& data) {
    // 处理触觉数据
});
```

**Q4: 性能优化建议？**

A:
1. 不要在回调函数中执行耗时操作
2. 避免在实时循环中调用 `ToString()`
3. 使用合适的日志级别（生产环境用 INFO）
4. 使用 `-O3` 编译优化

**Q5: 如何自定义手势？**

A: 参考 `preset_gesture.cc`，创建自己的手势定义：
```cpp
std::unordered_map<JointId, float> my_gesture = {
    {JointId::FF_PIP, 45.0f},
    {JointId::FF_MCP, 30.0f},
    // ... 其他关节
};
```

### C. 技术支持

如有问题或建议，请联系：

- **Email**: qpan@glitech.com
- **公司**: 深圳果力智能科技有限公司
- **SDK 仓库**: https://github.com/yourcompany/xiaoyao-sdk-cpp

---

## 许可证

Xiaoyao SDK C++ 是 Glitech 的专有软件。使用本 SDK 前，请参阅 LICENSE 文件了解完整条款。

Copyright © 2025 Glitech. All rights reserved.
