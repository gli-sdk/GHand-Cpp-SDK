// SPDX-FileCopyrightText: 2025-2026 GLITech
// SPDX-License-Identifier: GPL-3.0-only

#ifndef INCLUDE_GHAND_TYPES_H_
#define INCLUDE_GHAND_TYPES_H_

#include <cstdint>
#include <string>
#include <vector>

#include "ghand/export.h"

namespace ghand {

// Hand type definitions
enum class HandType : uint8_t {
  NONE,
  LEFT,
  RIGHT,
  NUM_HANDS  // For counting only, not a valid value
};

/**
 * @brief Get string representation of hand type (for debug/logging)
 * @warning Do not call in real-time control loops (string processing overhead)
 */
GHAND_API const char* ToString(HandType type);  // NOLINT(runtime/string)

// Finger type definitions
enum class FingerType : uint8_t {
  THUMB,
  FF,
  MF,
  RF,
  LF,
  NUM_FINGERS  // For counting only, not a valid value
};

/**
 * @brief Get string representation of finger type (for debug/logging)
 * @warning Do not call in real-time control loops (string processing overhead)
 */
GHAND_API const char* ToString(FingerType finger);  // NOLINT(runtime/string)

// Force data structure
struct Force {
  float x;
  float y;
  float z;
  Force() : x(0.0f), y(0.0f), z(0.0f) {}
  Force(int16_t x_val, int16_t y_val, uint16_t z_val)
      : x(x_val), y(y_val), z(z_val) {}
};

enum class State : uint8_t {
  STOPPED = 0,            // Stopped
  RUNNING = 1,            // Normal operation
  ABNORMAL_RUNNING = 2,   // Abnormal operation
  PROTECTIVE_STOPPED = 3  // Protective stop
};

/**
 * @brief Get string representation of state (for debug/logging)
 * @warning Do not call in real-time control loops (string processing overhead)
 */
GHAND_API const char* ToString(State state);  // NOLINT(runtime/string)

enum class ErrorCode : uint8_t {
  NORMAL = 0,
  // Motor errors
  MOTOR_HARDWARE_OVERCURRENT = 1,  // Motor hardware overcurrent
  MOTOR_SOFTWARE_OVERCURRENT = 2,  // Motor software overcurrent
  MOTOR_BUS_OVERCURRENT = 3,       // Motor bus overcurrent
  MOTOR_PHASE_LOST = 4,            // Motor phase loss
  MOTOR_STALLED = 5,               // Motor stall
  MOTOR_DRIVER_OVERTEMP = 6,       // Motor driver overtemperature
  MOTOR_COMM_ERROR = 7,            // Motor communication error
  MOTOR_OVER_TEMP = 8,             // Motor overtemperature
  // Finger errors
  JOINT_CONFLICT = 11,       // Joint position conflict
  TIP_CONFLICT = 12,         // Fingertip pose conflict
  JOINT_POSITION_ERROR = 13,  // Joint position error
  // Hand errors
  LOW_TEMP = 21,      // Temperature too low
  HIGH_TEMP = 22,     // Temperature too high
  LOW_VOLTAGE = 23,   // Voltage too low
  HIGH_VOLTAGE = 24,  // Voltage too high
  // Tactile sensor errors
  TACTILE_DISCONNECTED = 31,  // Tactile sensor disconnected
  TACTILE_DATA_ERROR = 32,    // Tactile sensor data error
  SELF_CHECK_ERROR = 41,  // Self-check error
  // Data processing errors
  PARAM_ERROR = 101,  // Parameter error
  // Other
  UNKNOWN_ERROR = 201  // Unknown error
};

/**
 * @brief Get string representation of error code (for debug/logging)
 * @warning Do not call in real-time control loops (string processing overhead)
 */
GHAND_API const char* ToString(ErrorCode error);  // NOLINT(runtime/string)

// Communication type definitions
enum class CommType : uint8_t { ETHERCAT, CANFD, RS485 };

GHAND_API const char* ToString(CommType type);  // NOLINT(runtime/string)

/**
 * @brief Predefined RS485 baud rates.
 *
 * Each value maps directly to the baud-rate gear defined by the GHand device
 * protocol.
 */
enum class RS485BaudRate : uint8_t {
  RATE_57600 = 0x00,
  RATE_115200 = 0x01,
  RATE_230400 = 0x02,
  RATE_460800 = 0x03,
  RATE_921600 = 0x04,
  RATE_1000000 = 0x05,
};

/**
 * @brief Predefined CAN FD bit timing profiles.
 *
 * Naming format: TIMING_<arbitration bitrate>_<data bitrate>.
 * Sample points are fixed by the GHand device protocol for each profile.
 */
enum class CANFDBitTiming : uint8_t {
  // Arbitration: 500 Kbps @ 80%; data: 1 Mbps @ 75%.
  TIMING_500K_1M = 0x00,
  // Arbitration: 500 Kbps @ 80%; data: 2 Mbps @ 80%.
  TIMING_500K_2M = 0x01,
  // Arbitration: 500 Kbps @ 80%; data: 4 Mbps @ 80%.
  TIMING_500K_4M = 0x02,
  // Arbitration: 500 Kbps @ 80%; data: 5 Mbps @ 75%.
  TIMING_500K_5M = 0x03,
  // Arbitration: 1 Mbps @ 75%; data: 4 Mbps @ 80%.
  TIMING_1M_4M = 0x04,
  // Arbitration: 1 Mbps @ 75%; data: 5 Mbps @ 75%.
  TIMING_1M_5M = 0x05,
};

// Product type definitions
enum class ProductType : uint8_t {AUTO, GHand5,  GHandLite1 };

/**
 * @brief Get string representation of product type (for debug/logging)
 * @warning Do not call in real-time control loops (string processing overhead)
 */
GHAND_API const char* ToString(ProductType type);  // NOLINT(runtime/string)

enum class SdkError : uint8_t {
  OK = 0,
  NOT_CONNECTED,
  INVALID_ARGUMENT,
  NOT_SUPPORTED,
  TIMEOUT,
  TRANSPORT_ERROR,
  PROTOCOL_ERROR,
  CONFIG_ERROR,
  DEVICE_REJECTED,
  INTERNAL_ERROR
};

GHAND_API const char* ToString(SdkError error);  // NOLINT(runtime/string)

struct OperationResult {
  SdkError code;
  std::string message;
  CommType comm;

