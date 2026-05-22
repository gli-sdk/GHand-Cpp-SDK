#ifndef GHAND_TYPES_H_
#define GHAND_TYPES_H_

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "export.h"

namespace ghand {

// ===== Hand Type Definitions =====
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
std::string GHAND_API ToString(HandType type);

// ===== Finger Type Definitions =====
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
std::string GHAND_API ToString(FingerType finger);

// ===== Force Data Structure =====
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
std::string GHAND_API ToString(State state);

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
  // Finger errors
  JOINT_CONFLICT = 11,  // Joint conflict
  TIP_CONFLICT = 12,    // Tip conflict
  // Hand errors
  LOW_TEMP = 21,      // Temperature too low
  HIGH_TEMP = 22,     // Temperature too high
  LOW_VOLTAGE = 23,   // Voltage too low
  HIGH_VOLTAGE = 24,  // Voltage too high
  // Tactile sensor errors
  TACTILE_ERROR = 31,  // Tactile sensor error
  // Data processing errors
  PARAM_ERROR = 101,  // Parameter error
  TIMEOUT = 102,      // Timeout
  // Other
  UNKNOWN_ERROR = 201  // Unknown error
};

/**
 * @brief Get string representation of error code (for debug/logging)
 * @warning Do not call in real-time control loops (string processing overhead)
 */
std::string GHAND_API ToString(ErrorCode error);

enum class JointId : uint8_t {
  THUMB_DIP,
  THUMB_PIP,
  THUMB_MCP,
  THUMB_SWING,
  THUMB_ROTATION,
  FF_DIP,
  FF_PIP,
  FF_MCP,
  FF_SWING,
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
std::string GHAND_API ToString(JointId id);

// ===== Product Type Definitions =====
enum class ProductType : uint8_t { G5, AUTO };

/**
 * @brief Get string representation of product type (for debug/logging)
 * @warning Do not call in real-time control loops (string processing overhead)
 */
std::string GHAND_API ToString(ProductType type);

// ===== Communication Type Definitions =====
enum class CommType : uint8_t { ETHERCAT, CANFD, RS485 };

// ===== Control Mode Definitions =====
enum class ControlMode : uint8_t { POSITION = 0, TORQUE = 1, SPEED = 2 };

// ===== Device Info Structure =====
struct DeviceInfo {
  std::string device_name;
  std::string hardware_version;
  std::string software_version;
  std::string motor_driver_version;
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
  const char* region_name;                // Region name (provided by device)
  bool state;                             // Sensor state (true=normal, false=abnormal)
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

// ===== HandState / Joint Query Functions =====

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

std::string GHAND_API ToString(const HandState& hs);
std::string GHAND_API ToString(const Joint& joint);

}  // namespace ghand

#endif  // GHAND_TYPES_H_
