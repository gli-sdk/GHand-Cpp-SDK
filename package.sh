#!/bin/bash

VERSION="1.0.0"
SDK_NAME="xiaoyao-sdk-cpp-${VERSION}"
BUILD_DIR="$(pwd)/build"
PACKAGE_DIR="$(pwd)/package/${SDK_NAME}"

echo "========================================="
echo "  Xiaoyao SDK Packaging Script"
echo "========================================="

# 清理旧包
echo "Cleaning old package..."
rm -rf package

# 创建包目录结构
echo "Creating package directory structure..."
mkdir -p "${PACKAGE_DIR}/include/xiaoyao"
mkdir -p "${PACKAGE_DIR}/lib"
mkdir -p "${PACKAGE_DIR}/bin"
mkdir -p "${PACKAGE_DIR}/examples"

# 编译 Release 版本
echo "Building Release version..."
cd "${BUILD_DIR}"
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
cd ..

# 复制公共头文件
echo "Copying public headers..."
cp include/xiaoyao/xiaoyao.h "${PACKAGE_DIR}/include/xiaoyao/"
cp include/xiaoyao/types.h "${PACKAGE_DIR}/include/xiaoyao/"
cp include/xiaoyao/version.h "${PACKAGE_DIR}/include/xiaoyao/"

# 复制库文件
echo "Copying library files..."
if [ -f "${BUILD_DIR}/xiaoyao.lib" ]; then
    cp "${BUILD_DIR}/xiaoyao.lib" "${PACKAGE_DIR}/lib/"
fi

if [ -f "${BUILD_DIR}/Release/xiaoyao.dll" ]; then
    cp "${BUILD_DIR}/Release/xiaoyao.dll" "${PACKAGE_DIR}/bin/"
elif [ -f "${BUILD_DIR}/xiaoyao.dll" ]; then
    cp "${BUILD_DIR}/xiaoyao.dll" "${PACKAGE_DIR}/bin/"
fi

# 复制示例代码
echo "Copying examples..."
cp -r examples/* "${PACKAGE_DIR}/examples/"

# 复制文档
echo "Copying documentation..."
cp README.md "${PACKAGE_DIR}/" 2>/dev/null || true
cp CHANGELOG.md "${PACKAGE_DIR}/" 2>/dev/null || true

# 创建使用说明
cat > "${PACKAGE_DIR}/QUICKSTART.md" << 'EOF'
# Xiaoyao SDK C++ 快速开始

## 目录结构
```
xiaoyao-sdk-cpp/
├── include/xiaoyao/    # 头文件
│   ├── xiaoyao.h       # 主 API
│   ├── types.h         # 类型定义
│   └── version.h       # 版本信息
├── lib/               # 导入库
│   └── xiaoyao.lib
├── bin/               # 运行时库
│   └── xiaoyao.dll
└── examples/          # 示例代码
```

## 编译示例

### 使用 CMake
```bash
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

### 使用 Visual Studio
1. 配置头文件目录: `项目属性 -> C/C++ -> 常规 -> 附加包含目录`
   - 添加: `..\include`
2. 配置库目录: `项目属性 -> 链接器 -> 常规 -> 附加库目录`
   - 添加: `..\lib`
3. 配置依赖库: `项目属性 -> 链接器 -> 输入 -> 附加依赖项`
   - 添加: `xiaoyao.lib`
4. 将 `bin/xiaoyao.dll` 复制到输出目录

## 快速示例
```cpp
#include "xiaoyao/xiaoyao.h"

int main() {
    xiaoyao::DexHand hand;

    // 连接设备
    if (!hand.Connect(xiaoyao::CommType::ETHERCAT, "auto")) {
        printf("连接失败\n");
        return -1;
    }

    // 注册回调
    hand.SetJointsCallback([](const std::vector<xiaoyao::Joint>& joints) {
        printf("收到关节数据\n");
    });

    // 控制运动
    std::vector<xiaoyao::JointCommand> commands = {
        {xiaoyao::JointId::THUMB_MCP, 45.0f, 50, 50}
    };
    hand.MoveJoints(commands);

    // 断开连接
    hand.Disconnect();
    return 0;
}
```

## 系统要求
- Windows 7 或更高版本
- Visual Studio 2017 或更高版本
- CMake 3.5 或更高版本

## 技术支持
- Email: csi@glitech.com
EOF

# 打包
echo "Creating package..."
cd package
zip -r "${SDK_NAME}.zip" "${SDK_NAME}"
cd ..

echo ""
echo "========================================="
echo "  Package created successfully!"
echo "  Location: package/${SDK_NAME}.zip"
echo "========================================="
