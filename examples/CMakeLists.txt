cmake_minimum_required(VERSION 3.10)

# Copy the shared library and its runtime dependencies next to an example.
function(copy_runtime_for_target target_name)
    add_custom_command(TARGET ${target_name} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:ghand>"
            "$<TARGET_FILE_DIR:${target_name}>"
        COMMENT "Copying ghand.dll to ${target_name} directory"
    )

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

function(add_ghand_example target_name source_file)
    add_executable(${target_name} ${source_file})
    target_link_libraries(${target_name} ghand)
    copy_runtime_for_target(${target_name})
endfunction()

# Tutorials, numbered in the same order as their filenames.
add_ghand_example(basic_connection tutorial/01_basic_connection.cc)
add_ghand_example(move_joints tutorial/02_move_joints.cc)
add_ghand_example(speed_control tutorial/03_speed_control.cc)
add_ghand_example(current_control tutorial/04_current_control.cc)
add_ghand_example(tactile_callback tutorial/05_tactile_callback.cc)
add_ghand_example(multi_hand tutorial/06_multi_hand.cc)
add_ghand_example(glove_control tutorial/07_glove_control.cc)
add_ghand_example(interactive_joint_control
    tutorial/08_interactive_joint_control.cc)
add_ghand_example(hand_state tutorial/09_hand_state.cc)
add_ghand_example(switch_baud_rate tutorial/10_switch_baud_rate.cc)
add_ghand_example(connect_hands tutorial/11_connect_hands.cc)
add_ghand_example(mit_control tutorial/13_mit_control.cc)
add_ghand_example(custom_control tutorial/14_custom_control.cc)

option(GHAND_BUILD_TACTILE_DISPLAY "Build the optional Qt 6 tactile display" OFF)
if(GHAND_BUILD_TACTILE_DISPLAY)
    find_package(Qt6 6.2 REQUIRED COMPONENTS Widgets)
    add_library(ghand_tactile_display STATIC
        ../src/tactile_display/tactile_display.cc
        ../src/tactile_display/tactile_widgets.cc
        ../src/tactile_display/tactile_assets.qrc)
    set_target_properties(ghand_tactile_display PROPERTIES AUTORCC ON)
    target_include_directories(ghand_tactile_display PUBLIC ${PROJECT_SOURCE_DIR}/src)
    target_compile_features(ghand_tactile_display PUBLIC cxx_std_17)
    target_link_libraries(ghand_tactile_display PUBLIC ghand Qt6::Widgets)
    add_ghand_example(tactile_display tutorial/15_tactile_display.cc)
    target_link_libraries(tactile_display ghand_tactile_display)
    if(GHAND_BUILD_TESTS)
        add_ghand_example(tactile_display_test ../tests/tactile_display_test.cc)
        target_include_directories(tactile_display_test PRIVATE ${PROJECT_SOURCE_DIR}/src)
        target_compile_features(tactile_display_test PRIVATE cxx_std_17)
        target_link_libraries(tactile_display_test ghand_tactile_display)
        add_test(NAME tactile_display_test COMMAND tactile_display_test)
        set_tests_properties(tactile_display_test PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 10)
        if(WIN32)
            set_property(TEST tactile_display_test APPEND PROPERTY
                ENVIRONMENT "QT_QPA_FONTDIR=$ENV{WINDIR}/Fonts")
        endif()
    endif()
endif()

# The glove demo uses Winsock on Windows and BSD sockets elsewhere.
if(WIN32)
    target_link_libraries(glove_control ws2_32)
endif()

# Action demos.
add_ghand_example(preset_gesture demo/preset_gesture.cc)
add_ghand_example(gesture_dance demo/gesture_dance.cc)
add_ghand_example(grabbing_action demo/grabbing_action.cc)
add_ghand_example(press_action demo/press_action.cc)
add_ghand_example(clap_pose demo/clap_pose.cc)
add_ghand_example(hold_action demo/hold_action.cc)
add_ghand_example(knock_action demo/knock_action.cc)
add_ghand_example(lift_action demo/lift_action.cc)
add_ghand_example(pull_action demo/pull_action.cc)
add_ghand_example(support_action demo/support_action.cc)
