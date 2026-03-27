# 测试环境搭建

## 1. 安装 MinGW-w64（编译器 + make+gdb）
下载：[MinGW-w64](https://github.com/niXman/mingw-builds-binaries/releases)（选x86_64-15.2.0-release-win32-seh-ucrt-rt_v13-rev1.7z）
解压到 C:\mingw64
配置环境变量：把 C:\mingw64\bin 加入系统 Path
gcc -v
g++ -v
gdb -v
mingw32-make -v

## 2. 安装 CMake
下载：CMake（Windows x64 Installer）https://cmake.org/download/
安装时勾选 Add CMake to the system PATH
验证：
cmake --version

## 3. 安装 VS Code 插件
C/C++（Microsoft）：语法、调试、智能提示
CMake Tools（Microsoft）：CMake 项目管理、一键构建 / 运行
GoogleTest Adapter：在 VS Code “测试” 面板运行 / 查看 GTest 结果
Coverage Gutters：编辑器内显示覆盖率高亮

## 4. 安装 gcovr（覆盖率报告）
pip install gcovr
# 验证
gcovr --version

## 5. 获取 googletest 源码
项目根目录执行
git clone https://github.com/google/googletest.git  #这个很慢
git clone https://gitee.com/hongjie-zhang0507/googletest.git  #自己在gitee上找，下载快一些

## 6. 安装xunit-viewer（生成html格式的测试报告）
先安装 Node.js（若未安装）：https://nodejs.org/（选 LTS 版本）
验证 Node.js
node -v
npm -v
全局安装 xunit-viewer
npm install -g xunit-viewer

# 项目结构（推荐）

gtest_demo/
├── .vscode/               # VS Code 配置
│   ├── c_cpp_properties.json
│   ├── launch.json
│   ├── settings.json
│   └── tasks.json
├── src/                   # 业务代码
│   ├── calc.h
│   └── calc.cpp
├── test/                  # 测试代码（GTest+GMock）
│   ├── mock_calc.h
│   └── calc_test.cpp
├── googletest/            # 源码目录
├── CMakeLists.txt         # 根 CMake
└── build/                 # 构建目录（自动生成）

# 构建、运行、测试、覆盖率

## 1. 构建项目（两种方式）
方式 1：终端（推荐）
bash运行
cd build
cmake .. -G "MinGW Makefiles" -DENABLE_COVERAGE=ON;mingw32-make -j8

cmake	                CMake 核心命令	跨平台构建工具，负责把 CMakeLists.txt 翻译成对应编译器的构建文件
..	                    指定 CMakeLists.txt 所在目录	.. 表示「上级目录」（因为你当前在 build 目录，CMakeLists.txt 在项目根目录）
-G "MinGW Makefiles"	指定生成器（Generator）	告诉 CMake：「我要用 MinGW 的 make 工具构建，生成 MinGW 能识别的 Makefile 文件」
                        ⚠️ 注意：
                        - 必须加引号，因为名称有空格；
                        - 区分大小写（MinGW Makefiles 不能写成 mingw makefiles）；
                        - 如果不加 -G，CMake 可能默认生成 Visual Studio 的工程文件，和 MinGW 不兼容
-DENABLE_COVERAGE=ON	定义 CMake 宏 / 选项	传递自定义参数给 CMakeLists.txt，对应我们之前配置的 option(ENABLE_COVERAGE ...)
                        作用：开启「覆盖率编译选项」（-fprofile-arcs -ftest-coverage），让编译后的程序能生成覆盖率数据
mingw32-make	        MinGW 的 make 工具	替代 Linux 下的 make，负责解析 Makefile 并调用 g++ 编译代码
-j8	                    指定编译线程数	-j = --jobs，表示「用 8 个线程并行编译」
                        大幅提升编译速度（线程数建议等于 CPU 核心数，比如 4 核 8 线程就用 -j8，4 核 4 线程用 -j4）



## 2. 运行测试
方式 1：终端
bash运行
cd build
calc_test.exe
生成 JUnit 报告（用于 CI）
.\calc_test.exe --gtest_output=xml:test_report.xml --gtest_color=yes

## 3. 生成覆盖率报告
bash运行
cd build
生成 HTML 报告（含行级详情）
先切换到根目录
gcovr -r . --html --html-details -o test/coverage/coverage.html --exclude test/ --exclude googletest/
命令片段	作用说明
gcovr	    核心命令基于 gcov 封装的覆盖率报告工具，比原生 gcov 更易用，支持多格式输出
-r ..	    指定根目录（root）	.. 表示「上级目录」（即项目根目录）出现绝对路径）
--html	        输出 HTML 格式报告	生成人类可读的网页版报告，而非纯文本 / XML 格式
--html-details	    生成行级详情的 HTML 报告	关
-o coverage.html	    指定输出文件（output）	把主报告保存为 coverage.html（行级详情文件会自动生成在同目录）
--exclude test/	        排除 test/ 目录	不统计测试代码本身的覆盖率（我们只关心业务代码）
--exclude googletest/	    排除 googletest/ 目录	不统计 GTest/GMock 源码的覆盖率（第三方库无需统计）
打开报告
start coverage.html

## 4. 编辑器内覆盖率高亮
安装 Coverage Gutters
构建后在编辑器右键 → Coverage Gutters: Display Coverage
绿 = 覆盖、红 = 未覆盖

## 5. 生成html格式的测试报告
xunit-viewer --results=test_report.xml --output=test_report.html --title="C++ SDK 测试报告"
