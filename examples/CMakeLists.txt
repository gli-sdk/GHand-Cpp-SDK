cmake_minimum_required(VERSION 3.5)

# Helper function: add post-build commands to copy DLLs and config files for examples
function(copy_runtime_for_target target_name)
    # Copy ghand.dll
    add_custom_command(TARGET ${target_name} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:ghand>"
            "$<TARGET_FILE_DIR:${target_name}>"
        COMMENT "Copying ghand.dll to ${target_name} directory"
    )

    # Copy runtime DLLs attached to ghand target
    get_target_property(DLL_LIST ghand RUNTIME_DLL_LIST)
    if(DLL_LIST)
        foreach(dll_path ${DLL_LIST})
            if(EXISTS "${dll_path}")
                add_custom_command(TARGET ${target_name} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${dll_path}"
                        "$<TARGET_FILE_DIR:${target_name}>"
                )
            endif()
        endforeach()
    endif()
endfunction()

# 01. Basic connection example
add_executable(basic_connection tutorial/01basic_connection.cc)
target_link_libraries(basic_connection ghand)
copy_runtime_for_target(basic_connection)

# 02. EtherCAT joint movement example
add_executable(move_joints tutorial/02move_joints.cc)
target_link_libraries(move_joints ghand)
copy_runtime_for_target(move_joints)

# 03. Tactile callback example
add_executable(tactile_callback tutorial/05tactile_callback.cc)
target_link_libraries(tactile_callback ghand)
copy_runtime_for_target(tactile_callback)

# 04. Preset gesture example
add_executable(preset_gesture demo/preset_gesture.cc)
target_link_libraries(preset_gesture ghand)
copy_runtime_for_target(preset_gesture)

# 06. Multi-hand control example
add_executable(multi_hand tutorial/06multi_hand.cc)
target_link_libraries(multi_hand ghand)
copy_runtime_for_target(multi_hand)

# 06. Interactive joint control example
add_executable(interactive_joint_control tutorial/08interactive_joint_control.cc)
target_link_libraries(interactive_joint_control ghand)
copy_runtime_for_target(interactive_joint_control)

# 07. Data glove control example (Windows only)
if(WIN32)
    add_executable(glove_control tutorial/07glove_control.cc)
    target_link_libraries(glove_control ghand ws2_32)
    set_target_properties(glove_control PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/tutorial
    )
    copy_runtime_for_target(glove_control)
endif()

# 08. Speed control example
add_executable(speed_control tutorial/03speed_control.cc)
target_link_libraries(speed_control ghand)
copy_runtime_for_target(speed_control)

# 09. Torque control example
add_executable(torque_control tutorial/04torque_control.cc)
target_link_libraries(torque_control ghand)
copy_runtime_for_target(torque_control)

# Gesture dance demo
add_executable(gesture_dance demo/gesture_dance.cc)
target_link_libraries(gesture_dance ghand)
copy_runtime_for_target(gesture_dance)

# Grabbing action demo
add_executable(grabbing_action demo/grabbing_action.cc)
target_link_libraries(grabbing_action ghand)
copy_runtime_for_target(grabbing_action)

# Press action demo
add_executable(press_action demo/press_action.cc)
target_link_libraries(press_action ghand)
copy_runtime_for_target(press_action)

# Clap action demo
add_executable(clap_action demo/clap_action.cc)
target_link_libraries(clap_action ghand)
copy_runtime_for_target(clap_action)

# Hold action demo
add_executable(hold_action demo/hold_action.cc)
target_link_libraries(hold_action ghand)
copy_runtime_for_target(hold_action)

# Knock action demo
add_executable(knock_action demo/knock_action.cc)
target_link_libraries(knock_action ghand)
copy_runtime_for_target(knock_action)

# Lift action demo
add_executable(lift_action demo/lift_action.cc)
target_link_libraries(lift_action ghand)
copy_runtime_for_target(lift_action)

# Pull action demo
add_executable(pull_action demo/pull_action.cc)
target_link_libraries(pull_action ghand)
copy_runtime_for_target(pull_action)

# Support action demo
add_executable(support_action demo/support_action.cc)
target_link_libraries(support_action ghand)
copy_runtime_for_target(support_action)
