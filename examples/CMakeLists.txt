cmake_minimum_required(VERSION 3.5)

add_executable(basic_connection basic_connection.cc)
target_link_libraries(basic_connection xiaoyao)
target_include_directories(basic_connection PRIVATE ${CMAKE_SOURCE_DIR}/include)

add_executable(tactile_callback tactile_callback.cc)
target_link_libraries(tactile_callback xiaoyao)
target_include_directories(tactile_callback PRIVATE ${CMAKE_SOURCE_DIR}/include)

add_executable(move_joints move_joints.cc)
target_link_libraries(move_joints xiaoyao)
target_include_directories(move_joints PRIVATE ${CMAKE_SOURCE_DIR}/include)