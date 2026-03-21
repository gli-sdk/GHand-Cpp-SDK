cmake_minimum_required(VERSION 3.14)
project(gtest_demo)

# C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 启用覆盖率选项
option(ENABLE_COVERAGE "Enable coverage reporting" OFF)
if(ENABLE_COVERAGE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fprofile-arcs -ftest-coverage")
endif()

# 包含头文件
include_directories(${PROJECT_SOURCE_DIR}/src)

# 编译业务库
add_library(calc_lib src/calc.cpp)
# add_library(calc_lib src/)

# 引入 googletest
add_subdirectory(googletest)
include_directories(${googletest_SOURCE_DIR}/googletest/include)
include_directories(${googletest_SOURCE_DIR}/googlemock/include)

# 编译测试程序
add_executable(calc_test test/calc_test.cpp)
target_link_libraries(calc_test calc_lib gtest gmock gtest_main)

# 测试命令
enable_testing()
add_test(NAME CalcTest COMMAND calc_test)