  OperationResult()
      : code(SdkError::OK), message(), comm(CommType::ETHERCAT) {}
  OperationResult(SdkError error_code, const std::string& error_message,
                  CommType comm_type)
      : code(error_code), message(error_message), comm(comm_type) {}
};

struct ConnectionDiagnostics {
  bool connected;
  CommType comm_type;
  std::string device;

  ConnectionDiagnostics()
      : connected(false), comm_type(CommType::ETHERCAT), device() {}
};

struct IdentityDiagnostics {
  ProductType product;
  HandType hand_type;
  std::string firmware_version;

  IdentityDiagnostics()
      : product(ProductType::AUTO), hand_type(HandType::NONE),
        firmware_version() {}
};

struct TransportDiagnostics {
  uint64_t tx_packets;
  uint64_t rx_packets;
  uint64_t tx_errors;
  uint64_t rx_errors;
  uint64_t timeouts;
  uint64_t retries;
  uint64_t reconnects;

  TransportDiagnostics()
      : tx_packets(0),
        rx_packets(0),
        tx_errors(0),
        rx_errors(0),
        timeouts(0),
        retries(0),
        reconnects(0) {}
};

struct SubscriptionDiagnostics {
  uint64_t active_subscriptions;
  uint64_t received_frames;
  uint64_t dispatched_frames;
  uint64_t dropped_frames;
  uint64_t callback_errors;

  SubscriptionDiagnostics()
      : active_subscriptions(0),
        received_frames(0),
        dispatched_frames(0),
        dropped_frames(0),
        callback_errors(0) {}
};

struct ErrorDiagnostics {
  SdkError last_sdk_error;
  std::string last_error_message;

