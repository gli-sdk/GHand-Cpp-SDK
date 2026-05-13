# GHand SDK C++

GHand SDK C++ 是GHand灵巧手的 C++ 开发包，支持 EtherCAT 、CANFD 和 RS485 通信，提供关节控制、触觉数据采集和实时状态反馈。

## 功能特性

- 支持多种通信方式：EtherCAT、CANFD、RS485
- 提供对5根手指每个关节的独立控制
- 关节级控制，支持角度、速度和扭矩参数
- 数据实时回调：支持关节数据、触觉合力和分布力数据的实时推送
- 支持左手和右手设备
- Windows 平台动态链接库 (DLL) 形式提供

## 目录结构

```
ghand-sdk-cpp/
├── include/ghand/            # 公共头文件
│   ├── dexhand.h             # 主 API
│   ├── types.h               # 类型定义
│   ├── logging.h             # 日志系统
│   ├── export.h              # DLL 导出宏
│   └── version.h             # 版本信息
├── src/
│   ├── dexhand.cc            # 公共 API 实现（PIMPL 桥接）
│   ├── logging.cc            # 日志实现
│   ├── types.cc              # 类型序列化
│   ├── comm/                 # 通信层
│   │   ├── icomm.h           # 通信抽象接口
│   │   ├── ethercat_comm.h   # EtherCAT 实现
│   │   ├── ethercat_comm.cc
│   │   ├── canfd_comm.h      # CANFD 实现
│   │   ├── canfd_comm.cc
│   │   ├── canfd_protocol.h  # CANFD 协议编解码
│   │   ├── canfd_protocol.cc
│   │   ├── canfd_driver.h    # CANFD 驱动抽象
│   │   └── canfd_driver_zlg.cc  # ZLG 驱动实现
│   └── internal/             # 内部实现
│       ├── dexhand.h         # 内部状态机
│       ├── dexhand.cc
│       ├── dexhand_callback_manager.h  # 回调管理 + 数据缓存
│       ├── dexhand_callback_manager.cc
│       ├── product_config.h          # 产品配置结构
│       ├── product_config_loader.h   # JSON 配置加载
│       ├── product_config_loader.cc
│       ├── file_lock.h       # 设备文件锁
│       └── file_lock.cc
├── config/                   # 产品配置文件
│   └── XIAOYAO-Hand.json
├── examples/
│   ├── tutorial/
│   │   ├── 01basic_connection.cc         # 基础连接
│   │   ├── 02move_joints.cc             # 关节运动
│   │   ├── 03speed_control.cc           # 速度控制
│   │   ├── 04torque_control.cc          # 力矩控制
│   │   ├── 05tactile_callback.cc        # 触觉回调
│   │   ├── 06multi_hand.cc              # 多手控制
│   │   ├── 07glove_control.cc           # 手套控制
│   │   └── 08interactive_joint_control.cc  # 交互式控制
│   └── demo/
│       ├── preset_gesture.cc            # 预设手势
│       ├── gesture_dance.cc             # 手势舞
│       ├── grabbing_action.cc           # 抓取
│       ├── press_action.cc              # 按压
│       ├── clap_action.cc               # 拍手
│       ├── hold_action.cc               # 握持
│       ├── knock_action.cc              # 敲击
│       ├── lift_action.cc               # 拎起
│       ├── pull_action.cc               # 拉动
│       └── support_action.cc            # 支撑
└── third_party/              # 第三方库
    ├── include/
    │   ├── soem/             # SOEM (EtherCAT)
    │   ├── wpcap/            # WinPcap
    │   ├── zlgcan/           # ZLG CAN
    │   └── nlohmann/         # JSON 解析
    └── lib/
        ├── windows/
        │   ├── soem.lib
        │   ├── wpcap.lib
        │   ├── Packet.lib
        │   ├── zlgcan.lib
        │   ├── zlgcan.dll
        │   └── kerneldlls/   # ZLG 驱动 DLL 及设备配置
        └── linux/
            └── libsoem.a
```

## 快速开始

```cpp
#include "ghand/dexhand.h"

using namespace ghand;

int main() {
    // 指定产品类型
    auto hand = DexHand::Create(ProductType::G5, CommType::ETHERCAT);

    // 或自动识别（连接后根据设备名匹配配置）
    auto hand = DexHand::Create(ProductType::AUTO, CommType::ETHERCAT);

    if (!hand || !hand->Connect("auto")) {
        printf("连接失败\n");
        return -1;
    }

    // 拉模式：主动查询最新缓存
    auto state   = hand->GetHandData();     // 手部状态
    auto joints  = hand->GetJointsData();   // 关节数据
    auto tactile = hand->GetTactileData();  // 触觉数据

    // 推模式：注册回调
    hand->SetJointsCallback([](const std::vector<Joint>& joints) {
        for (const auto& j : joints) {
            printf("关节 %d: %.1f°\n", (int)j.id, j.angle);
        }
    });

    // 控制关节
    std::vector<JointCommand> cmds = {
        {JointId::THUMB_MCP, 45.0f, 50, 50},
        {JointId::FF_MCP,    30.0f, 50, 50},
    };
    hand->MoveJoints(cmds);

    hand->Disconnect();
}
```

## 日志系统

SDK 提供了内置的日志系统，默认只显示 WARNING 和 ERROR 级别的日志。你可以根据需要配置日志级别：

```cpp
#include "ghand/logging.h"

ghand::ConfigureConsole(ghand::LogLevel::INFO);
ghand::ConfigureFile("ghand.log");
```

详细的日志系统使用说明请参阅 [C++ SDK 开发者文档](https://fcnzogxju7xr.feishu.cn/docx/Ex2Gd2i5RoJZzcxtIyPcSAW8nVg)。

## 编译示例

### Windows
```bash
mkdir build && cd build
cmake ..
cmake --build . --config Release

# 运行示例
.\examples\Release\basic_connection.exe
```

### Linux
```bash
# 安装依赖
sudo apt install -y cmake build-essential pkg-config libpcap-dev libssl-dev

mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# 运行示例（需要 root 权限访问网卡）
sudo ./examples/tutorial/basic_connection
```

## 系统要求

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
- GCC 7.5+ (支持 C++11)
- CMake 3.5 或更高版本
- libpcap-dev, libssl-dev

## API 参考

完整的 API 文档请查看 [C++ SDK 开发者文档](https://fcnzogxju7xr.feishu.cn/docx/Ex2Gd2i5RoJZzcxtIyPcSAW8nVg)，包含：
- 完整的接口说明和参数定义
- 详细的代码示例和使用场景
- 常见问题解答和技术支持信息
- 环境配置和安装流程

详细的 API 签名说明请查看头文件中的注释。

## 支持与反馈

- 如有问题或建议，请提交 Issue 或联系技术支持。
- Email: qpan@glitech.com

## 许可证

GHand SDK C++ 是 Glitech 的专有软件。使用本 SDK 前，请参阅 [LICENSE](LICENSE) 文件了解完整条款。

Copyright © 2025 Glitech. All rights reserved.
