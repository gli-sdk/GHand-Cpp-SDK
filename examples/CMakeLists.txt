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

# 01. 基础连接示例
add_executable(basic_connection tutorial/basic_connection.cc)
target_link_libraries(basic_connection xiaoyao)
target_include_directories(basic_connection PRIVATE ${CMAKE_SOURCE_DIR}/include)
copy_dll_for_target(basic_connection)

# 02. 关节运动示例
add_executable(move_joints tutorial/move_joints.cc)
target_link_libraries(move_joints xiaoyao)
target_include_directories(move_joints PRIVATE ${CMAKE_SOURCE_DIR}/include)
copy_dll_for_target(move_joints)

# 03. 触觉回调示例
add_executable(tactile_callback tutorial/tactile_callback.cc)
target_link_libraries(tactile_callback xiaoyao)
target_include_directories(tactile_callback PRIVATE ${CMAKE_SOURCE_DIR}/include)
copy_dll_for_target(tactile_callback)

# 04. 预设手势示例
add_executable(preset_gesture demo/preset_gesture.cc)
target_link_libraries(preset_gesture xiaoyao)
target_include_directories(preset_gesture PRIVATE ${CMAKE_SOURCE_DIR}/include)
copy_dll_for_target(preset_gesture)

# 05. 多手控制示例
add_executable(multi_hand tutorial/multi_hand.cc)
target_link_libraries(multi_hand xiaoyao)
target_include_directories(multi_hand PRIVATE ${CMAKE_SOURCE_DIR}/include)
copy_dll_for_target(multi_hand)

# 06. 交互式关节控制示例
add_executable(interactive_joint_control tutorial/interactive_joint_control.cc)
target_link_libraries(interactive_joint_control xiaoyao)
target_include_directories(interactive_joint_control PRIVATE ${CMAKE_SOURCE_DIR}/include)
copy_dll_for_target(interactive_joint_control)

# 07. 数据手套控制示例 (仅 Windows)
if(WIN32)
    add_executable(glove_control tutorial/glove_control.cc)
    target_link_libraries(glove_control xiaoyao ws2_32)
    target_include_directories(glove_control PRIVATE ${CMAKE_SOURCE_DIR}/include)
    copy_dll_for_target(glove_control)
endif()

# 手势舞演示
add_executable(gesture_dance demo/gesture_dance.cc)
target_link_libraries(gesture_dance xiaoyao)
target_include_directories(gesture_dance PRIVATE ${CMAKE_SOURCE_DIR}/include)
copy_dll_for_target(gesture_dance)

# 抓取动作演示
add_executable(grabbing_action demo/grabbing_action.cc)
target_link_libraries(grabbing_action xiaoyao)
target_include_directories(grabbing_action PRIVATE ${CMAKE_SOURCE_DIR}/include)
copy_dll_for_target(grabbing_action)

# 按压动作演示
add_executable(press_action demo/press_action.cc)
target_link_libraries(press_action xiaoyao)
target_include_directories(press_action PRIVATE ${CMAKE_SOURCE_DIR}/include)
copy_dll_for_target(press_action)

# 拍手动作演示
add_executable(clap_action demo/clap_action.cc)
target_link_libraries(clap_action xiaoyao)
target_include_directories(clap_action PRIVATE ${CMAKE_SOURCE_DIR}/include)
copy_dll_for_target(clap_action)

# 握持动作演示
add_executable(hold_action demo/hold_action.cc)
target_link_libraries(hold_action xiaoyao)
target_include_directories(hold_action PRIVATE ${CMAKE_SOURCE_DIR}/include)
copy_dll_for_target(hold_action)

# 敲击动作演示
add_executable(knock_action demo/knock_action.cc)
target_link_libraries(knock_action xiaoyao)
target_include_directories(knock_action PRIVATE ${CMAKE_SOURCE_DIR}/include)
copy_dll_for_target(knock_action)

# 拎起动作演示
add_executable(lift_action demo/lift_action.cc)
target_link_libraries(lift_action xiaoyao)
target_include_directories(lift_action PRIVATE ${CMAKE_SOURCE_DIR}/include)
copy_dll_for_target(lift_action)

# 拉动动作演示
add_executable(pull_action demo/pull_action.cc)
target_link_libraries(pull_action xiaoyao)
target_include_directories(pull_action PRIVATE ${CMAKE_SOURCE_DIR}/include)
copy_dll_for_target(pull_action)

# 支撑动作演示
add_executable(support_action demo/support_action.cc)
target_link_libraries(support_action xiaoyao)
target_include_directories(support_action PRIVATE ${CMAKE_SOURCE_DIR}/include)
copy_dll_for_target(support_action)