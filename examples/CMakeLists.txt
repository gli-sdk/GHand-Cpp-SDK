cmake_minimum_required(VERSION 3.5)

# 辅助函数：为示例程序添加自动复制DLL的post-build命令
function(copy_dll_for_target target_name)
    add_custom_command(TARGET ${target_name} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:ghand>"
            "$<TARGET_FILE_DIR:${target_name}>"
        COMMENT "Copying ghand.dll to ${target_name} directory"
    )
    if(WIN32 AND EXISTS "${CMAKE_SOURCE_DIR}/lib/windows/zlgcan.dll")
        add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_SOURCE_DIR}/lib/windows/zlgcan.dll"
                "$<TARGET_FILE_DIR:${target_name}>"
            COMMENT "Copying zlgcan.dll to ${target_name} directory"
        )
    endif()
    if(WIN32 AND EXISTS "${CMAKE_SOURCE_DIR}/lib/windows/kerneldlls")
        file(GLOB KERNEL_DLLS "${CMAKE_SOURCE_DIR}/lib/windows/kerneldlls/*.dll")
        if(KERNEL_DLLS)
            add_custom_command(TARGET ${target_name} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${KERNEL_DLLS}
                    "$<TARGET_FILE_DIR:${target_name}>"
                COMMENT "Copying kerneldlls to ${target_name} directory"
            )
        endif()
    endif()
endfunction()

# 01. 基础连接示例
add_executable(basic_connection tutorial/01basic_connection.cc)
target_link_libraries(basic_connection ghand)
target_include_directories(basic_connection PRIVATE ${CMAKE_SOURCE_DIR}/include)
copy_dll_for_target(basic_connection)

# 03. EtherCAT 关节运动示例
add_executable(move_joints tutorial/02move_joints.cc)
target_link_libraries(move_joints ghand)
target_include_directories(move_joints PRIVATE ${CMAKE_SOURCE_DIR}/include)
copy_dll_for_target(move_joints)

# 04. 触觉回调示例
add_executable(tactile_callback tutorial/05tactile_callback.cc)
target_link_libraries(tactile_callback ghand)
target_include_directories(tactile_callback PRIVATE ${CMAKE_SOURCE_DIR}/include)
copy_dll_for_target(tactile_callback)

# 05. 预设手势示例
add_executable(preset_gesture demo/preset_gesture.cc)
target_link_libraries(preset_gesture ghand)
target_include_directories(preset_gesture PRIVATE ${CMAKE_SOURCE_DIR}/include)
copy_dll_for_target(preset_gesture)

# 06. 多手控制示例
add_executable(multi_hand tutorial/06multi_hand.cc)
target_link_libraries(multi_hand ghand)
target_include_directories(multi_hand PRIVATE ${CMAKE_SOURCE_DIR}/include)
copy_dll_for_target(multi_hand)

# 07. 交互式关节控制示例
add_executable(interactive_joint_control tutorial/08interactive_joint_control.cc)
target_link_libraries(interactive_joint_control ghand)
target_include_directories(interactive_joint_control PRIVATE ${CMAKE_SOURCE_DIR}/include)
copy_dll_for_target(interactive_joint_control)

# 08. 数据手套控制示例 (仅 Windows)
if(WIN32)
    add_executable(glove_control tutorial/07glove_control.cc)
    target_link_libraries(glove_control ghand ws2_32)
    target_include_directories(glove_control PRIVATE ${CMAKE_SOURCE_DIR}/include)
    copy_dll_for_target(glove_control)
endif()

# 09. 速度控制示例
add_executable(speed_control tutorial/03speed_control.cc)
target_link_libraries(speed_control ghand)
target_include_directories(speed_control PRIVATE ${CMAKE_SOURCE_DIR}/include)
copy_dll_for_target(speed_control)

# 10. 力矩控制示例
add_executable(torque_control tutorial/04torque_control.cc)
target_link_libraries(torque_control ghand)
target_include_directories(torque_control PRIVATE ${CMAKE_SOURCE_DIR}/include)
copy_dll_for_target(torque_control)

# 手势舞演示
add_executable(gesture_dance demo/gesture_dance.cc)
target_link_libraries(gesture_dance ghand)
target_include_directories(gesture_dance PRIVATE ${CMAKE_SOURCE_DIR}/include)
copy_dll_for_target(gesture_dance)

# 抓取动作演示
add_executable(grabbing_action demo/grabbing_action.cc)
target_link_libraries(grabbing_action ghand)
target_include_directories(grabbing_action PRIVATE ${CMAKE_SOURCE_DIR}/include)
copy_dll_for_target(grabbing_action)

# 按压动作演示
add_executable(press_action demo/press_action.cc)
target_link_libraries(press_action ghand)
target_include_directories(press_action PRIVATE ${CMAKE_SOURCE_DIR}/include)
copy_dll_for_target(press_action)

# 拍手动作演示
add_executable(clap_action demo/clap_action.cc)
target_link_libraries(clap_action ghand)
target_include_directories(clap_action PRIVATE ${CMAKE_SOURCE_DIR}/include)
copy_dll_for_target(clap_action)

# 握持动作演示
add_executable(hold_action demo/hold_action.cc)
target_link_libraries(hold_action ghand)
target_include_directories(hold_action PRIVATE ${CMAKE_SOURCE_DIR}/include)
copy_dll_for_target(hold_action)

# 敲击动作演示
add_executable(knock_action demo/knock_action.cc)
target_link_libraries(knock_action ghand)
target_include_directories(knock_action PRIVATE ${CMAKE_SOURCE_DIR}/include)
copy_dll_for_target(knock_action)

# 拎起动作演示
add_executable(lift_action demo/lift_action.cc)
target_link_libraries(lift_action ghand)
target_include_directories(lift_action PRIVATE ${CMAKE_SOURCE_DIR}/include)
copy_dll_for_target(lift_action)

# 拉动动作演示
add_executable(pull_action demo/pull_action.cc)
target_link_libraries(pull_action ghand)
target_include_directories(pull_action PRIVATE ${CMAKE_SOURCE_DIR}/include)
copy_dll_for_target(pull_action)

# 支撑动作演示
add_executable(support_action demo/support_action.cc)
target_link_libraries(support_action ghand)
target_include_directories(support_action PRIVATE ${CMAKE_SOURCE_DIR}/include)
copy_dll_for_target(support_action)