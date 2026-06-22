cmake_minimum_required(VERSION 3.5)
project(GHandSDK VERSION 1.0.0 LANGUAGES C CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Windows-specific compile options
if(WIN32)
    add_compile_definitions(_WIN32)
    add_compile_definitions(_WINSOCKAPI_)
    # Set UTF-8 encoding to support Chinese strings
    if(MSVC)
        add_compile_options("/utf-8")
        # Set runtime library to dynamic link (Multi-threaded DLL)
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MD")
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MDd")
        set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /MD")
        set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /MDd")
    endif()
elseif(UNIX AND NOT APPLE)
    add_compile_definitions(__linux__)
endif()

# ========== Dependencies ==========

# Linux system libraries
if(UNIX AND NOT APPLE)
    find_package(Threads REQUIRED)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(PCAP REQUIRED libpcap)
    pkg_check_modules(OPENSSL REQUIRED openssl)
endif()

# SOEM (required for EtherCAT)
find_library(SOEM_LIBRARY NAMES soem PATHS
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/lib/windows
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/lib/linux
    NO_DEFAULT_PATH
)
find_path(SOEM_INCLUDE_DIR soem/soem.h PATHS
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/include
    NO_DEFAULT_PATH
)
if(NOT SOEM_LIBRARY)
    message(FATAL_ERROR "SOEM library not found. Check third_party/lib/")
endif()
if(NOT SOEM_INCLUDE_DIR)
    message(FATAL_ERROR "SOEM headers not found. Check third_party/include/")
endif()

# ZLG CAN (Windows only, required for CANFD)
if(WIN32)
    find_library(ZLG_LIBRARY NAMES zlgcan PATHS
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/lib/windows
        NO_DEFAULT_PATH
    )
    find_path(ZLG_INCLUDE_DIR zlgcan.h PATHS
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/include/zlgcan
        NO_DEFAULT_PATH
    )
    if(NOT ZLG_LIBRARY OR NOT ZLG_INCLUDE_DIR)
        message(FATAL_ERROR "ZLG CAN library not found. Check third_party/")
    endif()
endif()

# WinPcap / Packet (Windows only, required for EtherCAT)
if(WIN32)
    find_library(WPCAP_LIBRARY NAMES wpcap PATHS
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/lib/windows
        NO_DEFAULT_PATH
    )
    find_library(PACKET_LIBRARY NAMES Packet PATHS
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/lib/windows
        NO_DEFAULT_PATH
    )
    find_path(WPCAP_INCLUDE_DIR pcap.h PATHS
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/include/wpcap
        NO_DEFAULT_PATH
    )
    if(NOT WPCAP_LIBRARY OR NOT PACKET_LIBRARY)
        message(FATAL_ERROR "WinPcap/Packet library not found. Check third_party/")
    endif()
endif()

# libmodbus (optional, required only for RS485)
set(LIBMODBUS_FOUND FALSE)

# 1. Try pkg-config first (Linux/macOS)
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(LIBMODBUS QUIET libmodbus)
    if(LIBMODBUS_FOUND)
        message(STATUS "Found libmodbus via pkg-config: ${LIBMODBUS_LIBRARIES}")
    endif()
endif()

# 2. Fallback: find_library / find_path
if(NOT LIBMODBUS_FOUND)
    find_library(LIBMODBUS_LIBRARY NAMES modbus libmodbus)
    find_path(LIBMODBUS_INCLUDE_DIR modbus/modbus.h)
    if(LIBMODBUS_INCLUDE_DIR AND LIBMODBUS_LIBRARY)
        set(LIBMODBUS_FOUND TRUE)
        set(LIBMODBUS_INCLUDE_DIRS ${LIBMODBUS_INCLUDE_DIR})
        set(LIBMODBUS_LIBRARIES ${LIBMODBUS_LIBRARY})
        message(STATUS "Found libmodbus via find_library: ${LIBMODBUS_LIBRARY}")
    endif()
endif()

# 3. Windows bundled fallback
if(NOT LIBMODBUS_FOUND AND WIN32)
    set(LIBMODBUS_BUNDLED_INCLUDE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/include)
    set(LIBMODBUS_BUNDLED_LIB ${CMAKE_CURRENT_SOURCE_DIR}/third_party/lib/windows/libmodbus.lib)
    if(EXISTS ${LIBMODBUS_BUNDLED_LIB})
        set(LIBMODBUS_FOUND TRUE)
        set(LIBMODBUS_INCLUDE_DIRS ${LIBMODBUS_BUNDLED_INCLUDE})
        set(LIBMODBUS_LIBRARIES ${LIBMODBUS_BUNDLED_LIB})
        message(STATUS "Using bundled libmodbus: ${LIBMODBUS_BUNDLED_LIB}")
    endif()
endif()

if(NOT LIBMODBUS_FOUND)
    message(WARNING
        "libmodbus not found. RS485 support will be disabled.\n"
        "Install it:\n"
        "  Ubuntu/Debian: sudo apt install libmodbus-dev\n"
        "  Or build from source: https://github.com/stephane/libmodbus\n"
        "  Windows: place libmodbus.lib and headers in third_party/"
    )
    add_compile_definitions(GHAND_NO_LIBMODBUS)
else()
    message(STATUS "libmodbus enabled: ${LIBMODBUS_LIBRARIES}")
endif()

# Auto-generate device type mapping from zlgcan.h
set(ZLGCAN_H "${CMAKE_CURRENT_SOURCE_DIR}/third_party/include/zlgcan/zlgcan.h")
set(GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY ${GENERATED_DIR})

file(STRINGS ${ZLGCAN_H} ZLGCAN_LINES REGEX "^#define ZCAN_[A-Za-z0-9_]+[ \t]+[0-9]+")
set(DEVICE_TYPE_INC "")
foreach(line ${ZLGCAN_LINES})
    string(REGEX MATCH "ZCAN_[A-Za-z0-9_]+" MACRO_NAME ${line})
    if(MACRO_NAME)
        set(DEVICE_TYPE_INC "${DEVICE_TYPE_INC}{\"${MACRO_NAME}\", ${MACRO_NAME}},\n")
    endif()
endforeach()

set(GENERATED_INC "${GENERATED_DIR}/zlgcan_device_types.inc")
file(WRITE ${GENERATED_INC} "${DEVICE_TYPE_INC}")

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
    src/internal/dexhand.cc
    src/internal/dexhand_callback_manager.cc
    src/internal/file_lock.cc
    src/internal/product_config_loader.cc
)

