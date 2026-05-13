cmake_minimum_required(VERSION 3.5)
project(GHandSDK VERSION 1.0.0 LANGUAGES C CXX)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置 Windows 特定的编译选项
if(WIN32)
    add_compile_definitions(_WIN32)
    add_compile_definitions(_WINSOCKAPI_)
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
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/comm)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/internal)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/third_party/include)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/third_party/include/wpcap)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/third_party/include/zlgcan)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/third_party/include/soem/win32)

    # 链接 SOEM 的库文件
    link_directories(${CMAKE_CURRENT_SOURCE_DIR}/third_party/lib)

elseif(UNIX AND NOT APPLE)
    # 添加头文件目录
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/comm)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/internal)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/third_party/include)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/third_party/include/soem/linux)

    # 依赖检测
    find_package(Threads REQUIRED)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(PCAP REQUIRED libpcap)
    pkg_check_modules(OPENSSL REQUIRED openssl)
endif()

# 从 zlgcan.h 自动生成设备类型映射表，避免手动维护宏名映射
set(ZLGCAN_H "${CMAKE_CURRENT_SOURCE_DIR}/third_party/include/zlgcan/zlgcan.h")
set(GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY ${GENERATED_DIR})

file(STRINGS ${ZLGCAN_H} ZLGCAN_LINES REGEX "^#define ZCAN_[A-Za-z0-9_]+[ \t]+[0-9]+")
set(DEVICE_TYPE_INC "")
foreach(line ${ZLGCAN_LINES})
    string(REGEX MATCH "ZCAN_[A-Za-z0-9_]+" MACRO_NAME ${line})
    if(MACRO_NAME)
        set(DEVICE_TYPE_INC "${DEVICE_TYPE_INC}XX(${MACRO_NAME})\n")
    endif()
endforeach()

set(GENERATED_INC "${GENERATED_DIR}/zlgcan_device_types.inc")
file(WRITE ${GENERATED_INC} "${DEVICE_TYPE_INC}")
include_directories(${GENERATED_DIR})

# 源文件列表（显式列出，避免 GLOB_RECURSE 在新增/删除文件时不同步）
set(SOURCES
    src/dexhand.cc
    src/logging.cc
    src/types.cc
    src/comm/canfd_comm.cc
    src/comm/canfd_driver_zlg.cc
    src/comm/canfd_protocol.cc
    src/comm/ethercat_comm.cc
    src/internal/dexhand.cc
    src/internal/dexhand_callback_manager.cc
    src/internal/file_lock.cc
    src/internal/product_config_loader.cc
)

# 创建动态库
add_library(ghand SHARED ${SOURCES})
target_compile_definitions(ghand PRIVATE GHAND_BUILD)

# 链接依赖库
if(WIN32)
    target_link_libraries(ghand PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/lib/windows/soem.lib
        winmm ws2_32
        setupapi cfgmgr32
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/lib/windows/Packet.lib
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/lib/windows/wpcap.lib
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/lib/windows/zlgcan.lib
    )
else()
    # Linux: 使用预编译库
    target_link_libraries(ghand PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/lib/linux/libsoem.a
        Threads::Threads
        ${PCAP_LIBRARIES}
        ssl crypto rt dl
    )
endif()

# 设置输出名称和版本信息
set_target_properties(ghand PROPERTIES
    OUTPUT_NAME ghand
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
)

# ========== 自动复制运行时 DLL 和配置文件到输出目录 ==========
if(WIN32)
    file(GLOB RUNTIME_DLLS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/lib/windows/*.dll")
    if(RUNTIME_DLLS)
        add_custom_command(TARGET ghand POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${RUNTIME_DLLS}
            $<TARGET_FILE_DIR:ghand>
            COMMENT "Copying runtime DLLs to output directory"
        )
    endif()
endif()

# 复制配置文件到输出目录（开发时自动从源码 config/ 同步）
# GetSdkRootFromModule() 从 DLL 路径反推 SDK root（DLL 目录的父目录）
# 因此 config 需放在 DLL 目录的父目录下：
#   主库 DLL: build/<config>/ghand.dll  → SDK root = build/  → config at build/config/
#   示例 DLL: build/examples/<config>/   → SDK root = build/examples/ → config at build/examples/config/
add_custom_command(TARGET ghand POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/config"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/config/XIAOYAO-Hand.json"
        "${CMAKE_BINARY_DIR}/config/"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/examples/config"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/config/XIAOYAO-Hand.json"
        "${CMAKE_BINARY_DIR}/examples/config/"
    COMMENT "Copying config files to build/config/ and build/examples/config/"
)

# ========== 示例程序 ==========
add_subdirectory(examples)

# ========== 安装规则 ==========
install(TARGETS ghand
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

# 安装公共头文件
install(DIRECTORY include/ghand
    DESTINATION include/ghand
    FILES_MATCHING PATTERN "*.h"
)

# 安装产品配置文件
install(DIRECTORY config/
    DESTINATION share/ghand-sdk/config
    FILES_MATCHING PATTERN "*.json"
)

# 安装文档
install(FILES README.md CHANGELOG.md
    DESTINATION .
)

