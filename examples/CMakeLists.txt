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

# 添加 tutorial 可执行程序（输出到 tutorial/ 子目录）
function(add_tutorial_executable name)
    add_executable(${name} tutorial/${name}.cc)
    target_link_libraries(${name} xiaoyao)
    target_include_directories(${name} PRIVATE ${CMAKE_SOURCE_DIR}/include)
    set_target_properties(${name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/tutorial
    )
    copy_dll_for_target(${name})
endfunction()

# 添加 demo 可执行程序（输出到 demo/ 子目录）
function(add_demo_executable name)
    add_executable(${name} demo/${name}.cc)
    target_link_libraries(${name} xiaoyao)
    target_include_directories(${name} PRIVATE ${CMAKE_SOURCE_DIR}/include)
    set_target_properties(${name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/demo
    )
    copy_dll_for_target(${name})
endfunction()

# ========== TUTORIAL ==========
add_tutorial_executable(basic_connection)      # 01. 基础连接示例
add_tutorial_executable(move_joints)           # 02. 关节运动示例
add_tutorial_executable(tactile_callback)      # 03. 触觉回调示例
add_tutorial_executable(multi_hand)            # 05. 多手控制示例
add_tutorial_executable(interactive_joint_control) # 06. 交互式关节控制示例

# 07. 数据手套控制示例 (仅 Windows)
if(WIN32)
    add_executable(glove_control tutorial/glove_control.cc)
    target_link_libraries(glove_control xiaoyao ws2_32)
    target_include_directories(glove_control PRIVATE ${CMAKE_SOURCE_DIR}/include)
    set_target_properties(glove_control PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/tutorial
    )
    copy_dll_for_target(glove_control)
endif()

# ========== DEMO ==========
add_demo_executable(preset_gesture)            # 04. 预设手势示例
add_demo_executable(gesture_dance)             # 手势舞演示
add_demo_executable(grabbing_action)           # 抓取动作演示
add_demo_executable(press_action)              # 按压动作演示
add_demo_executable(clap_action)               # 拍手动作演示
add_demo_executable(hold_action)               # 握持动作演示
add_demo_executable(knock_action)              # 敲击动作演示
add_demo_executable(lift_action)               # 拎起动作演示
add_demo_executable(pull_action)               # 拉动动作演示
add_demo_executable(support_action)            # 支撑动作演示