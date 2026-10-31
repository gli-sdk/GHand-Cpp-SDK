# 发布程序

## Linux

`linux/` 包含全部 Linux 示例、SDK、Qt 6 运行库和插件、共享库依赖及配置。
完整复制该目录后，可从任意工作目录直接调用入口：

```bash
# 部署后一次性设置权限（只在 setcap 时请求 sudo）：
python3 ./dist/linux/setup_ethercat.py
# 日常以普通用户运行：
./dist/linux/15_tactile_display
```

根目录为启动脚本，实际 ELF 程序位于 `bin/`，依赖在 `lib/`，Qt 插件在
`plugins/`。通过入口脚本运行，保留整个目录结构，无需手动设置库路径。

授权脚本需要 `patchelf` 和 `setcap`（Ubuntu 软件包 `patchelf`、`libcap2-bin`）。
默认只授权示例 15；其他示例例如：
`python3 dist/linux/setup_ethercat.py 01_basic_connection`。
脚本先将实际 ELF 的 DT_RPATH 设置为当前 `lib/` 绝对路径，再对 ELF 设置
`cap_net_raw=ep`，不授予 `CAP_NET_ADMIN`，也不以 root 运行 GUI。
这是为了兼容文件能力启用后的安全加载模式：该模式会忽略 `LD_LIBRARY_PATH`。
移动目录、重新打包或替换程序后需重新运行授权脚本；脚本会更新文件校验和。
可使用 `--prepare-only` 仅准备库路径并打印最后的授权命令。
使用 `getcap dist/linux/bin/15_tactile_display` 查看授权结果。

glibc、动态加载器、显卡/设备驱动及字体使用目标系统版本。目标机器需兼容
`linux/README.md` 中记录的架构和 glibc 版本。示例 15 连接 GHand5 后才显示窗口。

重新编译全部示例并打包（需要 SDK 构建依赖、Qt 6 Widgets 开发组件及 patchelf）：

```bash
python3 scripts/package_linux.py
# 自定义 Qt 路径：
python3 scripts/package_linux.py --qtpaths /path/to/Qt/bin/qtpaths6
```

脚本使用 `build-dist-linux/` 并重新生成 `dist/linux/`，附带许可、依赖包版本
`PACKAGES.tsv` 和 `SHA256SUMS.txt`。依赖许可收集针对 Debian/Ubuntu。

## Windows

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
