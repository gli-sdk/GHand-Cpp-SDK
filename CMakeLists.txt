cmake_minimum_required(VERSION 3.16)

project(GHandSDK VERSION 2.2.1 LANGUAGES C CXX)
include(CMakePackageConfigHelpers)

# Set C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Windows-specific compile options
if(WIN32)
    add_compile_definitions(_WIN32)
    add_compile_definitions(WIN32_LEAN_AND_MEAN)
    add_compile_definitions(NOMINMAX)
    # Set UTF-8 encoding to support Chinese strings
    if(MSVC)
        add_compile_options("/utf-8")
    endif()
elseif(UNIX AND NOT APPLE)
    add_compile_definitions(__linux__)
endif()

# ========== Dependencies ==========

set(DEPS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/deps")
set(DEPS_INCLUDE_DIR "${DEPS_ROOT}/include")
if(WIN32)
    set(DEPS_LIB_DIR "${DEPS_ROOT}/lib/windows")
elseif(UNIX AND NOT APPLE)
    set(DEPS_LIB_DIR "${DEPS_ROOT}/lib/linux")
else()
    message(FATAL_ERROR "Unsupported platform for deps layout")
endif()

if(NOT EXISTS "${DEPS_INCLUDE_DIR}" OR NOT EXISTS "${DEPS_LIB_DIR}")
    message(FATAL_ERROR
        "deps directory not found. Download the release package with deps/.\n"
        "Expected:\n"
        "  ${DEPS_INCLUDE_DIR}\n"
        "  ${DEPS_LIB_DIR}"
    )
endif()

find_path(NLOHMANN_JSON_INCLUDE_DIR nlohmann/json.hpp
    PATHS "${DEPS_INCLUDE_DIR}"
    NO_DEFAULT_PATH
)
if(NOT NLOHMANN_JSON_INCLUDE_DIR)
    message(FATAL_ERROR "nlohmann/json.hpp not found in deps/include")
endif()

# Linux system libraries
if(UNIX AND NOT APPLE)
    find_package(Threads REQUIRED)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(OPENSSL REQUIRED openssl)
    pkg_check_modules(LIBMODBUS REQUIRED IMPORTED_TARGET libmodbus)
endif()

# SOEM (required for EtherCAT)
if(WIN32)
    set(SOEM_LIBRARY "${DEPS_LIB_DIR}/soem.lib")
else()
    set(SOEM_LIBRARY "${DEPS_LIB_DIR}/libsoem.a")
endif()
set(SOEM_INCLUDE_DIR "${DEPS_INCLUDE_DIR}")
if(NOT EXISTS "${SOEM_LIBRARY}")
    message(FATAL_ERROR "SOEM library not found in ${DEPS_LIB_DIR}")
endif()
if(NOT EXISTS "${SOEM_INCLUDE_DIR}/soem/soem.h")
    message(FATAL_ERROR "SOEM headers not found in ${DEPS_INCLUDE_DIR}")
endif()
if(WIN32)
    set(SOEM_PLATFORM_INCLUDE_DIR "${SOEM_INCLUDE_DIR}/soem/win32")
elseif(UNIX AND NOT APPLE)
    set(SOEM_PLATFORM_INCLUDE_DIR "${SOEM_INCLUDE_DIR}/soem/linux")
endif()

# WinPcap / Packet (Windows only, required for EtherCAT)
if(WIN32)
    set(WPCAP_LIBRARY "${DEPS_LIB_DIR}/wpcap.lib")
    set(PACKET_LIBRARY "${DEPS_LIB_DIR}/Packet.lib")
    set(WPCAP_INCLUDE_DIR "${DEPS_INCLUDE_DIR}/wpcap")
    if(NOT EXISTS "${WPCAP_LIBRARY}" OR NOT EXISTS "${PACKET_LIBRARY}")
        message(FATAL_ERROR "WinPcap/Packet library not found in ${DEPS_LIB_DIR}")
    endif()
    if(NOT EXISTS "${WPCAP_INCLUDE_DIR}/pcap.h")
        message(FATAL_ERROR "WinPcap headers not found in ${WPCAP_INCLUDE_DIR}")
    endif()
endif()

# libmodbus (required for RS485)
# libmodbus is LGPL-2.1-or-later. Keep it dynamically linked in the
# official SDK distribution so users can replace the LGPL library binary.
if(WIN32)
    set(LIBMODBUS_LIBRARY "${DEPS_LIB_DIR}/libmodbus.lib")
    set(LIBMODBUS_INCLUDE_DIR "${DEPS_INCLUDE_DIR}")
    if(NOT EXISTS "${LIBMODBUS_INCLUDE_DIR}/modbus/modbus.h" OR NOT EXISTS "${LIBMODBUS_LIBRARY}")
        message(FATAL_ERROR "libmodbus not found in deps")
    endif()
    set(LIBMODBUS_INCLUDE_DIRS ${LIBMODBUS_INCLUDE_DIR})
    set(LIBMODBUS_LIBRARIES ${LIBMODBUS_LIBRARY})
else()
    set(LIBMODBUS_INCLUDE_DIRS)
    set(LIBMODBUS_LIBRARIES PkgConfig::LIBMODBUS)
endif()
set(LIBMODBUS_FOUND TRUE)

set(GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY ${GENERATED_DIR})

# Source file list (explicitly listed to avoid GLOB_RECURSE desync on add/remove)
set(SOURCES
    src/ghand.cc
    src/logging.cc
    src/types.cc
    src/comm/canfd_comm.cc
    src/comm/canfd_driver_zlg.cc
    src/comm/canfd_protocol.cc
    src/comm/ethercat_comm.cc
    src/comm/modbus_codec.cc
    src/comm/rs485_comm.cc
    src/comm/self_test_workflow.cc
    src/internal/ghand_impl.cc
    src/internal/ghand_callback_manager.cc
    src/internal/file_lock.cc
    src/internal/product_config_loader.cc
)

# Create shared library
add_library(ghand SHARED ${SOURCES})
add_library(GHand::ghand ALIAS ghand)
target_compile_definitions(ghand PRIVATE GHAND_BUILD)

target_include_directories(ghand
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/comm
        ${CMAKE_CURRENT_SOURCE_DIR}/src/internal
        ${GENERATED_DIR}
        ${NLOHMANN_JSON_INCLUDE_DIR}
        ${LIBMODBUS_INCLUDE_DIRS}
        ${SOEM_INCLUDE_DIR}
        ${SOEM_PLATFORM_INCLUDE_DIR}
        $<$<BOOL:${WIN32}>:${WPCAP_INCLUDE_DIR}>
)

# Link dependencies
target_link_libraries(ghand PRIVATE
    ${SOEM_LIBRARY}
    $<$<BOOL:${WIN32}>:${WPCAP_LIBRARY}>
    $<$<BOOL:${WIN32}>:${PACKET_LIBRARY}>
    ${LIBMODBUS_LIBRARIES}
    $<$<BOOL:${UNIX}>:Threads::Threads>
    $<$<BOOL:${UNIX}>:ssl>
    $<$<BOOL:${UNIX}>:crypto>
    $<$<BOOL:${UNIX}>:rt>
    $<$<BOOL:${UNIX}>:dl>
    $<$<BOOL:${WIN32}>:winmm>
    $<$<BOOL:${WIN32}>:ws2_32>
    $<$<BOOL:${WIN32}>:setupapi>
)

# Set output name and version info
set_target_properties(ghand PROPERTIES
    OUTPUT_NAME ghand
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
)

if(MSVC)
    set_property(TARGET ghand PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
    )
endif()

# ========== Runtime DLLs ==========
set(RUNTIME_DLLS)

if(WIN32)
    # libmodbus DLL (if found alongside the import library in deps/lib/windows)
    if(LIBMODBUS_FOUND)
        get_filename_component(LIBMODBUS_LIB_DIR "${LIBMODBUS_LIBRARIES}" DIRECTORY)
        file(GLOB LIBMODBUS_DLLS
            "${LIBMODBUS_LIB_DIR}/libmodbus.dll"
            "${LIBMODBUS_LIB_DIR}/modbus.dll"
            "${LIBMODBUS_LIB_DIR}/../bin/libmodbus.dll"
            "${LIBMODBUS_LIB_DIR}/../bin/modbus.dll"
        )
        if(LIBMODBUS_DLLS)
            list(APPEND RUNTIME_DLLS ${LIBMODBUS_DLLS})
        endif()
    endif()
endif()

# Store runtime DLL list as target property for examples to access
set_target_properties(ghand PROPERTIES RUNTIME_DLL_LIST "${RUNTIME_DLLS}")

# Copy runtime DLLs to ghand output directory
foreach(dll ${RUNTIME_DLLS})
    if(EXISTS "${dll}")
        add_custom_command(TARGET ghand POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${dll}"
                "$<TARGET_FILE_DIR:ghand>"
        )
    endif()
endforeach()

# Copy config files to output directory
add_custom_command(TARGET ghand POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/config"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/config/ghand5.json"
        "${CMAKE_BINARY_DIR}/config/"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/config/ghandlite1.json"
        "${CMAKE_BINARY_DIR}/config/"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/examples/config"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/config/ghand5.json"
        "${CMAKE_BINARY_DIR}/examples/config/"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/config/ghandlite1.json"
        "${CMAKE_BINARY_DIR}/examples/config/"
    COMMENT "Copying config files to build/config/ and build/examples/config/"
)

# ========== Examples ==========
add_subdirectory(examples)

# ========== Install rules ==========
install(TARGETS ghand
    EXPORT GHandSDKTargets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)

# Install public headers
install(DIRECTORY include/ghand
    DESTINATION include
    FILES_MATCHING PATTERN "*.h"
)

# Install product config files
install(DIRECTORY config/
    DESTINATION share/ghand-sdk/config
    FILES_MATCHING PATTERN "*.json"
)

# Install documentation
install(FILES
    README.md
    README.zh.md
    CHANGELOG.md
    LICENSE
    THIRD_PARTY_NOTICES.md
    DESTINATION .
)

install(FILES
    deps/SOEM_BUILD_INFO.md
    DESTINATION deps
)

install(DIRECTORY LICENSES/
    DESTINATION LICENSES
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/GHandSDKConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/GHandSDKConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/GHandSDKConfig.cmake"
    INSTALL_DESTINATION lib/cmake/GHandSDK
)

install(EXPORT GHandSDKTargets
    NAMESPACE GHand::
    FILE GHandSDKTargets.cmake
    DESTINATION lib/cmake/GHandSDK
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/GHandSDKConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/GHandSDKConfigVersion.cmake"
    DESTINATION lib/cmake/GHandSDK
)
