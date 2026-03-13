cmake_minimum_required(VERSION 3.5)
project(XiaoyaoSDK VERSION 1.0.0 LANGUAGES C CXX)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 11)
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
    else()
        # MinGW: 修复 timespec64_get 问题 (MinGW 13.x 兼容性)
        add_compile_definitions(_POSIX_C_SOURCE=200809L)
    endif()
endif()

# 添加头文件目录
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/internal)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include/wpcap)

# 链接 SOEM 的库文件
link_directories(${CMAKE_CURRENT_SOURCE_DIR}/lib)

# ========== 判断构建模式 ==========
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/dexhand.cc")
    # ========== 源代码构建模式 ==========
    message(STATUS "Building from source code")

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
    target_link_libraries(xiaoyao PRIVATE soem)
    target_link_libraries(xiaoyao PRIVATE winmm ws2_32)
    target_link_libraries(xiaoyao PRIVATE Packet wpcap)

    # 设置输出名称和版本信息
    set_target_properties(xiaoyao PROPERTIES
        OUTPUT_NAME xiaoyao
        VERSION ${PROJECT_VERSION}
        SOVERSION 1
    )

else()
    # ========== SDK 模式（预编译库）==========
    message(STATUS "Using pre-built library")

    # 使用预编译库
    add_library(xiaoyao SHARED IMPORTED)

    # 设置导入库和 DLL 路径
    set_target_properties(xiaoyao PROPERTIES
        IMPORTED_IMPLIB "${CMAKE_CURRENT_SOURCE_DIR}/lib/xiaoyao.lib"
        IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/bin/xiaoyao.dll"
    )
endif()

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

