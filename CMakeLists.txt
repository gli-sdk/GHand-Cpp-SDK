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

if(UNIX AND NOT APPLE)
    find_package(Threads REQUIRED)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(PCAP REQUIRED libpcap)
    pkg_check_modules(OPENSSL REQUIRED openssl)
endif()

# Auto-generate device type mapping from zlgcan.h to avoid manual macro mapping maintenance
set(ZLGCAN_H "${CMAKE_CURRENT_SOURCE_DIR}/third_party/include/zlgcan/zlgcan.h")
set(GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY ${GENERATED_DIR})

file(STRINGS ${ZLGCAN_H} ZLGCAN_LINES REGEX "^#define ZCAN_[A-Za-z0-9_]+[ \t]+[0-9]+")
set(DEVICE_TYPE_INC "")
foreach(line ${ZLGCAN_LINES})
    string(REGEX MATCH "ZCAN_[A-Za-z0-9_]+" MACRO_NAME ${line})
    if(MACRO_NAME)
        set(DEVICE_TYPE_INC "${DEVICE_TYPE_INC}GHAND_ZLG_MAP_ENTRY(${MACRO_NAME})\n")
    endif()
endforeach()

set(GENERATED_INC "${GENERATED_DIR}/zlgcan_device_types.inc")
file(WRITE ${GENERATED_INC} "${DEVICE_TYPE_INC}")
include_directories(${GENERATED_DIR})

# Source file list (explicitly listed to avoid GLOB_RECURSE desync on add/remove)
set(SOURCES
    src/ghand.cc
    src/logging.cc
    src/types.cc
    src/comm/canfd_comm.cc
    src/comm/canfd_driver_zlg.cc
    src/comm/canfd_protocol.cc
    src/comm/ethercat_comm.cc
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
)

if(WIN32)
    target_include_directories(ghand PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/include
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/include/wpcap
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/include/zlgcan
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/include/soem/win32
    )
    target_link_directories(ghand PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/lib
    )
else()
    target_include_directories(ghand PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/include
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/include/soem/linux
    )
endif()

# Link dependencies
if(WIN32)
    target_link_libraries(ghand PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/lib/windows/soem.lib
        winmm ws2_32
        setupapi cfgmgr32
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/lib/windows/Packet.lib
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/lib/windows/wpcap.lib
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/lib/windows/zlgcan.lib
    )
else()
    # Linux: use precompiled libraries
    target_link_libraries(ghand PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/lib/linux/libsoem.a
        Threads::Threads
        ${PCAP_LIBRARIES}
        ssl crypto rt dl
    )
endif()

# Set output name and version info
set_target_properties(ghand PROPERTIES
    OUTPUT_NAME ghand
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
)

# ========== Auto-copy runtime DLLs and config files to output directory ==========
if(WIN32)
    file(GLOB RUNTIME_DLLS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/lib/windows/*.dll")
    if(RUNTIME_DLLS)
        add_custom_command(TARGET ghand POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${RUNTIME_DLLS}
            $<TARGET_FILE_DIR:ghand>
            COMMENT "Copying runtime DLLs to output directory"
        )
    endif()
endif()

# Copy config files to output directory (auto-sync from source config/ during development)
# GetSdkRootFromModule() derives SDK root from DLL path (parent of DLL directory)
# Therefore config must be placed under the parent of DLL directory:
#   Main DLL: build/<config>/ghand.dll  → SDK root = build/  → config at build/config/
#   Example DLL: build/examples/<config>/   → SDK root = build/examples/ → config at build/examples/config/
add_custom_command(TARGET ghand POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/config"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/config/xiaoyao_hand.json"
        "${CMAKE_BINARY_DIR}/config/"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/examples/config"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/config/xiaoyao_hand.json"
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

