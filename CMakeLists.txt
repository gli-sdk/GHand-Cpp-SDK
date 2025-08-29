cmake_minimum_required(VERSION 3.5)
project(XiaoyaoSDK VERSION 1.0.0 LANGUAGES CXX)

# 设置c++标准
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置Windows特定的编译选型
if(WIN32)
    add_compile_definitions(_WIN32)
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

# 添加头文件目录
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# 查找源文件
file(GLOB_RECURSE SOURCES 
    "src/*.cpp"
    "src/*.cc"
    "src/*.h"
)

# 创建动态库
add_library(xiaoyao SHARED ${SOURCES})

# 设置输出名称和版本信息
set_target_properties(xiaoyao PROPERTIES
    OUTPUT_NAME xiaoyao
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
)

# 链接依赖库（如果需要）
# target_link_libraries(xiaoyao_sdk PRIVATE some_dependency)

# 添加示例子目录
add_subdirectory(examples)

# 安装规则
install(TARGETS xiaoyao
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

