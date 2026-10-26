// SPDX-FileCopyrightText: 2025-2026 GLITech
// SPDX-License-Identifier: GPL-3.0-only

#ifndef INCLUDE_GHAND_TYPES_H_
#define INCLUDE_GHAND_TYPES_H_

#include <cstdint>
#include <array>
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

enum class State : uint16_t {
  DISABLED = 0,
  ENABLED = 1,
  UNKNOWN_STATE = 255,
  STOPPED = DISABLED,  // Source-compatible names for the hand enable state.
  RUNNING = ENABLED
};

/**
 * @brief Get string representation of state (for debug/logging)
 * @warning Do not call in real-time control loops (string processing overhead)
 */
GHAND_API const char* ToString(State state);  // NOLINT(runtime/string)

// Legacy compatibility codes. V2.4 feedback uses JointErrorCode and HandState bitmaps.
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

enum class JointStateCode : uint8_t {
  DISABLED = 0,
  RUNNING = 1,
  STOPPED = 2,
  FAULT = 3,
  UNKNOWN_STATE = 255
};

GHAND_API const char* ToString(JointStateCode state);  // NOLINT(runtime/string)

enum class JointErrorCode : uint8_t {
  NORMAL = 0,
  MOTOR_HARDWARE_OVERCURRENT = 1,
  MOTOR_CONTINUOUS_CURRENT_HIGH = 2,
  MOTOR_BUS_OVERCURRENT = 3,
  MOTOR_HARDWARE_OVERVOLTAGE = 4,
  MOTOR_HARDWARE_UNDERVOLTAGE = 5,
  MOTOR_PHASE_LOSS = 6,
  MOTOR_STALLED = 7,
  MOTOR_CHIP_OVERTEMPERATURE = 8,
  COMMUNICATION_LOST = 9,
  MOTOR_SOFTWARE_OVERVOLTAGE = 10,
  MOTOR_SOFTWARE_UNDERVOLTAGE = 11,
  MOTOR_SOFTWARE_OVERTEMPERATURE = 12,
  MOTOR_WINDING_OVERTEMPERATURE = 13,
  MOTOR_OVERTEMPERATURE = MOTOR_SOFTWARE_OVERTEMPERATURE,  // Compatibility alias.
  UNKNOWN_ERROR = 255
};

GHAND_API const char* ToString(JointErrorCode error);  // NOLINT(runtime/string)

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
  // Arbitration: 500 Kbps @ 80%; data: 800 Kbps @ 80%.
  TIMING_500K_800K = 0x00,
  // Arbitration: 500 Kbps @ 80%; data: 1 Mbps @ 75%.
  TIMING_500K_1M = 0x01,
  // Arbitration: 500 Kbps @ 80%; data: 2 Mbps @ 80%.
  TIMING_500K_2M = 0x02,
  // Arbitration: 500 Kbps @ 80%; data: 4 Mbps @ 80%.
  TIMING_500K_4M = 0x03,
  // Arbitration: 1 Mbps @ 75%; data: 1 Mbps @ 75%.
  TIMING_1M_1M = 0x04,
  // Arbitration: 1 Mbps @ 75%; data: 2 Mbps @ 80%.
  TIMING_1M_2M = 0x05,
  // Arbitration: 1 Mbps @ 75%; data: 4 Mbps @ 80%.
  TIMING_1M_4M = 0x06,
  // Arbitration: 1 Mbps @ 75%; data: 5 Mbps @ 75%.
  TIMING_1M_5M = 0x07,
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
  INTERNAL_ERROR,
  PRODUCT_MISMATCH
};

GHAND_API const char* ToString(SdkError error);  // NOLINT(runtime/string)

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

