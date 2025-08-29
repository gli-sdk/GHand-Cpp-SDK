cmake_minimum_required(VERSION 3.5)

add_executable(basic_connection basic_connection.cc)
target_link_libraries(basic_connection xiaoyao)
target_include_directories(basic_connection PRIVATE ${CMAKE_SOURCE_DIR}/include)