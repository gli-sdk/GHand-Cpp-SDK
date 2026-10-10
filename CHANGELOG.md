# Changelog

All notable changes to the GHand CPP SDK will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Changed
- **BREAKING:** Aligned self-test diagnostics with the Python SDK: `SelfTestError` now contains `Motor` (0x01), `Sensor` (0x02), and `Joint` (0x04); `SelfTestErrorInfo` contains `motor`, `sensor`, and `joint` channel lists. Replaced the old category enums with `SensorError` and `JointError` bit masks.
- EtherCAT, CANFD, and RS485 query only A8/A2/A6 categories selected by A0. Query failures and timeouts throw `std::runtime_error` instead of returning empty or partial diagnostics.

## [2.2.2] - 2026-09-02

### Added
- Added `CompleteFirmwareResetHandshake()` to coordinate firmware reset handling after EtherCAT FOE transfer.
- Added `GetSelfTestStatus()` for self-test status retrieval across communication backends.

### Changed
- **BREAKING:** Changed `DeviceInfo::serial_number` from a numeric value to `std::string` so ASCII device serial numbers are preserved without lossy conversion.
- Updated serial number handling to use ASCII format across communication modules.
- Refined firmware reset handling with polling, timeout management, and clearer firmware update error states.
- Simplified firmware update reconnect logic by removing a redundant retry loop.

### Fixed
- Restored CANFD register write default timeout to 5000 ms after reverting the EtherCAT SDO write settle delay change.
- Reduced noisy firmware reset polling logs while waiting for reset readiness.

## [2.2.1] - 2026-08-24

### Licensing
- Changed the GHand C++ SDK distribution license to GPL-3.0-only to align with the GPLv3 licensing option used by the statically linked SOEM 2.0.0 stack.
- Added complete third-party notices and license texts.
- Corrected SOEM license metadata from GPL-2.0 to GPLv3 / Commercial dual license.
- Documented libmodbus 3.1.12 as LGPL-2.1-or-later and retained dynamic linking.
- Added WinPcap and nlohmann/json notices.

### Changed
- Switched Linux libmodbus handling from bundled `deps/lib/linux/libmodbus.so*` binaries to the system `libmodbus-dev` package discovered through pkg-config.
- Added `deps/SOEM_BUILD_INFO.md` to record SOEM v2.0.0 provenance and release rebuild requirements.
- Refreshed bundled SOEM headers and rebuilt the Windows `soem.lib` from official SOEM v2.0.0 commit `304d1c05eab77dc0d426f1a5cf09c8cc7dc03713`.
- Updated README dependency guidance for Linux system libmodbus and Windows binary distribution notes.
- Updated `.gitignore` so Linux libmodbus shared objects are not accidentally reintroduced while the intended bundled SOEM and Windows dependency binaries remain trackable.
- Added `RS485BaudRate` and `CANFDBitTiming` public enums and replaced raw baud-rate gear parameters in the public C++ API.

## [2.2.0] - 2026-08-17

### Added
- Added RS485/CANFD connection parameters for slave ID and baud-rate gear.
- Added RS485/CANFD baud-rate configuration through holding register `0x002C`.

### Changed
- CANFD ZQWL configuration now clears configuration ACK responses and waits after bitrate setup, matching the Python SDK transport behavior.

### Breaking Changes
- Removed `LOG_DEBUG`, `LOG_INFO`, `LOG_WARNING`, `LOG_ERROR` macros from public header `include/ghand/logging.h`. These were never documented as public API. If your code depended on them, define your own macros or call `ghand::Log()` directly.
- Renamed all example files to remove number prefixes for consistency with C++ naming conventions
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