# Create shared library
add_library(ghand SHARED ${SOURCES})
target_compile_definitions(ghand PRIVATE GHAND_BUILD)

target_include_directories(ghand
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/comm
        ${CMAKE_CURRENT_SOURCE_DIR}/src/internal
        ${GENERATED_DIR}
        $<$<BOOL:${LIBMODBUS_FOUND}>:${LIBMODBUS_INCLUDE_DIRS}>
        ${SOEM_INCLUDE_DIR}
        $<$<BOOL:${WIN32}>:${SOEM_INCLUDE_DIR}/soem/win32>
        $<$<BOOL:${UNIX}>:${SOEM_INCLUDE_DIR}/soem/linux>
        $<$<BOOL:${WIN32}>:${ZLG_INCLUDE_DIR}>
        $<$<BOOL:${WIN32}>:${WPCAP_INCLUDE_DIR}>
)

# Link dependencies
target_link_libraries(ghand PRIVATE
    ${SOEM_LIBRARY}
    $<$<BOOL:${WIN32}>:${ZLG_LIBRARY}>
    $<$<BOOL:${WIN32}>:${WPCAP_LIBRARY}>
    $<$<BOOL:${WIN32}>:${PACKET_LIBRARY}>
    $<$<BOOL:${LIBMODBUS_FOUND}>:${LIBMODBUS_LIBRARIES}>
    $<$<BOOL:${UNIX}>:Threads::Threads>
    $<$<BOOL:${UNIX}>:${PCAP_LIBRARIES}>
    $<$<BOOL:${UNIX}>:ssl>
    $<$<BOOL:${UNIX}>:crypto>
    $<$<BOOL:${UNIX}>:rt>
    $<$<BOOL:${UNIX}>:dl>
    $<$<BOOL:${WIN32}>:winmm>
    $<$<BOOL:${WIN32}>:ws2_32>
    $<$<BOOL:${WIN32}>:setupapi>
    $<$<BOOL:${WIN32}>:cfgmgr32>
)

# Set output name and version info
set_target_properties(ghand PROPERTIES
    OUTPUT_NAME ghand
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
)

# ========== Runtime DLLs ==========
set(RUNTIME_DLLS)

if(WIN32)
    # ZLG CAN driver
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/lib/windows/zlgcan.dll")
        list(APPEND RUNTIME_DLLS
            "${CMAKE_CURRENT_SOURCE_DIR}/third_party/lib/windows/zlgcan.dll"
        )
    endif()

    # ZLG kernel DLLs
    file(GLOB ZLG_KERNEL_DLLS
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/lib/windows/kerneldlls/*.dll"
    )
    if(ZLG_KERNEL_DLLS)
        list(APPEND RUNTIME_DLLS ${ZLG_KERNEL_DLLS})
    endif()

    # libmodbus DLL (if found alongside the import library)
    if(LIBMODBUS_FOUND)
        get_filename_component(LIBMODBUS_LIB_DIR "${LIBMODBUS_LIBRARIES}" DIRECTORY)
        file(GLOB LIBMODBUS_DLLS
            "${LIBMODBUS_LIB_DIR}/libmodbus.dll"
            "${LIBMODBUS_LIB_DIR}/modbus.dll"
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
        "${CMAKE_CURRENT_SOURCE_DIR}/config/xiaoyao_hand.json"
        "${CMAKE_BINARY_DIR}/config/"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/config/l1_hand.json"
        "${CMAKE_BINARY_DIR}/config/"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/examples/config"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/config/xiaoyao_hand.json"
        "${CMAKE_BINARY_DIR}/examples/config/"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/config/l1_hand.json"
        "${CMAKE_BINARY_DIR}/examples/config/"
    COMMENT "Copying config files to build/config/ and build/examples/config/"
)

# ========== Examples ==========
add_subdirectory(examples)

# ========== Install rules ==========
install(TARGETS ghand
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

# Install public headers
install(DIRECTORY include/ghand
    DESTINATION include/ghand
    FILES_MATCHING PATTERN "*.h"
)

# Install product config files
install(DIRECTORY config/
    DESTINATION share/ghand-sdk/config
    FILES_MATCHING PATTERN "*.json"
)

# Install documentation
install(FILES README.md CHANGELOG.md
    DESTINATION .
)