  ErrorDiagnostics()
      : last_sdk_error(SdkError::OK), last_error_message() {}
};

struct Diagnostics {
  ConnectionDiagnostics connection;
  IdentityDiagnostics identity;
  TransportDiagnostics transport;
  SubscriptionDiagnostics subscription;
  ErrorDiagnostics error;
};

enum class JointId : uint8_t {
  THUMB_IP,
  THUMB_MCP,
  THUMB_TMC_FE,
  THUMB_TMC_AA,
  THUMB_TMC_PS,
  FF_DIP,
  FF_PIP,
  FF_MCP,
  FF_MCP_AA,
  MF_DIP,
  MF_PIP,
  MF_MCP,
  RF_DIP,
  RF_PIP,
  RF_MCP,
  LF_DIP,
  LF_PIP,
  LF_MCP,
  NUM_JOINTS
};

/**
 * @brief Get string representation of joint ID (for debug/logging)
 * @warning Do not call in real-time control loops (string processing overhead)
 */
GHAND_API const char* ToString(JointId id);  // NOLINT(runtime/string)

// Control mode definitions
enum class ControlMode : uint8_t { POSITION = 0, TORQUE = 1, SPEED = 2 };

// Firmware update error codes
enum class FirmwareUpdateError : int {
  SUCCESS = 0,
  PREPARE_COMMAND_FAILED,   // Write 0x5A pre-check failed
  ENTER_BOOT_MODE_FAILED,   // Enter BOOT state failed
  FOE_TRANSFER_FAILED,      // FOE transfer failed
  SWITCH_COMM_STATE_FAILED,  // Communication state switch failed
  WRITE_RESET_CHECK_FAILED,  // Write reset check command failed
  NOT_RESETTABLE,            // Device reset is unavailable
  FIRMWARE_RESET_FAILED,     // Firmware reset command failed
  MAIN_CONTROLLER_FAILED,   // Main controller MCU upgrade failed
  POSITION_SENSOR_FAILED,   // Position sensor MCU upgrade failed
  TACTILE_SENSOR_FAILED,    // Tactile sensor MCU upgrade failed
  MOTOR_DRIVER_FAILED,      // Motor driver MCU upgrade failed
  RECONNECT_FAILED,  // Reconnect after update failed
  // Communication protocol does not support firmware update
  NOT_SUPPORTED,
  QUERY_FAILED,  // Failed to query upgrade results
};

// Device info structure
struct DeviceInfo {
  std::string device_name = "N/A";
  std::string hardware_version = "N/A";
  std::string software_version = "N/A";       // Main controller firmware version
  std::string position_sensor_version = "N/A";
  std::string tactile_sensor_version = "N/A";
  std::string motor_driver_version = "N/A";
  std::string firmware_package_version = "N/A";
  std::string thumb_tactile_sensor_version = "N/A";
  std::string finger_tactile_sensor_version = "N/A";
  std::string serial_number = "N/A";
};

/**
 * @brief Hand state data structure
 */
struct HandState {
  State state;
  ErrorCode error;
  int16_t temperature;
};

/**
 * @brief Sensor data for a single tactile region
 */
struct RegionTactile {
  const char* region_name;  // Region name (provided by device)
  // Sensor state (true=normal, false=abnormal)
  bool state;
  Force resultant_force;                  // Resultant force data
  std::vector<Force> distributed_forces;  // Distributed force data
};

/**
 * @brief Tactile data structure
 *
 * Design notes:
 * - regions are ordered according to device protocol definition, consistent
 *   with protocol frame byte order
 * - region_name points to string returned by device, lifetime same as
 *   TactileData frame
 * - sensor_state is bit-encoded: bits 0-4 correspond to regions 0-4
 * - sensor_error is global error code
 */
struct TactileData {
  uint8_t sensor_state;  // Raw byte reserved (for debugging)
  uint8_t sensor_error;  // Global error code

