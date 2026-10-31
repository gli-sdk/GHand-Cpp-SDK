# Windows 发布程序

`windows/` 提供 GHand SDK 3.1.0 的 Windows x64 Release 示例程序，使用
MSVC 2022 和 Qt 6.9.1 编译。教程程序保留源码的数字前缀，动作演示使用原名称。

请完整复制 `windows/` 文件夹，在该目录中运行程序。例如：

```powershell
cd dist/windows
.\11_connect_hands.exe --help
.\01_basic_connection.exe
.\15_tactile_display.exe
```

- `ghand.dll`、`modbus.dll`：SDK 及 RS485 运行库。
- `Qt6*.dll`、`platforms/` 等目录：触觉显示所需 Qt 运行库和插件，无需安装 Qt。
- `msvcp140*.dll`、`vcruntime140*.dll` 等：MSVC x64 运行库。
- `config/`：产品配置；保持与程序的相对位置。
- `LICENSES/`、`LICENSE`、`THIRD_PARTY_NOTICES.md`：许可和第三方声明。
- `SHA256SUMS.txt`：发布文件校验值，不包含校验文件自身。

连接实际设备前请阅读仓库根目录的 `README.zh.md`。EtherCAT 需要在目标机器
安装兼容 WinPcap 的 x64 抓包驱动（例如启用 WinPcap 兼容模式的 Npcap），并可能
需要管理员权限；本目录不包含系统驱动。CANFD/RS485 适配器需要相应设备驱动。
这些示例会操作实际硬件，请确认设备连接和运动空间后运行。

重新构建（需要本机 Qt 开发套件，按实际路径修改）：

```powershell
cmake -S . -B build-dist -A x64 -DGHAND_BUILD_TESTS=OFF -DCMAKE_PREFIX_PATH=C:/Qt/6.9.1/msvc2022_64
cmake --build build-dist --config Release --parallel 4
```

程序来自 `build-dist/examples/Release/`，SDK DLL 来自
`build-dist/Release/`；Qt 依赖由同一套件的 `windeployqt --release` 部署。
当前包已通过编译、DLL 静态依赖检查和命令行帮助启动检查；设备通信和运动需在硬件上验证。
