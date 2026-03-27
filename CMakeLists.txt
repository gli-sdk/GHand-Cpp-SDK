cmake_minimum_required(VERSION 3.14)
project(gtest_demo)

# C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置 Windows 特定的编译选项
if(WIN32)
    add_compile_definitions(_WIN32)
    add_compile_definitions(_WINSOCKAPI_)
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
    # 设置 UTF-8 编码以支持中文字符串
    if(MSVC)
        add_compile_options("/utf-8")
        # 设置运行时库为动态链接 (Multi-threaded DLL)
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MD")
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MDd")
        set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /MD")
        set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /MDd")
    endif()
elseif(UNIX AND NOT APPLE)
    add_compile_definitions(__linux__)
endif()

if(WIN32)
    # 添加头文件目录
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/internal)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include/wpcap)

    # 链接 SOEM 的库文件
    link_directories(${CMAKE_CURRENT_SOURCE_DIR}/lib)

elseif(UNIX AND NOT APPLE)
    # 添加头文件目录
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/internal)

    # 依赖检测
    find_package(Threads REQUIRED)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(PCAP REQUIRED libpcap)
    pkg_check_modules(OPENSSL REQUIRED openssl)
endif()

# 查找源文件
file(GLOB_RECURSE SOURCES
    "src/*.cpp"
    "src/*.cc"
    "src/*.c"
    "src/internal/*.cpp"
    "src/internal/*.cc"
    "src/internal/*.c"
)

# 创建动态库
add_library(xiaoyao SHARED ${SOURCES})

# 链接依赖库
if(WIN32)
    target_link_libraries(xiaoyao PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/windows/soem.lib
        winmm ws2_32
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/windows/Packet.lib
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/windows/wpcap.lib
    )
else()
    # Linux: 使用预编译库
    target_link_libraries(xiaoyao PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/linux/libsoem.a
        Threads::Threads
        ${PCAP_LIBRARIES}
        ssl crypto rt dl
    )
endif()

# 设置输出名称和版本信息
set_target_properties(xiaoyao PROPERTIES
    OUTPUT_NAME xiaoyao
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
)

# ========== 示例程序 ==========
add_subdirectory(examples)

# ========== 安装规则 ==========
install(TARGETS xiaoyao
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

# 安装公共头文件
install(DIRECTORY include/xiaoyao
    DESTINATION include/xiaoyao
    FILES_MATCHING PATTERN "*.h"
)

# 安装文档
install(FILES README.md CHANGELOG.md
    DESTINATION .
)

# 测试命令
enable_testing()
add_test(NAME CalcTest COMMAND calc_test)