struct Diagnostics {
  ConnectionDiagnostics connection;
  IdentityDiagnostics identity;
  TransportDiagnostics transport;
  SubscriptionDiagnostics subscription;
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
enum class ControlMode : uint8_t {
  POSITION = 0,
  SPEED = 1,
  CURRENT = 2,
  MIT = 3,
  CUSTOM = 0x0F,
  INHERIT = 0xFF  // JointCommand only: inherit the hand or previous joint mode.
};

enum class HandStateMachine : uint8_t {
  INITIALIZED = 0, POWER_ON_SELF_TEST = 1, IDLE = 2, MOVING = 3,
  FAULT = 4, LOW_POWER = 5, FIRMWARE_UPGRADE = 6, UNKNOWN_STATE = 255
};

enum class JointControlState : uint8_t { DISABLED = 0, RUNNING = 1, STOPPED = 2 };

// Firmware update error codes
enum class FirmwareUpdateError : int {
  SUCCESS = 0,
  PREPARE_COMMAND_FAILED,   // Write 0x5A pre-check failed
  ENTER_BOOT_MODE_FAILED,   // Enter BOOT state failed
  FOE_TRANSFER_FAILED,      // FOE transfer failed
  SWITCH_COMM_STATE_FAILED,  // Communication state switch failed
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
  State state = State::STOPPED;
  std::array<uint8_t, 6> error_warn{};
  std::array<uint8_t, 12> error_fault{};
  std::array<uint8_t, 6> error_critical{};
  int16_t temperature = 0;  // Back-of-hand temperature in degrees Celsius.
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
  uint8_t sensor_state = 0;  // Raw byte reserved (for debugging)
  uint8_t sensor_error = 0;  // Global error code

  std::vector<RegionTactile> regions;
};

// Joint command structure for parameterized joint control
struct JointCommand {
  JointCommand() = default;
  JointCommand(JointId joint_id, float target_angle, int16_t target_velocity,
               int16_t target_current)
      : id(joint_id), angle(target_angle), velocity(target_velocity), current(target_current) {}
  JointId id = JointId::THUMB_IP;  // Joint identifier
  float angle = 0.0f;              // Target angle (deg)
  int16_t velocity = 0;             // Target velocity (-100~100%, depending on control mode)
  int16_t current = 0;              // Target current (-100~100%, depending on control mode)
  ControlMode mode = ControlMode::INHERIT;
  uint8_t stop = 1;
  int16_t kp = 0;                  // MIT proportional gain
  int16_t kd = 0;                  // MIT derivative gain
};

struct Joint {
  JointId id;
  JointStateCode state;
  JointErrorCode error;
  uint8_t mode = 0;
  uint8_t stop = 0;  // Legacy feedback. Reserved and always 0 for V2.4 GHand5.
  float angle;
  int16_t velocity;
  int16_t current = 0;
};

// HandState / Joint query functions

inline bool IsNormal(const HandState& hs) {
  for (uint8_t code : hs.error_warn) if (code) return false;
  for (uint8_t code : hs.error_fault) if (code) return false;
  for (uint8_t code : hs.error_critical) if (code) return false;
  return hs.state == State::DISABLED || hs.state == State::ENABLED;
}

inline bool HasError(const HandState& hs) {
  return !IsNormal(hs) ||
         (hs.state != State::ENABLED && hs.state != State::DISABLED);
}

inline bool IsNormal(const Joint& j) {
  return (j.state == JointStateCode::DISABLED ||
          j.state == JointStateCode::RUNNING ||
          j.state == JointStateCode::STOPPED) &&
         j.error == JointErrorCode::NORMAL;
}

inline bool HasError(const Joint& j) {
  return j.error != JointErrorCode::NORMAL ||
         (j.state != JointStateCode::RUNNING &&
          j.state != JointStateCode::STOPPED &&
          j.state != JointStateCode::DISABLED);
}

GHAND_API std::string ToString(const HandState& hs);
GHAND_API std::string ToString(const Joint& joint);

}  // namespace ghand

#endif  // INCLUDE_GHAND_TYPES_H_
