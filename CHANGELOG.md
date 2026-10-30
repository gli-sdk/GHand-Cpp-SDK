# Changelog

All notable changes to the GHand CPP SDK will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [3.1.0] - 2026-10-30

### Added
- Bundled the Release-only Qt 6.9.1 Windows x64/MSVC 2022 runtime subset under `deps/qt/`: Core/Gui/Widgets DLLs, window/offscreen/style plugins and upstream license texts. Building the GUI examples uses a local Qt installation; CMake deploys the found runtime DLLs and plugins automatically.
- Added MIT and CUSTOM tutorials (`13_mit_control.cc`, `14_custom_control.cc`) covering compliant grasping, tactile contact detection, per-finger mode consistency, feedback and explicit drive disabling.
- Added the optional Qt 6 tactile display and tutorial (`15_tactile_display.cc`), with Python-aligned hand images, left/right hand detection, five finger maps, force markers, Fz bars, point tooltips, tactile switching and zeroing.
- Added 60-second timestamped XYZ force history with 50 Hz callback coalescing, missing-data gaps, stale-subscription rejection and embedded Qt image resources. GUI targets use C++17; the core SDK remains C++11.
- Added hardware-free tactile acquisition, resource and window lifecycle checks, plus `--preview` for viewing the interface without connecting hardware.
- Added a numbered tutorial sequence covering position, velocity and current control, tactile callbacks, multi-hand and glove control, interactive joint commands, hand state feedback, CANFD bit-timing configuration, and shared-bus connection examples.
- Integrated the new protocol with the V2.2.3 `Master` / `GHand` ownership model for same-product hands sharing an EtherCAT, CANFD, or RS485 bus.
- Added per-hand new-protocol controls, mode state, tactile state, callbacks and feedback caches; EtherCAT routes commands and feedback by physical hand slave index, including hands behind a junction.
- Added hardware-free shared-bus regression coverage for independent commands and feedback, ID changes, offline-hand isolation, slow-callback keepalive, and reconnect invalidation.

### Changed
- Standardized tutorial filenames as `01_basic_connection.cc` through `11_connect_hands.cc`; executable targets keep unnumbered names. Renamed the clap demo to `clap_pose`.
- Clarified `JointCommand` units, defaults and mode-dependent fields in `02_move_joints.cc`, with explicit position mode and command failure messages.
- **BREAKING ABI (relative to 2.x):** Set the SDK version to 3.1.0 and SOVERSION to 3. Applications must rebuild against matching headers and libraries. Existing library base names, CMake package/target names and `SameMajorVersion` package compatibility are retained.
- Restored V2.2.3 firmware-result polling (500 ms interval, 10 s per query) and command acknowledgment checks. Firmware updates remain limited to one EtherCAT hand at physical slave 1.
- **BREAKING (relative to 2.2.2 and earlier):** Create a `Master` and obtain non-owning `GHand*` objects from `Connect()` or `hand()`. Results use `Result` / `ResultOf`; the single-connection `GHand::Create`, `SetControlMode` and `GetLastResult` API is replaced. Reconnect, disconnect and Master destruction invalidate old hand pointers.
- Bus-wide commands address each discovered hand individually and attempt all hands even if one fails. New-protocol register values, PDO layouts, units, mailbox handling and control modes are preserved.
- Mixed product profiles on one Master and firmware updates on multi-hand/junction EtherCAT buses are rejected rather than targeting an unintended hand.
- **BREAKING:** Under the new protocol, `SetSlaveId()` writes holding register `0x0003` and `SetBaudrateConfig()` writes holding register `0x0004` for both RS485 and CANFD (2.2.0 used `0x002C` for baud-rate configuration).

### Fixed
- Preserve signed velocity/current targets in MIT mode and clamp MIT gains to `kp=10..2000`, `kd=0..50`, `kp>kd`, matching Python SDK semantics.
- Read thumb and finger tactile firmware versions over RS485 and CANFD.
- Use EtherCAT subindex 0 for serial-number and runtime-state reads, and issue the firmware read command before fetching stall tolerances and PID parameters.
- Parse the joint stop field from Modbus feedback so `Joint::stop` reflects the received state.

### Removed
- Removed legacy self-test queries and their dedicated diagnostic types; motor feedback and hand status now provide diagnostics.

## [2.2.3]

Release date is not recorded in the source branch; this entry summarizes the `V2.2.3` branch through commit `6c84eed`.

### Added
- Added shared-bus multi-hand support using `Master` ownership and per-hand `GHand` objects, including EtherCAT topology discovery, RS485 multi-hand connections, and per-slave callbacks.
- Added EtherCAT dual-hand and automatic topology detection examples.
- Added `SdkError::PRODUCT_MISMATCH` to distinguish unexpected products from transport failures.
- Added `SensorError` (A2 bit mask) and `JointError` (A6 bit mask), matching Python SDK `pan_fix_sn`.

### Changed
- **BREAKING:** Aligned legacy self-test diagnostics with Python SDK `pan_fix_sn`: A0 flags are `Motor=0x01`, `Sensor=0x02`, and `Joint=0x04`; detail queries use A8, A2, and A6. `SelfTestErrorInfo` contains `summary`, `sensor`, `joint`, and `motor`. Removed legacy summary flags, `VersionCheckError`, `TactileCheckError`, `ZeroingCheckError`, and their result fields. Applications must rebuild and use firmware implementing the matching diagnostic protocol. These legacy APIs are subsequently removed in 3.1.0.
- EtherCAT, CANFD, and RS485 self-test query failures and timeouts throw `std::runtime_error` instead of returning empty or partial diagnostics.

### Fixed
- Disconnect EtherCAT after failed BOOT entry or communication-state transition during firmware update, preserving the original error.
- Preserve device information after a product mismatch and clear it before each new connection attempt.
- Continue adapter auto-search after product mismatch; if no adapter connects, preserve the first mismatch and its device information.
- Corrected maximum ranges for FF_MCP, MF_MCP, RF_MCP, and LF_MCP joints.

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
