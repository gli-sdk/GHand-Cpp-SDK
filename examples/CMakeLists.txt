cmake_minimum_required(VERSION 3.5)

# 辅助函数：为示例程序添加自动复制DLL的post-build命令
function(copy_dll_for_target target_name)
    add_custom_command(TARGET ${target_name} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:xiaoyao>"
            "$<TARGET_FILE_DIR:${target_name}>"
        COMMENT "Copying xiaoyao.dll to ${target_name} directory"
    )
endfunction()

add_executable(basic_connection basic_connection.cc)
target_link_libraries(basic_connection xiaoyao)
target_include_directories(basic_connection PRIVATE ${CMAKE_SOURCE_DIR}/include)
copy_dll_for_target(basic_connection)

add_executable(tactile_callback tactile_callback.cc)
target_link_libraries(tactile_callback xiaoyao)
target_include_directories(tactile_callback PRIVATE ${CMAKE_SOURCE_DIR}/include)
copy_dll_for_target(tactile_callback)

add_executable(move_joints move_joints.cc)
target_link_libraries(move_joints xiaoyao)
target_include_directories(move_joints PRIVATE ${CMAKE_SOURCE_DIR}/include)
copy_dll_for_target(move_joints)