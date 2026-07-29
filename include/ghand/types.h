// Copyright 2025 Glitech.

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

// Product type definitions
enum class ProductType : uint8_t { G5, AUTO, L1 };

/**
 * @brief Get string representation of product type (for debug/logging)
 * @warning Do not call in real-time control loops (string processing overhead)
 */
GHAND_API const char* ToString(ProductType type);  // NOLINT(runtime/string)

// Communication type definitions
enum class CommType : uint8_t { ETHERCAT, CANFD, RS485 };

// Control mode definitions
enum class ControlMode : uint8_t { POSITION = 0, TORQUE = 1, SPEED = 2 };

// Firmware update error codes
enum class FirmwareUpdateError : int {
  SUCCESS = 0,
  PREPARE_COMMAND_FAILED,   // Write 0x5A pre-check failed
  ENTER_BOOT_MODE_FAILED,   // Enter BOOT state failed
  FOE_TRANSFER_FAILED,      // FOE transfer failed
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
  std::string device_name;
  std::string hardware_version;
  std::string software_version;       // Main controller firmware version
  std::string position_sensor_version;
  std::string tactile_sensor_version;
  std::string motor_driver_version;
  std::string firmware_package_version;
  std::string thumb_tactile_sensor_version;
  std::string finger_tactile_sensor_version;
  unsigned int serial_number;
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

}  // namespace ghand

#endif  // INCLUDE_GHAND_TYPES_H_
