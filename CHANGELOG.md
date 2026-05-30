# Changelog

All notable changes to the Xiaoyao SDK C++ will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Changed
- **BREAKING:** Removed `LOG_DEBUG`, `LOG_INFO`, `LOG_WARNING`, `LOG_ERROR` macros from public header `include/ghand/logging.h`. These were never documented as public API. If your code depended on them, define your own macros or call `ghand::Log()` directly.
- **BREAKING:** Renamed all example files to remove number prefixes for consistency with C++ naming conventions
  - `01.basic_connection.cc` → `basic_connection.cc`
  - `02.move_joints.cc` → `move_joints.cc`
  - `03.tactile_callback.cc` → `tactile_callback.cc`
  - `04.preset_gesture.cc` → `preset_gesture.cc`
  - `05.multi_hand.cc` → `multi_hand.cc`
  - `06.interactive_joint_control.cc` → `interactive_joint_control.cc`
  - `07.glove_control.cc` → `glove_control.cc`
  - Corresponding CMake targets and executable names have also been updated
  - If you have scripts or documentation referencing old file names, please update them

### Fixed
- Updated all documentation references to use new example file names
- Refactored `CMakeLists.txt` to use `target_include_directories()` with proper `PUBLIC`/`PRIVATE` scoping instead of global `include_directories()`
- Changed `CANFDComm` and `EtherCATComm` to hold `ProductConfig` by value instead of by reference, eliminating potential lifetime coupling issues
- Sanitized public header comments in `include/ghand/types.h` to remove internal implementation references (`ProductConfig`)
