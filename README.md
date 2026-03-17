# Xiaoyao SDK C++

## 概述

Xiaoyao SDK C++ 是枭尧灵巧手的 C++ 开发包，提供对机械手的完整控制功能。

## 功能特性

- 支持多种通信方式：EtherCAT、CANFD、RS485
- 提供对5根手指每个关节的独立控制
- 关节级控制，支持角度、速度和扭矩参数
- 数据实时回调：支持关节数据、触觉合力和分布力数据的实时推送
- 支持左手和右手设备
- Windows 平台动态链接库 (DLL) 形式提供

## 目录结构

```
xiaoyao-sdk-cpp/
├── include/xiaoyao/    # 公共头文件
│   ├── xiaoyao.h       # 主 API
│   ├── types.h         # 类型定义
│   ├── logging.h       # 日志系统
│   └── version.h       # 版本信息
├── lib/               # 库文件（按平台分类）
│   ├── windows/
│   │   └── x64/       # Windows 64位
│   │       ├── xiaoyao.lib
│   │       └── xiaoyao.dll
│   └── linux/
│       └── x64/       # Linux 64位（待添加）
└── examples/          # 示例代码
```

## 快速开始

### 1. 包含头文件

```cpp
#include "xiaoyao/xiaoyao.h"
```

### 2. 创建实例并连接

```cpp
xiaoyao::DexHand hand;

if (!hand.Connect(xiaoyao::CommType::ETHERCAT, "auto")) {
    printf("连接失败\n");
    return -1;
}
```

### 3. 注册回调

```cpp
hand.SetJointsCallback([](const std::vector<xiaoyao::Joint>& joints) {
    for (const auto& joint : joints) {
        printf("关节 %d: %.1f°\n", (int)joint.id, joint.angle);
    }
});

hand.SetTactileDataCallback([](const xiaoyao::TactileData& data) {
    auto force = data.GetResultant(xiaoyao::FingerType::THUMB);
    printf("拇指合力: %.2f N\n", sqrt(force.x*force.x + force.y*force.y + force.z*force.z));
});
```

### 4. 控制运动

```cpp
std::vector<xiaoyao::JointCommand> commands = {
    {xiaoyao::JointId::THUMB_MCP, 45.0f, 50, 50},
    {xiaoyao::JointId::FF_MCP, 30.0f, 50, 50}
};

hand.MoveJoints(commands);
```

### 5. 断开连接

```cpp
hand.Disconnect();
```

## 日志系统

SDK 提供了内置的日志系统，默认只显示 WARNING 和 ERROR 级别的日志。你可以根据需要配置日志级别：

```cpp
#include "xiaoyao/logging.h"

// 升级到 INFO 级别
xiaoyao::ConfigureConsole(xiaoyao::LogLevel::INFO);

// 启用文件日志（DEBUG 级别，包含详细的时间戳和源文件信息）
xiaoyao::ConfigureFile("xiaoyao.log");

// 在代码中记录日志
LOG_INFO("连接到设备: " << adapter_name);
LOG_ERROR("连接失败");
LOG_DEBUG("调试信息");
```

详细的日志系统使用说明请参阅 [日志系统文档](docs/LOGGING.md)。

## 编译示例

```bash
mkdir build 
cd build
cmake ..
cmake --build . --config Release

# 运行示例
.\examples\Release\basic_connection.exe
```

## 系统要求

### 当前支持的平台
- **Windows x64** (Windows 7 或更高版本)
  - Visual Studio 2017 或更高版本
  - CMake 3.5 或更高版本

### 计划支持的平台
- **Linux x64** (开发中)

## API 参考

详细的 API 文档请查看头文件中的注释。

## 支持与反馈

- 如有问题或建议，请提交 Issue 或联系技术支持。
- Email: qpan@glitech.com

## 许可证

Xiaoyao SDK C++ 是 Glitech 的专有软件。使用本 SDK 前，请参阅 [LICENSE](LICENSE) 文件了解完整条款。

Copyright © 2025 Glitech. All rights reserved.
