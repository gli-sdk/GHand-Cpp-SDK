# Xiaoyao-SDK-C++

## 功能特性
- 支持多种通信方式：EtherCAT、CANFD、RS485
- 提供对5根手指每个关节的独立控制
- 关节级控制，支持角度、速度和扭矩参数
- 数据实时回调：支持关节数据、触觉合力和分布力数据的实时推送
- 支持左手和右手设备
- Windows平台动态链接库(DLL)形式提供

## 目录结构
- TODO

## 系统要求
- Windows 7 或更高版本
- Visual Studio 2017 或更高版本
- CMake 3.5 或更高版本

## 编译构建
```bash
   mkdir build
   cd build
   cmake ..
   cmake --build . --config Release
```
## 快速开始
```cpp
#include "xiaoyao/dexhand.h"

int main() {
    // 创建机械手对象
    DexHand hand;
    
    // 打开设备连接
    hand.Open(COMM_ETHERCAT, "auto");
    
    // 创建关节运动参数
    std::vector<Joint> joints;
    // ... 设置关节参数 ...
    
    // 控制机械手运动
    hand.MoveJoints(joints);
    
    // 关闭设备连接
    hand.Close();
    
    return 0;
}
```
## Licence
- TODO

## 支持与反馈
- 如有问题或建议，请提交 Issue 或联系技术支持。
- Charles: csi@glitech.com