  std::vector<RegionTactile> regions;
};

// Joint command structure for parameterized joint control
struct JointCommand {
  JointId id;       // Joint identifier
  float angle;      // Target angle (deg)
  int8_t velocity;  // Target velocity (-100~100%, depending on control mode)
  int8_t torque;    // Target torque (-100~100%, depending on control mode)
};

struct Joint {
  JointId id;
  State state;
  ErrorCode error;
  float angle;
  int8_t velocity;
  int8_t torque;
};

// HandState / Joint query functions

inline bool IsNormal(const HandState& hs) {
  return (hs.state == State::STOPPED || hs.state == State::RUNNING) &&
         hs.error == ErrorCode::NORMAL;
}

inline bool HasError(const HandState& hs) {
  return hs.error != ErrorCode::NORMAL ||
         (hs.state != State::RUNNING && hs.state != State::STOPPED);
}

inline bool IsNormal(const Joint& j) {
  return (j.state == State::STOPPED || j.state == State::RUNNING) &&
         j.error == ErrorCode::NORMAL;
}

inline bool HasError(const Joint& j) {
  return j.error != ErrorCode::NORMAL ||
         (j.state != State::RUNNING && j.state != State::STOPPED);
}

GHAND_API std::string ToString(const HandState& hs);
GHAND_API std::string ToString(const Joint& joint);

// ============================================================================
// Self-test error query (object dictionary 0x2008)
// ============================================================================

/**
 * @brief A0 self-test summary bit mask. Multiple bits may be set simultaneously.
 */
enum class SelfTestError : uint8_t {
  None              = 0x00,
  Motor             = 0x01,
  Zeroing           = 0x04,
  Fan               = 0x08,
  TemperatureSensor = 0x10,
  TactileSensor     = 0x20,
  PositionSensor    = 0x40,
  Version           = 0x80,
};

inline uint8_t operator&(uint8_t lhs, SelfTestError rhs) {
  return lhs & static_cast<uint8_t>(rhs);
}

inline bool HasSelfTestError(uint8_t summary, SelfTestError bit) {
  return (summary & static_cast<uint8_t>(bit)) != 0;
}

/**
 * @brief A1 version-mismatch bit mask.
 */
enum class VersionCheckError : uint8_t {
  None           = 0x00,
  MotorDriver    = 0x20,
  TactileSensor  = 0x40,
  PositionSensor = 0x80,
};

/**
 * @brief A3 tactile-sensor error bit mask (per motor channel).
 */
enum class TactileCheckError : uint8_t {
  None                    = 0x00,
  ThumbTipDisconnected    = 0x01,
  FfTipDisconnected       = 0x02,
  MfTipDisconnected       = 0x04,
  RfTipDisconnected       = 0x08,
  LfTipDisconnected       = 0x10,
  CommunicationFailed     = 0x80,
};

/**
 * @brief A6 zeroing error (single-value enum, per motor channel).
 */
enum class ZeroingCheckError : uint8_t {
  None                   = 0x00,
  MotorAbnormal          = 0x01,
  JointStall             = 0x02,
  BrakeTimeout           = 0x03,
  GlobalTimeout          = 0x04,
  CommunicationFault     = 0x05,
};

/**
 * @brief A8 motor-check error (per motor channel).
 */
enum class MotorCheckError : uint8_t {
  None                    = 0x00,
  HardwareOverCurrent     = 0x01,
  SoftwareOverCurrent     = 0x02,
  BusOverCurrent          = 0x03,
  HardwareOverVoltage     = 0x04,
  HardwareUnderVoltage    = 0x05,
  PhaseLoss               = 0x06,
  Stall                   = 0x07,
  HardwareOverTemperature = 0x08,
  CommunicationLost       = 0x09,
  SoftwareOverVoltage     = 0x0A,
  SoftwareUnderVoltage    = 0x0B,
  SoftwareOverTemperature = 0x0C,
  PositionSensorError     = 0x0D,
};

/**
 * @brief Diagnostic error entry for a single 13-channel motor slot.
 *
 * ``motor_index`` is the raw index (0..12) into the protocol error array.
 * ``joint_id`` is the resolved JointId when the product configuration owns a
 * joint at that slot, or ``JointId::NUM_JOINTS`` if the slot is not mapped.
 * ``error_code`` is the category-specific raw error value.
 */
struct MotorDiagnosticError {
  uint8_t motor_index;
  JointId joint_id;
  uint8_t error_code;
};

/**
 * @brief Structured result of a self-test error query.
 *
 * Only categories actually flagged by ``summary`` will have their fields
 * populated; the rest remain at their default values.
 */
struct SelfTestErrorInfo {
  uint8_t summary = 0;              // Raw A0 bit mask
  uint8_t version = 0;              // Raw A1 bit mask
  std::vector<MotorDiagnosticError> position_sensor;
  std::vector<MotorDiagnosticError> tactile_sensor;
  uint8_t temperature = 0;          // Raw A4 value
  uint8_t fan = 0;                  // Raw A5 value
  std::vector<MotorDiagnosticError> zeroing;
  std::vector<MotorDiagnosticError> motor;
};

}  // namespace ghand

#endif  // INCLUDE_GHAND_TYPES_H_
