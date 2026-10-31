// SPDX-FileCopyrightText: 2025-2026 GLITech
// SPDX-License-Identifier: GPL-3.0-only

#ifndef INCLUDE_GHAND_TYPES_DATA_H_
#define INCLUDE_GHAND_TYPES_DATA_H_

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "ghand/types/enums.h"
#include "ghand/types/errors.h"

namespace ghand {

// Device info structure
struct DeviceInfo {
  std::string device_name = "N/A";
  std::string software_version = "N/A";  // Main controller firmware version
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
struct HandData {
  HandControlState joints_state =
      HandControlState::DISABLED;  // Overall joint enable state.
  HandWarning error_warn;          // Warning flags; see HandWarning.
  HandFault error_fault;           // Fault flags; see HandFault.
  HandCriticalError
      error_critical;       // Critical error flags; see HandCriticalError.
  int16_t temperature = 0;  // Back-of-hand temperature in degrees Celsius.
};

// One finger's tactile data, with forces in Newtons.
struct TactileData {
  bool state = false;  // Reported tactile sensor status.
  // Sensor X/Y/Z forces [Fx, Fy, Fz] (N); empty when unavailable.
  std::vector<float> resultant_force;
  // Per-point [Fx0, Fy0, Fz0, Fx1, Fy1, Fz1, ...] (N); empty when unavailable.
  // Point counts are defined by tactile_regions in the product JSON under
  // config/.
  std::vector<float> distributed_force;
};

// Joint command structure for parameterized joint control
struct JointCommand {
  JointCommand() = default;
  JointCommand(JointId joint_id,
               JointControlMode target_mode = JointControlMode::POSITION,
               JointLoopType target_loop = JointLoopType::CLOSED_LOOP,
               JointControlState target_state = JointControlState::RUNNING,
               float target_angle = 0.0f, int16_t target_velocity = 0,
               int16_t target_current = 0, int16_t target_kp = 0,
               int16_t target_kd = 0)
      : id(joint_id),
        control_mode(target_mode),
        loop_type(target_loop),
        control_state(target_state),
        angle(target_angle),
        velocity(target_velocity),
        current(target_current),
        kp(target_kp),
        kd(target_kd) {}
  JointId id = JointId::THUMB_IP;  // Target joint; see JointId for numeric IDs
                                   // and anatomy.
  JointControlMode control_mode =
      JointControlMode::POSITION;  // Per-joint control mode.
  JointLoopType loop_type =
      JointLoopType::CLOSED_LOOP;  // Selectable in hand CUSTOM mode only.
  JointControlState control_state =
      JointControlState::RUNNING;  // Disable, run, or stop and hold.
  float angle = 0.0f;  // Target angle (deg); limits are in the product JSON
                       // under config/.
  int16_t velocity =
      0;  // Target velocity (-100~100%); velocity limit in POSITION mode.
  int16_t current =
      0;  // Target current (-100~100%); current limit in POSITION mode.
  int16_t kp = 0;  // MIT proportional gain
  int16_t kd = 0;  // MIT derivative gain
};

// Single joint feedback received from the device.
struct JointData {
  JointId id = JointId::THUMB_IP;  // Joint identifier; see JointId for numeric
                                   // IDs and anatomy.
  JointState state = JointState::DISABLED;  // Reported joint operating state.
  JointErrorCode error = JointErrorCode::NORMAL;  // Reported joint error code.
  JointControlMode control_mode =
      JointControlMode::POSITION;  // Reported control mode.
  JointLoopType loop_type =
      JointLoopType::CLOSED_LOOP;  // Reported feedback loop type.
  float angle = 0.0f;              // Measured angle (deg).
  int16_t velocity = 0;            // Reported velocity (%).
  int16_t current = 0;             // Reported motor current (%).
};

// Empty tactile data and a null timestamp indicate unavailable values.
struct DeviceData {
  HandData hand;  // Overall hand state, errors and temperature.
  std::vector<JointData>
      joints;  // Joint feedback; each entry identifies its joint by ID.
  std::map<TactileSensorId, TactileData>
      tactile;  // Tactile readings keyed by finger sensor ID.
  std::shared_ptr<const double>
      timestamp;  // Timestamp associated with this update; null if unavailable.
};

}  // namespace ghand

#endif  // INCLUDE_GHAND_TYPES_DATA_H_
